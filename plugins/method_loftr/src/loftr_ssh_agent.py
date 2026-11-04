"""Versioned LoFTR SSH protocol. Executed only by the selected remote Python."""
import contextlib
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import stat
import subprocess
import sys
import tarfile
import time

PROTOCOL = 'posdk.loftr.ssh.v1'
MAX_HEADER = 64 * 1024 * 1024
MAX_IMAGE = 1024 * 1024 * 1024
PINNED_WEIGHTS = {
    'outdoor': '21f5bec5968178e8bc8b7633441836fe5de4f47d861dd2cd7dc38e271b0479ec',
    'indoor': 'd73c54720370ba0690cd477e38404a626e881292cc0be09a6801da0cc6f53198',
}


def digest(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def emit(event, **values):
    print(json.dumps(dict(protocol=PROTOCOL, event=event, **values)), flush=True)


def private_directory(path):
    path.mkdir(mode=0o700, parents=True, exist_ok=True)
    info = path.lstat()
    if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid() or stat.S_IMODE(info.st_mode) & 0o077:
        raise ValueError('SSH workspace must be a real, owner-only directory: ' + str(path))
    return path


def workspace(base, job_id):
    if not re.fullmatch(r'[0-9a-f]{32}', job_id):
        raise ValueError('Invalid SSH job identity')
    base = Path(base).expanduser()
    if not base.is_absolute() or base == Path('/') or '\n' in str(base) or '\r' in str(base):
        raise ValueError('SSH working directory must be an absolute path or ~/ path')
    root = private_directory(base / 'posdk-ssh-v1')
    jobs = private_directory(root / 'jobs')
    return root, jobs / job_id


def runtime_identity():
    import platform
    import torch
    import torchvision
    import kornia
    import cv2
    import numpy
    versions = dict(python=platform.python_version(), torch=torch.__version__,
                    torchvision=torchvision.__version__, kornia=kornia.__version__,
                    opencv=cv2.__version__, numpy=numpy.__version__)
    expected = dict(python='3.11.11', torch='2.2.2', torchvision='0.17.2',
                    kornia='0.7.4', opencv='4.10.0', numpy='1.26.4')
    if any(versions[key].split('+')[0] != value for key, value in expected.items()):
        raise ValueError('Unsupported remote LoFTR environment: ' + json.dumps(versions))
    return versions


def checked_inventory(entries):
    if not isinstance(entries, list) or len(entries) > 1004:
        raise ValueError('Invalid upload inventory')
    inventory = {}
    for entry in entries:
        name = entry.get('name', '')
        if not (re.fullmatch(r'images/[0-9a-f]{64}\.img', name)
                or name in ('loftr_batch_worker.py', 'loftr_aggregation.py', 'loftr_rotation.py', 'checkpoint.ckpt')):
            raise ValueError('Invalid upload name')
        if name in inventory or type(entry.get('size')) is not int or not 0 < entry['size'] <= MAX_IMAGE:
            raise ValueError('Invalid upload size or duplicate name')
        if not re.fullmatch(r'[0-9a-f]{64}', entry.get('sha256', '')):
            raise ValueError('Invalid upload hash')
        inventory[name] = entry
    if sum(row['size'] for row in inventory.values()) > 32 * 1024 ** 3:
        raise ValueError('SSH input exceeds 32 GiB')
    return inventory


def receive_files(stream, directory, inventory):
    seen = set()
    with tarfile.open(fileobj=stream, mode='r|') as archive:
        for member in archive:
            entry = inventory.get(member.name)
            if not entry or member.name in seen or not member.isfile() or member.size != entry['size']:
                raise ValueError('Unexpected SSH upload member')
            destination = directory / member.name
            destination.parent.mkdir(mode=0o700, parents=True, exist_ok=True)
            checksum = hashlib.sha256()
            source = archive.extractfile(member)
            with destination.open('xb') as output:
                remaining = member.size
                while remaining:
                    chunk = source.read(min(1024 * 1024, remaining))
                    if not chunk:
                        raise ValueError('Truncated SSH upload')
                    output.write(chunk); checksum.update(chunk); remaining -= len(chunk)
            destination.chmod(0o600)
            if checksum.hexdigest() != entry['sha256']:
                raise ValueError('SSH upload checksum mismatch')
            seen.add(member.name)
    if seen != set(inventory):
        raise ValueError('Incomplete SSH upload')


def prepare(root, job, config):
    versions = runtime_identity()
    inventory = checked_inventory(config['files'])
    images = config['images']
    if not isinstance(images, list) or not 2 <= len(images) <= 1000 or any(name not in inventory or not name.startswith('images/') for name in images):
        raise ValueError('Invalid SSH image list')
    if not {'loftr_batch_worker.py', 'loftr_aggregation.py', 'loftr_rotation.py'} <= set(inventory):
        raise ValueError('Missing signed worker resources')
    checkpoint = ''
    if 'checkpoint.ckpt' not in inventory:
        supplied = config.get('remote_checkpoint', '')
        checkpoint = Path(supplied).expanduser() if supplied else Path(sys.executable).resolve().parent.parent / 'runtime/weights' / ('loftr_' + config['options']['model_type'] + '.ckpt')
        if not checkpoint.is_absolute() or not checkpoint.is_file():
            raise ValueError('Remote checkpoint is unavailable; configure ssh_checkpoint_path')
        checksum = digest(checkpoint)
        if not supplied and checksum != PINNED_WEIGHTS[config['options']['model_type']]:
            raise ValueError('Managed remote checkpoint checksum mismatch')
    if type(config.get('result_limit')) is not int or not 1024 * 1024 <= config['result_limit'] <= 64 * 1024 ** 3:
        raise ValueError('Invalid result size limit')
    job.mkdir(mode=0o700)  # Exclusive: never adopt an existing job or symlink.
    config['checkpoint'] = str(checkpoint)
    (job / 'job.json').write_text(json.dumps(config), encoding='utf8')
    (job / 'job.json').chmod(0o600)
    emit('loftr_ssh_prepared', job_id=job.name, versions=versions, image_count=len(images))


def result_inventory(output, count, maximum):
    names = [f'features_{i}.txt' for i in range(count)]
    names += [f'pair_{i}_{j}.txt' for i in range(count) for j in range(i + 1, count)]
    names += [name for name in ('image_geometry.json', 'association.json', 'rotation_provenance.json') if (output / name).exists()]
    result = []; total = 0
    for name in names:
        path = output / name
        info = path.lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_size > maximum:
            raise ValueError('Invalid remote result file')
        total += info.st_size
        if total > maximum:
            raise ValueError('Remote results exceed ssh_result_limit_mb')
        result.append(dict(name=name, size=info.st_size, sha256=digest(path)))
    return result


def worker_command(job, config):
    args = [sys.executable, '-u', str(job / 'loftr_batch_worker.py'), '--input', 'images.txt', '--output-dir', str(job / 'output')]
    for key, value in config['options'].items():
        if key not in ('model_type', 'max_keypoints', 'max_keypoints_per_image', 'keypoint_merge_radius', 'keypoint_cell_size',
                       'confidence_threshold', 'resize_max', 'refine_endpoint', 'device', 'pair_geometry', 'geometry_threshold', 'cpu_threads'):
            raise ValueError('Unknown matching option')
        args += ['--' + key.replace('_', '-'), str(value)]
    checkpoint = job / 'checkpoint.ckpt' if 'checkpoint.ckpt' in checked_inventory(config['files']) else Path(config['checkpoint'])
    args += ['--checkpoint', str(checkpoint)]
    for key in ('pair_cache_dir', 'base_pair_cache_dir'):
        if config.get(key):
            path = Path(config[key]).expanduser()
            if not path.is_absolute() or path == Path('/') or '\n' in str(path) or '\r' in str(path):
                raise ValueError('SSH cache paths must be absolute or ~/ paths')
            args += ['--' + key.replace('_', '-'), str(path)]
    if config.get('rotation_rescue'):
        args.append('--rotation-rescue')
    return args


def run_worker(root, job, config):
    import fcntl
    images = config['images']
    (job / 'images.txt').write_text(str(len(images)) + '\n' + '\n'.join(images) + '\n', encoding='utf8')
    args = worker_command(job, config)
    with contextlib.ExitStack() as stack:
        # Shared remote caches are serialized without changing their contents.
        lock_dir = private_directory(root / 'locks')
        for path in sorted(set(str(Path(config[key]).expanduser().resolve()) for key in ('pair_cache_dir', 'base_pair_cache_dir') if config.get(key))):
            lock = stack.enter_context((lock_dir / (hashlib.sha256(path.encode()).hexdigest() + '.lock')).open('a'))
            deadline = time.monotonic() + config['run_timeout']
            announced = False
            while True:
                try:
                    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB); break
                except BlockingIOError:
                    if time.monotonic() >= deadline:
                        raise TimeoutError('Remote LoFTR cache is busy')
                    if not announced:
                        emit('loftr_ssh_waiting_for_cache'); announced = True
                    time.sleep(.5)
        process = subprocess.Popen(args, cwd=job, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True)
        (job / 'worker.pid').write_text(str(process.pid))
        finished = None
        try:
            while True:
                line = process.stdout.readline(1024 * 1024 + 1)
                if not line:
                    break
                if len(line) > 1024 * 1024:
                    raise ValueError('Remote worker log line is too large')
                sys.stdout.buffer.write(line); sys.stdout.buffer.flush()
                try:
                    event = json.loads(line)
                    if event.get('event') == 'loftr_finished':
                        finished = event
                except (ValueError, AttributeError):
                    pass
            if process.wait() != 0 or not finished or finished.get('pairs') != len(images) * (len(images) - 1) // 2:
                raise RuntimeError('Remote LoFTR did not finish the declared image pairs')
        finally:
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL); process.wait()
            process.stdout.close()
            (job / 'worker.pid').unlink(missing_ok=True)
    entries = result_inventory(job / 'output', len(images), config['result_limit'])
    (job / 'results.json').write_text(json.dumps(dict(protocol=PROTOCOL, job_id=job.name, image_count=len(images), files=entries)))
    emit('loftr_ssh_finished', job_id=job.name, files=len(entries), bytes=sum(row['size'] for row in entries))


def main():
    os.umask(0o077)
    operation, base, job_id = sys.argv[1:]
    root, job = workspace(base, job_id)
    if operation == 'prepare':
        raw = sys.stdin.buffer.readline(MAX_HEADER + 1)
        if len(raw) > MAX_HEADER:
            raise ValueError('SSH request is too large')
        prepare(root, job, json.loads(raw)); return
    info = job.lstat()
    if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid() or stat.S_IMODE(info.st_mode) & 0o077:
        raise ValueError('Invalid SSH job directory')
    config = json.loads((job / 'job.json').read_text())
    if operation == 'upload':
        receive_files(sys.stdin.buffer, job, checked_inventory(config['files']))
        emit('loftr_ssh_uploaded', job_id=job.name)
    elif operation == 'run':
        def interrupted(_signal, _frame):
            raise InterruptedError('Remote LoFTR cancelled or timed out')
        for name in (signal.SIGTERM, signal.SIGHUP, signal.SIGINT, signal.SIGALRM):
            signal.signal(name, interrupted)
        signal.alarm(config['run_timeout'])
        try:
            run_worker(root, job, config)
        finally:
            signal.alarm(0)
    elif operation == 'download':
        manifest = json.loads((job / 'results.json').read_text())
        sys.stdout.buffer.write(json.dumps(manifest).encode() + b'\n'); sys.stdout.buffer.flush()
        for row in manifest['files']:
            with (job / 'output' / row['name']).open('rb') as source:
                for chunk in iter(lambda: source.read(1024 * 1024), b''):
                    sys.stdout.buffer.write(chunk)
        sys.stdout.buffer.flush()
    elif operation == 'cancel':
        pid_path = job / 'worker.pid'
        if pid_path.exists():
            pid = int(pid_path.read_text())
            command = subprocess.run(['ps', '-p', str(pid), '-o', 'command='], capture_output=True, text=True).stdout
            if str(job) in command and os.getpgid(pid) == pid:
                os.killpg(pid, signal.SIGTERM)
        emit('loftr_ssh_cancelled', job_id=job.name)
    elif operation == 'cleanup':
        if (job / 'worker.pid').exists():
            raise ValueError('Refusing to remove a running SSH job')
        import shutil
        shutil.rmtree(job)
        emit('loftr_ssh_cleaned', job_id=job.name)
    else:
        raise ValueError('Unknown SSH operation')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(json.dumps(dict(protocol=PROTOCOL, event='loftr_ssh_error', message=str(error))), file=sys.stderr, flush=True)
        raise SystemExit(1)
