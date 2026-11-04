#!/usr/bin/env python3
"""RoMa v2 worker and stdlib-only SSH controller. No local model fallback."""
import argparse
import hashlib
import importlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tarfile
import tempfile
import time
import uuid

MAX_BYTES = 4 * 1024**3
PROTOCOL = 'posdk.romav2.ssh.v1'


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(8 * 1024**2), b''):
            h.update(block)
    return h.hexdigest()


def emit(event, **values):
    print(json.dumps(dict(protocol=PROTOCOL, event=event, **values)), flush=True)


def checked_extract(stream, root, entries):
    """Never extract archive paths, links, undeclared members or unbounded data."""
    seen = set()
    total = 0
    with tarfile.open(fileobj=stream, mode='r|') as archive:
        for item in archive:
            name = item.name
            if (name not in entries or name in seen or not item.isfile()
                    or Path(name).is_absolute() or '..' in Path(name).parts):
                raise ValueError('Invalid archive member ' + name)
            expected = entries[name]
            total += item.size
            if item.size != expected['size'] or total > MAX_BYTES:
                raise ValueError('Archive size mismatch')
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open('xb') as dest:
                shutil.copyfileobj(archive.extractfile(item), dest, 1024**2)
            if digest(target) != expected['sha256']:
                raise ValueError('Archive SHA-256 mismatch: ' + name)
            seen.add(name)
    if seen != set(entries):
        raise ValueError('Incomplete archive')


def inventory(root, names):
    result = {}
    for name in names:
        p = root / name
        if p.is_symlink() or not p.is_file():
            raise ValueError('Missing/linked result: ' + name)
        result[name] = dict(size=p.stat().st_size, sha256=digest(p))
    if sum(x['size'] for x in result.values()) > MAX_BYTES:
        raise ValueError('Result exceeds transfer limit')
    return result


def reuse_images(root, cfg):
    source_root = Path(cfg['remote_image_root']).expanduser().resolve(strict=True)
    if not source_root.is_dir():
        raise ValueError('Remote image root is not a directory')
    for name, record in cfg['image_inventory'].items():
        if re.fullmatch(r'images/[0-9]{5}\.[A-Za-z0-9]+', name) is None:
            raise ValueError('Invalid image destination')
        basename = record['basename']
        if Path(basename).name != basename or basename in ('', '.', '..'):
            raise ValueError('Invalid remote image name')
        source = source_root / basename
        if source.is_symlink() or not source.is_file():
            raise ValueError('Missing/linked remote image')
        if source.stat().st_size != record['size'] or digest(source) != record['sha256']:
            raise ValueError('Remote image differs from local input: ' + basename)
        target = root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        with source.open('rb') as src, target.open('xb') as dst:
            shutil.copyfileobj(src, dst, 1024**2)
        if digest(target) != record['sha256']:
            raise ValueError('Remote image changed while copying')


def serve(operation, base, job_id):
    if re.fullmatch(r'[0-9a-f]{32}', job_id) is None:
        raise ValueError('Invalid job identity')
    base = Path(base).expanduser()
    if not base.is_absolute() or base.is_symlink():
        raise ValueError('Invalid remote root')
    base.mkdir(mode=0o700, parents=True, exist_ok=True)
    # Refuse a public or shared job root instead of chmod-ing unrelated paths.
    if base.stat().st_uid != os.getuid() or base.stat().st_mode & 0o077:
        raise ValueError('Remote job root must be private to its owner')
    root = base / job_id
    if operation == 'prepare':
        root.mkdir(mode=0o700)
        raw = sys.stdin.buffer.readline(2 * 1024**2)
        request = json.loads(raw)
        if request['protocol'] != PROTOCOL:
            raise ValueError('Wrong protocol')
        allowed = {'romav2_worker.py', 'romav2_aggregation.py',
                   'romav2-runtime-lock.json', 'config.json'}
        for name in request['files']:
            if name not in allowed and re.fullmatch(r'images/[0-9]{5}\.[A-Za-z0-9]+', name) is None:
                raise ValueError('Unexpected input')
        checked_extract(sys.stdin.buffer, root, request['files'])
        cfg = json.loads((root / 'config.json').read_text())
        if cfg.get('remote_image_root'):
            reuse_images(root, cfg)
        emit('prepared', job_id=job_id, reused_images=len(cfg['images']) if cfg.get('remote_image_root') else 0)
        return
    if operation in ('cancel', 'cleanup') and not root.exists():
        return
    if not root.is_dir() or root.is_symlink():
        raise ValueError('Job does not exist')
    if operation == 'cancel':
        pid = root / 'worker.pid'
        if pid.exists():
            os.killpg(int(pid.read_text()), signal.SIGTERM)
        return
    if operation == 'cleanup':
        if (root / 'worker.pid').exists():
            raise ValueError('Cannot remove a running job')
        shutil.rmtree(root)
        return
    if operation == 'run':
        cfg = json.loads((root / 'config.json').read_text())
        command = [sys.executable, '-u', str(root / 'romav2_worker.py'),
                   'infer', '--config', str(root / 'config.json')]
        with (root / 'worker.log').open('wb') as log:
            child = subprocess.Popen(command, cwd=root, stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, start_new_session=True)
            (root / 'worker.pid').write_text(str(child.pid))
            def expired(*_):
                os.killpg(child.pid, signal.SIGTERM)
                raise TimeoutError('Remote worker timed out')
            signal.signal(signal.SIGALRM, expired)
            signal.alarm(cfg['timeout_seconds'])
            try:
                for line in child.stdout:
                    if len(line) > 1024**2:
                        raise ValueError('Oversized worker event')
                    log.write(line)
                    sys.stdout.buffer.write(line)
                    sys.stdout.buffer.flush()
                if child.wait() != 0:
                    raise RuntimeError('Remote inference failed')
            finally:
                signal.alarm(0)
                if child.poll() is None:
                    os.killpg(child.pid, signal.SIGTERM)
                    try:
                        child.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        os.killpg(child.pid, signal.SIGKILL)
                        child.wait()
                (root / 'worker.pid').unlink(missing_ok=True)
        count = len(cfg['images'])
        names = ['output/inference.json', 'output/association.json', 'worker.log']
        names += [f'output/features_{i}.txt' for i in range(count)]
        names += [f'output/pair_{i}_{j}.txt' for i in range(count) for j in range(i+1, count)]
        record = dict(protocol=PROTOCOL, job_id=job_id, files=inventory(root, names))
        (root / 'results.json').write_text(json.dumps(record))
        emit('remote_completed', job_id=job_id, files=len(names))
    elif operation == 'fetch':
        record = json.loads((root / 'results.json').read_text())
        sys.stdout.buffer.write(json.dumps(record).encode() + b'\n')
        sys.stdout.buffer.flush()
        with tarfile.open(fileobj=sys.stdout.buffer, mode='w|') as archive:
            for name in record['files']:
                archive.add(root / name, arcname=name, recursive=False)
    else:
        raise ValueError('Invalid operation')


def controller(args):
    images = Path(args.input).read_text().splitlines()
    count = int(images.pop(0))
    if not 2 <= count <= 200 or count != len(images):
        raise ValueError('Invalid image list')
    if re.fullmatch(r'[A-Za-z0-9_][A-Za-z0-9_.@-]*', args.ssh_target) is None:
        raise ValueError('Invalid SSH target')
    if args.setting not in ('turbo', 'fast', 'base', 'precise'):
        raise ValueError('Invalid model setting')
    if not 64 <= args.num_matches <= 10000 or not .1 <= args.keypoint_merge_radius <= 10:
        raise ValueError('Invalid sample/aggregation limits')
    if not 0 <= args.confidence_threshold <= 1 or not 1 <= args.cpu_threads <= 32:
        raise ValueError('Invalid confidence/CPU limits')
    if not 30 <= args.timeout_seconds <= 86400:
        raise ValueError('Invalid timeout')
    here = Path(__file__).resolve().parent
    aggregation = here / 'romav2_aggregation.py'
    lock = here / 'romav2-runtime-lock.json'
    # Developer source invocation uses the same canonical resources as CMake.
    if not aggregation.exists():
        aggregation = here.parent / 'LoFTR/loftr_aggregation.py'
    if not lock.exists():
        lock = here / 'deps/runtime-lock.json'
    job_id = uuid.uuid4().hex
    ssh = ['/usr/bin/ssh', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes',
           '-o', 'ConnectTimeout=15', '-o', 'ServerAliveInterval=15',
           '-o', 'ServerAliveCountMax=3', '-o', 'ClearAllForwardings=yes', args.ssh_target]
    # The remote interpreter expands ~/ itself; no untrusted shell expansion.
    launcher = ('import os,sys; p=os.path.expanduser(sys.argv[1]); '
                'os.execv(p,[p,"-c",sys.argv[2],*sys.argv[3:]])')
    code = Path(__file__).read_text()
    def command(op):
        return ssh + [shlex.join(['python3', '-c', launcher,
                                 args.ssh_python_executable, code,
                                 'serve', op, args.ssh_work_dir, job_id])]
    for sig in (signal.SIGTERM, signal.SIGINT):
        signal.signal(sig, lambda *_: (_ for _ in ()).throw(KeyboardInterrupt()))
    prepared = False
    completed = False
    with tempfile.TemporaryDirectory(prefix='posdk-romav2-ssh-') as local:
        root = Path(local)
        files = {}
        resources = {'romav2_worker.py': Path(__file__),
                     'romav2_aggregation.py': aggregation, 'romav2-runtime-lock.json': lock}
        for i, path in enumerate(images):
            p = Path(path).resolve(strict=True)
            resources[f'images/{i:05d}{p.suffix.lower()}'] = p
        image_names = [name for name in resources if name.startswith('images/')]
        image_inventory = {name: dict(basename=resources[name].name,
            size=resources[name].stat().st_size, sha256=digest(resources[name])) for name in image_names}
        cfg = dict(vars(args), images=image_names, image_inventory=image_inventory)
        if args.remote_image_root:
            resources = {name: path for name, path in resources.items() if not name.startswith('images/')}
        (root / 'config.json').write_text(json.dumps(cfg))
        resources['config.json'] = root / 'config.json'
        for name, path in resources.items():
            files[name] = dict(size=path.stat().st_size, sha256=digest(path))
        if sum(f['size'] for f in files.values()) > MAX_BYTES:
            raise ValueError('Input exceeds transfer limit')
        try:
            emit('uploading', job_id=job_id, images=count,
                 bytes=sum(f['size'] for f in files.values()))
            prepared = True  # Cleanup our UUID even if preparation/transfer fails.
            with tempfile.TemporaryFile() as payload:
                payload.write(json.dumps(dict(protocol=PROTOCOL, files=files)).encode()+b'\n')
                with tarfile.open(fileobj=payload, mode='w|') as archive:
                    for name, path in resources.items():
                        archive.add(path, arcname=name, recursive=False)
                payload.seek(0)
                subprocess.run(command('prepare'), stdin=payload, check=True,
                               timeout=args.timeout_seconds)
            emit('remote_running', job_id=job_id, target=args.ssh_target)
            subprocess.run(command('run'), check=True, timeout=args.timeout_seconds+60)
            fetch = subprocess.Popen(command('fetch'), stdout=subprocess.PIPE)
            try:
                record = json.loads(fetch.stdout.readline(2*1024**2))
                if record['protocol'] != PROTOCOL or record['job_id'] != job_id:
                    raise ValueError('Wrong result identity')
                expected = {'output/inference.json', 'output/association.json', 'worker.log'}
                expected.update(f'output/features_{i}.txt' for i in range(count))
                expected.update(f'output/pair_{i}_{j}.txt' for i in range(count) for j in range(i+1, count))
                if set(record['files']) != expected:
                    raise ValueError('Incomplete output inventory')
                checked_extract(fetch.stdout, root / 'result', record['files'])
                if fetch.wait(timeout=30) != 0:
                    raise RuntimeError('SSH download failed')
            finally:
                if fetch.poll() is None:
                    fetch.terminate()
                    fetch.wait(timeout=20)
            dest = Path(args.output_dir)
            dest.mkdir(parents=True, exist_ok=True)
            for p in (root / 'result/output').iterdir():
                shutil.copyfile(p, dest / p.name)
            shutil.copyfile(root / 'result/worker.log', dest / 'worker.log')
            (dest / 'ssh-receipt.json').write_text(json.dumps(record, indent=2)+'\n')
            completed = True
            emit('download_verified', job_id=job_id, target=args.ssh_target,
                 files=len(record['files']))
        finally:
            if prepared:
                if not completed:
                    subprocess.run(command('cancel'), timeout=20, check=False)
                subprocess.run(command('cleanup'), timeout=20, check=False)


def load_model(runtime, lock, setting, device):
    import torch
    from dataclasses import replace
    from unittest.mock import patch
    for project, spec in lock['sources'].items():
        for name, expected in spec['files'].items():
            if digest(runtime / 'vendor' / project / name) != expected:
                raise ValueError('Upstream source hash mismatch: ' + name)
    weights = runtime / 'assets/romav2.0.1.pt'
    if digest(weights) != lock['model_sha256']:
        raise ValueError('Model hash mismatch')
    sys.path[:0] = [str(runtime / 'vendor/RoMaV2/src'), str(runtime / 'vendor/DINOv3')]
    for name in ('device', 'geometry', 'features', 'matcher', 'refiner', 'romav2'):
        importlib.import_module('romav2.' + name).device = torch.device(device)
    from romav2 import RoMaV2
    from romav2.refiner import Block
    from dinov3.hub.backbones import dinov3_vitl16
    cfg = RoMaV2.Cfg(setting=setting, compile=False)
    cfg = replace(cfg, descriptor=replace(cfg.descriptor, enable_amp=False),
                  matcher=replace(cfg.matcher, enable_amp=False))
    def local_backbone(*args, **kwargs):
        if kwargs.get('model') != 'dinov3_vitl16':
            raise ValueError('Unexpected architecture lookup')
        return dinov3_vitl16(pretrained=False)
    def local_weights(url, *args, **kwargs):
        if url != 'https://github.com/Parskatt/RoMaV2/releases/download/v2.0.1/romav2.0.1.pt':
            raise ValueError('Unexpected model download')
        return torch.load(weights, map_location='cpu', weights_only=True)
    with patch('torch.hub.load', local_backbone), patch('torch.hub.load_state_dict_from_url', local_weights):
        model = RoMaV2(cfg)
    for module in model.modules():
        if isinstance(module, Block):
            module.enable_amp = False
    return model.requires_grad_(False).to(device).eval()


def infer(config):
    import numpy as np
    import torch
    import torchvision
    import PIL
    import einops
    import cv2
    from importlib.metadata import version as distribution_version
    from PIL import Image
    from romav2_aggregation import aggregate
    root = Path(config).resolve().parent
    cfg = json.loads(Path(config).read_text())
    lock = json.loads((root / 'romav2-runtime-lock.json').read_text())
    versions = dict(python='.'.join(map(str, sys.version_info[:3])),
                    torch=torch.__version__, torchvision=torchvision.__version__,
                    numpy=np.__version__, pillow=PIL.__version__, einops=einops.__version__,
                    opencv=distribution_version("opencv-python-headless"))
    if versions != lock['runtime']:
        raise ValueError('Remote runtime versions differ: ' + str(versions))
    device = cfg['device']
    if device == 'mps' and not torch.backends.mps.is_available():
        raise ValueError('MPS requested but unavailable')
    if device not in ('mps', 'cpu', 'cuda'):
        raise ValueError('Invalid device')
    torch.set_num_threads(cfg['cpu_threads'])
    torch.set_float32_matmul_precision('highest')
    start = time.monotonic()
    model = load_model(Path(cfg['runtime_root']).expanduser(), lock, cfg['setting'], device)
    loaded = time.monotonic()
    out = root / 'output'
    out.mkdir()
    images = [root / x for x in cfg['images']]
    metadata = []
    for p in images:
        with Image.open(p) as im:
            metadata.append(dict(width=im.width, height=im.height, sha256=digest(p)))
    count = len(images)
    times = []
    for i in range(count):
        for j in range(i+1, count):
            torch.manual_seed(cfg['seed'] + i * 1009 + j)
            tick = time.monotonic()
            with torch.inference_mode():
                predictions = model.match(str(images[i]), str(images[j]))
                matches, confidence, _, _ = model.sample(predictions, cfg['num_matches'])
                a, b = model.to_pixel_coordinates(matches,
                    metadata[i]['height'], metadata[i]['width'],
                    metadata[j]['height'], metadata[j]['width'])
                # RoMa uses edge coordinates; features use zero-index centers.
                a, b = a.float().cpu().numpy() - .5, b.float().cpu().numpy() - .5
                confidence = confidence.float().cpu().numpy()
            valid = np.isfinite(a).all(1) & np.isfinite(b).all(1) & np.isfinite(confidence)
            for xy, info in ((a, metadata[i]), (b, metadata[j])):
                valid &= (xy[:,0] >= 0) & (xy[:,0] <= info['width']-1)
                valid &= (xy[:,1] >= 0) & (xy[:,1] <= info['height']-1)
            np.savez(out / f'pair_{i}_{j}.npz', keypoints0=a[valid],
                     keypoints1=b[valid], confidence=confidence[valid])
            times.append(time.monotonic()-tick)
            emit('pair_completed', i=i, j=j, pairs_done=len(times),
                 pairs_total=count*(count-1)//2, matches=int(valid.sum()), seconds=times[-1])
            del predictions, matches
    assoc = aggregate(out, count, cfg['confidence_threshold'],
                      radius=cfg['keypoint_merge_radius'], pair_geometry='none')
    receipt = dict(protocol=PROTOCOL, images=metadata, device=device,
                   setting=cfg['setting'], precision='float32', versions=versions,
                   model_sha256=lock['model_sha256'], sources=lock['sources'],
                   image_count=count, pair_count=len(times), model_loads=1,
                   load_seconds=loaded-start, inference_seconds=sum(times),
                   worker_seconds=time.monotonic()-start, association=assoc,
                   official_balanced_sampling=True, fine_tuned_adapter=False)
    (out / 'inference.json').write_text(json.dumps(receipt, indent=2)+'\n')
    emit('inference_completed', image_count=count, pair_count=len(times), device=device,
         seconds=receipt['worker_seconds'], matches=assoc['associated_matches'])


def parser():
    p = argparse.ArgumentParser()
    p.add_argument('--input', required=True)
    p.add_argument('--output-dir', required=True)
    for name, default in [('ssh-target','remoteMAC'), ('ssh-python-executable','python3'),
                          ('ssh-work-dir','~/.cache/PoSDK/RoMaV2'), ('runtime-root',''),
                          ('setting','fast'), ('device','mps'), ('remote-image-root','')]:
        p.add_argument('--'+name, default=default)
    for name, default in [('num-matches',5000), ('cpu-threads',4), ('seed',31), ('timeout-seconds',7200)]:
        p.add_argument('--'+name, type=int, default=default)
    p.add_argument('--confidence-threshold', type=float, default=.2)
    p.add_argument('--keypoint-merge-radius', type=float, default=1.)
    return p


if __name__ == '__main__':
    try:
        if len(sys.argv) > 1 and sys.argv[1] == 'serve':
            serve(*sys.argv[2:])
        elif len(sys.argv) > 1 and sys.argv[1] == 'infer':
            infer(sys.argv[3])
        else:
            controller(parser().parse_args())
    except subprocess.CalledProcessError as error:
        emit('failed', error_type='ChildProcessFailed', returncode=error.returncode,
             message='Remote transport or model process failed; no partial results accepted')
        raise SystemExit(1)
    except Exception as error:
        emit('failed', error_type=type(error).__name__, message=str(error)[:1024])
        raise SystemExit(1)
