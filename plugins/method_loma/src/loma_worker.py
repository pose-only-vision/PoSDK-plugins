#!/usr/bin/env python3
"""LoMa-B worker and stdlib-only SSH controller. No local model fallback."""
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
PROTOCOL = 'posdk.loma.ssh.v1'


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
        allowed = {'loma_worker.py',
                   'loma-runtime-lock.json', 'config.json'}
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
        command = [sys.executable, '-u', str(root / 'loma_worker.py'),
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
    if args.setting not in ('b',):
        raise ValueError('Invalid model setting')
    if not 64 <= args.max_keypoints <= 8192 or not 128 <= args.resize_long_edge <= 4096:
        raise ValueError('Invalid keypoint/resize limits')
    if args.device not in ('cpu', 'coreml'):
        raise ValueError('Invalid LoMa backend')
    if not 0 <= args.confidence_threshold <= 1 or not 1 <= args.cpu_threads <= 32:
        raise ValueError('Invalid confidence/CPU limits')
    if not 30 <= args.timeout_seconds <= 86400:
        raise ValueError('Invalid timeout')
    here = Path(__file__).resolve().parent
    lock = here / 'loma-runtime-lock.json'
    # Developer source invocation uses the same canonical resources as CMake.
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
    with tempfile.TemporaryDirectory(prefix='posdk-loma-ssh-') as local:
        root = Path(local)
        files = {}
        resources = {'loma_worker.py': Path(__file__), 'loma-runtime-lock.json': lock}
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


def infer(config):
    import platform
    import numpy as np
    import onnxruntime as ort
    import pycolmap
    import PIL
    from PIL import Image
    root = Path(config).resolve().parent
    cfg = json.loads(Path(config).read_text())
    lock = json.loads((root / 'loma-runtime-lock.json').read_text())
    versions = dict(python=platform.python_version(), numpy=np.__version__,
                    pillow=PIL.__version__, onnxruntime=ort.__version__,
                    pycolmap=pycolmap.__version__)
    if versions != lock['runtime']:
        raise ValueError('Remote runtime versions differ: '+str(versions))
    runtime = Path(cfg['runtime_root']).expanduser().resolve(strict=True)
    sessions = {}
    model_metadata = {}
    start = time.monotonic()
    for item in lock['models']:
        path = runtime / 'models' / item['name']
        if path.is_symlink() or path.stat().st_size != item['size'] or digest(path) != item['sha256']:
            raise ValueError('LoMa ONNX identity mismatch: '+item['role'])
        options = ort.SessionOptions()
        options.intra_op_num_threads = cfg['cpu_threads']
        options.inter_op_num_threads = 1
        options.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
        options.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
        providers = ['CPUExecutionProvider']
        if cfg['device'] == 'coreml' and item['role'] == 'descriptor':
            if 'CoreMLExecutionProvider' not in ort.get_available_providers():
                raise ValueError('Explicit CoreML backend is unavailable')
            cache = runtime / 'coreml-cache' / ('nk'+str(cfg['max_keypoints']))
            cache.mkdir(parents=True, exist_ok=True)
            provider_options = dict(ModelFormat='MLProgram', MLComputeUnits='CPUAndGPU',
                RequireStaticInputShapes='1', EnableOnSubgraphs='0', ProfileComputePlan='0',
                AllowLowPrecisionAccumulationOnGPU='0', ModelCacheDirectory=str(cache))
            options.add_free_dimension_override_by_name('nkpts',cfg['max_keypoints'])
            providers = [('CoreMLExecutionProvider',provider_options),'CPUExecutionProvider']
        emit('model_loading',role=item['role'],device=cfg['device'])
        session = ort.InferenceSession(str(path),sess_options=options,providers=providers)
        session.disable_fallback()
        expected = ['CoreMLExecutionProvider','CPUExecutionProvider'] if isinstance(providers[0],tuple) else ['CPUExecutionProvider']
        if session.get_providers() != expected:
            raise ValueError('Unexpected execution providers')
        sessions[item['role']] = session
        model_metadata[item['role']] = dict(sha256=item['sha256'],providers=session.get_providers())
    loaded = time.monotonic()
    image_input = next(x for x in sessions['descriptor'].get_inputs() if x.name=='image')
    if image_input.shape[2:] != [784,784]:
        raise ValueError('Unexpected descriptor geometry')
    def run(role, feed):
        session=sessions[role]
        if set(feed) != {x.name for x in session.get_inputs()}:
            raise ValueError('Unexpected ONNX input schema')
        return dict(zip([x.name for x in session.get_outputs()],session.run(None,feed)))
    def tensor(bitmap):
        return np.ascontiguousarray(bitmap.to_array().transpose(2,0,1)[None],dtype=np.float32)/np.float32(255.)
    out=root/'output';out.mkdir()
    features=[];metadata=[];extraction_times=[];matching_times=[]
    for index,name in enumerate(cfg['images']):
        tick=time.monotonic();path=root/name
        with Image.open(path) as image:
            rgb=np.array(image.convert('RGB'),dtype=np.uint8,copy=True)
        h,w=rgb.shape[:2];factor=cfg['resize_long_edge']/max(h,w)
        dw,dh=max(1,round(w*factor)),max(1,round(h*factor))
        bitmap=pycolmap.Bitmap.from_array(rgb)
        bitmap.rescale(dw,dh)
        detected=run('detector',dict(image=tensor(bitmap),
            num_keypoints=np.array([cfg['max_keypoints']],dtype=np.int64)))
        points=detected['keypoints'];prob=detected['keypoint_probs'].reshape(-1)
        if points.shape!=(1,cfg['max_keypoints'],2) or len(prob)!=cfg['max_keypoints'] or not np.isfinite(points).all() or not np.isfinite(prob).all():
            raise ValueError('Invalid detector result')
        bitmap.rescale(784,784)
        described=run('descriptor',dict(image=tensor(bitmap),keypoints=points))
        if len(described)!=1:
            raise ValueError('Invalid descriptor outputs')
        desc=next(iter(described.values()))
        if desc.shape!=(1,cfg['max_keypoints'],256) or not np.isfinite(desc).all():
            raise ValueError('Invalid descriptor result')
        pixels=(points[0]+np.float32(1.))*np.array([w,h],dtype=np.float32)/np.float32(2.)-np.float32(.5)
        valid=(prob>=0)&(pixels[:,0]>=0)&(pixels[:,0]<=w-1)&(pixels[:,1]>=0)&(pixels[:,1]<=h-1)
        f=dict(keypoints=np.ascontiguousarray(points[:,valid]),descriptors=np.ascontiguousarray(desc[:,valid]),
               pixels=np.ascontiguousarray(pixels[valid]))
        features.append(f)
        metadata.append(dict(width=w,height=h,sha256=digest(path),features=int(valid.sum())))
        with (out/f'features_{index}.txt').open('w') as stream:
            stream.write(str(len(f['pixels']))+'\n')
            np.savetxt(stream,f['pixels'],fmt='%.9g')
        extraction_times.append(time.monotonic()-tick)
        emit('image_extracted',image=index,features=int(valid.sum()),seconds=extraction_times[-1])
    pair_rows=[];match_count=0
    for i in range(len(features)):
        for j in range(i+1,len(features)):
            tick=time.monotonic();a,b=features[i],features[j]
            if len(a['pixels']) and len(b['pixels']):
                predicted=run('matcher',dict(kpts0=a['keypoints'],kpts1=b['keypoints'],
                    desc0=a['descriptors'],desc1=b['descriptors']))
                m0=predicted['m0'][0];scores=predicted['mscores0'][0]
                if m0.shape!=(len(a['pixels']),) or scores.shape!=m0.shape or not np.isfinite(scores).all():
                    raise ValueError('Invalid matcher result')
                if not np.issubdtype(m0.dtype,np.integer):
                    raise ValueError('Invalid match index dtype')
                left=np.flatnonzero((m0>=0)&(scores>=cfg['confidence_threshold']))
                right=m0[left];score=scores[left]
                if len(set(right.tolist()))!=len(right) or np.any(right>=len(b['pixels'])) or np.any(score>1):
                    raise ValueError('Invalid or duplicate matcher associations')
                pairs=np.column_stack((left,right,score,np.zeros(len(left),dtype=np.int64)))
            else:
                pairs=np.empty((0,4))
            with (out/f'pair_{i}_{j}.txt').open('w') as stream:
                stream.write(str(len(pairs))+'\n')
                np.savetxt(stream,pairs,fmt=['%d','%d','%.9g','%d'])
            matching_times.append(time.monotonic()-tick);match_count+=len(pairs)
            pair_rows.append(dict(i=i,j=j,matches=len(pairs)))
            emit('pair_completed',i=i,j=j,matches=len(pairs),seconds=matching_times[-1])
    assoc=dict(associated_matches=match_count,per_image_features=[len(f['pixels']) for f in features],
               feature_identity='single extraction per image; immutable per-image indices',
               aggregation='none; no coordinate snapping or pairwise re-quantization',
               geometric_verification='none; backend unchanged',pairs=pair_rows)
    (out/'association.json').write_text(json.dumps(assoc,indent=2)+'\n')
    receipt=dict(protocol=PROTOCOL,model='LoMa-B / DaD + DeDoDe-G (official ONNX assets)',
        model_implementation='COLMAP-compatible Bitmap resampling and ONNX I/O adapter',
        coordinate_convention='original unoriented zero-index pixel centers; COLMAP edge minus 0.5',
        setting='b',device=cfg['device'],versions=versions,models=lock['models'],providers=model_metadata,
        images=metadata,image_count=len(features),pair_count=len(pair_rows),model_loads=1,
        load_seconds=loaded-start,extraction_seconds=extraction_times,matching_seconds=matching_times,
        worker_seconds=time.monotonic()-start,association=assoc,fine_tuned_adapter=False,
        coreml_partition_policy='descriptor CoreML CPUAndGPU plus explicit CPU partitions; detector/matcher CPU' if cfg['device']=='coreml' else 'all CPU')
    (out/'inference.json').write_text(json.dumps(receipt,indent=2)+'\n')
    emit('inference_completed',images=len(features),matches=match_count,seconds=receipt['worker_seconds'])

def parser():
    p=argparse.ArgumentParser()
    p.add_argument('--input',required=True);p.add_argument('--output-dir',required=True)
    for name,default in [('ssh-target','remoteMAC'),('ssh-python-executable','python3'),
        ('ssh-work-dir','~/.cache/PoSDK/LoMa'),('runtime-root',''),('setting','b'),
        ('device','coreml'),('remote-image-root','')]:
        p.add_argument('--'+name,default=default)
    for name,default in [('max-keypoints',1280),('resize-long-edge',1024),('cpu-threads',6),
        ('seed',31),('timeout-seconds',7200)]:
        p.add_argument('--'+name,type=int,default=default)
    p.add_argument('--confidence-threshold',type=float,default=.1)
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
