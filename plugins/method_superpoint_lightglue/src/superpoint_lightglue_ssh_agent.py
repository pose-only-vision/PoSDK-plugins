"""Versioned SuperPoint/LightGlue SSH protocol for the selected remote Python."""
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

PROTOCOL = 'posdk.superpoint_lightglue.ssh.v1'
MAX_HEADER = 64 * 1024 * 1024
MAX_IMAGE = 1024 * 1024 * 1024



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
    if platform.python_version() != '3.11.11':
        raise ValueError('Remote Python must be 3.11.11')
    return dict(python=platform.python_version(), system=platform.system(), architecture=platform.machine())


def checked_inventory(entries):
    if not isinstance(entries, list) or len(entries) > 10001:
        raise ValueError('Invalid upload inventory')
    inventory = {}
    for entry in entries:
        name = entry.get('name', '')
        if not (re.fullmatch(r'images/[0-9a-f]{64}\.img', name)
                or name in ('superpoint_lightglue_worker.py',)):
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
    if (config.get('protocol') != PROTOCOL or not isinstance(images, list)
        or not 2 <= len(images) <= 10000 or any(name not in inventory or not name.startswith('images/') for name in images)):
        raise ValueError('Invalid SSH image list')
    if 'superpoint_lightglue_worker.py' not in inventory:
        raise ValueError('Missing signed worker')
    runtime = Path(config['runtime_root']).expanduser() if config['runtime_root'] else Path(sys.prefix).parent / 'runtime'
    if not runtime.is_absolute() or not (runtime / 'identity.json').is_file():
        raise ValueError('Remote managed runtime is unavailable; configure ssh_runtime_folder')
    if type(config.get('result_limit')) is not int or not 1024 ** 2 <= config['result_limit'] <= 64 * 1024 ** 3:
        raise ValueError('Invalid result size limit')
    if type(config.get('run_timeout')) is not int or not 5 <= config['run_timeout'] <= 86400:
        raise ValueError('Invalid runtime timeout')
    job.mkdir(mode=0o700)
    config['runtime_root'] = str(runtime.resolve(strict=True))
    (job / 'job.json').write_text(json.dumps(config), encoding='utf8')
    (job / 'job.json').chmod(0o600)
    emit('sparse_ssh_prepared', job_id=job.name, versions=versions, image_count=len(images))


def result_inventory(output, count, maximum):
    path = output / 'result.bin'
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode) or not 0 < info.st_size <= maximum:
        raise ValueError('Invalid remote result')
    return [dict(name='result.bin', size=info.st_size, sha256=digest(path))]


def worker_command(job, config):
    args = [sys.executable, '-u', str(job / 'superpoint_lightglue_worker.py'),
            '--request', str(job / 'request.bin'), '--output', str(job / 'output/result.bin'),
            '--runtime-root', config['runtime_root']]
    allowed = {'device', 'pair_mode', 'pair_window', 'max_keypoints', 'resize_long_edge',
        'detection_threshold', 'filter_threshold', 'depth_confidence', 'width_confidence',
        'cpu_threads', 'seed', 'nms_radius', 'remove_borders', 'expected_lightglue_commit',
        'expected_lightglue_archive_sha256', 'expected_lightglue_tree_sha256',
        'expected_superpoint_commit', 'expected_superpoint_model_sha256', 'expected_lightglue_model_sha256'}
    if set(config['options']) != allowed:
        raise ValueError('Unknown or missing worker options')
    for key, value in config['options'].items():
        args += ['--' + key.replace('_', '-'), str(value)]
    return args


def run_worker(root, job, config):
    import struct
    images = config['images']
    with (job / 'request.bin').open('xb') as request:
        request.write(b'PSLGREQ1' + struct.pack('<II', 2, len(images)))
        for name in images:
            value = str(job / name).encode('utf8')
            request.write(struct.pack('<I', len(value)) + value)
    (job / 'output').mkdir(mode=0o700)
    process = subprocess.Popen(worker_command(job, config), cwd=job, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, start_new_session=True)
    (job / 'worker.pid').write_text(str(process.pid))
    finished = None
    try:
        while True:
            line = process.stdout.readline(1024 * 1024 + 1)
            if not line:
                break
            if len(line) > 1024 * 1024:
                raise ValueError('Remote worker log line exceeds limit')
            sys.stdout.buffer.write(line); sys.stdout.buffer.flush()
            try:
                event = json.loads(line)
                if event.get('status') == 'ok' and event.get('schema_version') == 2:
                    finished = event
            except (ValueError, AttributeError):
                pass
        count = len(images)
        expected_pairs = count * (count - 1) // 2 if config['options']['pair_mode'] == 'exhaustive' else sum(min(int(config['options']['pair_window']), count-i-1) for i in range(count))
        if process.wait() != 0 or not finished or finished.get('images') != count or finished.get('pairs') != expected_pairs:
            raise RuntimeError('Remote worker did not finish the exact image and pair set')
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
    emit('sparse_ssh_finished', job_id=job.name, files=len(entries), bytes=sum(row['size'] for row in entries))


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
        emit('sparse_ssh_uploaded', job_id=job.name)
    elif operation == 'run':
        def interrupted(_signal, _frame):
            raise InterruptedError('Remote Sparse frontend cancelled or timed out')
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
        emit('sparse_ssh_cancelled', job_id=job.name)
    elif operation == 'cleanup':
        if (job / 'worker.pid').exists():
            raise ValueError('Refusing to remove a running SSH job')
        import shutil
        shutil.rmtree(job)
        emit('sparse_ssh_cleaned', job_id=job.name)
    else:
        raise ValueError('Unknown SSH operation')


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print(json.dumps(dict(protocol=PROTOCOL, event='sparse_ssh_error', message=str(error))), file=sys.stderr, flush=True)
        raise SystemExit(1)
