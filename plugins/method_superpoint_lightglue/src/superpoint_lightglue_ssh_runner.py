#!/usr/bin/env python3
"""Offload Sparse frontend to OpenSSH; this local controller imports no ML packages."""
import argparse
import contextlib
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import signal
import subprocess
import sys
import tarfile
import tempfile
import threading
import uuid

PROTOCOL = 'posdk.superpoint_lightglue.ssh.v1'
MAX_HEADER = 64 * 1024 * 1024
WORKERS = ('superpoint_lightglue_worker.py',)


def emit(stage, **values):
    print(json.dumps(dict(event='sparse_ssh_progress', stage=stage, **values)), flush=True)


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            result.update(chunk)
    return result.hexdigest()


def ssh_arguments(args):
    if not re.fullmatch(r'(?:[A-Za-z0-9_][A-Za-z0-9_.-]*@)?(?:[A-Za-z0-9_][A-Za-z0-9_.:-]*|\[[0-9a-fA-F:]+\])', args.ssh_target):
        raise ValueError('ssh_target must be a host alias, hostname, or user@host')
    if not 0 <= args.ssh_port <= 65535 or not 1 <= args.ssh_connect_timeout <= 120:
        raise ValueError('Invalid SSH port or connection timeout')
    if not 5 <= args.ssh_run_timeout <= 86400 or not 1 <= args.ssh_result_limit_mb <= 65536:
        raise ValueError('Invalid SSH run timeout or result limit')
    command = ['/usr/bin/ssh', '-T', '-o', 'BatchMode=yes', '-o', 'StrictHostKeyChecking=yes',
               '-o', 'ForwardAgent=no', '-o', 'ClearAllForwardings=yes', '-o', 'RemoteCommand=none',
               '-o', 'ServerAliveInterval=15', '-o', 'ServerAliveCountMax=2',
               '-o', 'ConnectTimeout=' + str(args.ssh_connect_timeout)]
    if args.ssh_port:
        command += ['-p', str(args.ssh_port)]
    if args.ssh_identity_file:
        identity = Path(args.ssh_identity_file).expanduser()
        if not identity.is_file() or any(c in str(identity) for c in '\r\n\0'):
            raise ValueError('SSH identity file is unavailable')
        command += ['-i', str(identity)]
    return command + [args.ssh_target]


def remote_executable(value):
    if not value or any(c in value for c in '\r\n\0'):
        raise ValueError('Invalid remote Python executable')
    return '"$HOME"/' + shlex.quote(value[2:]) if value.startswith('~/') else shlex.quote(value)


@contextlib.contextmanager
def connection(args, agent, job_id, operation, **kwargs):
    command = remote_executable(args.ssh_python_executable) + ' -u -c ' + shlex.quote(agent)
    command += ' ' + ' '.join(shlex.quote(value) for value in (operation, args.ssh_work_dir, job_id))
    process = subprocess.Popen(ssh_arguments(args) + [command], start_new_session=True, **kwargs)
    timed_out = threading.Event()
    def expire():
        if process.poll() is None:
            timed_out.set()
            with contextlib.suppress(ProcessLookupError):
                os.killpg(process.pid, signal.SIGTERM)
    timeout = args.ssh_run_timeout if operation in ('run', 'upload', 'download') else max(30, args.ssh_connect_timeout + 20)
    timer = threading.Timer(timeout, expire); timer.daemon = True; timer.start()
    try:
        yield process
        code = process.wait(timeout=timeout + 5)
        if timed_out.is_set():
            raise TimeoutError('SSH ' + operation + ' timed out')
        if code:
            raise RuntimeError('SSH ' + operation + ' failed (exit ' + str(code) + '); check the SSH log and remote environment')
    finally:
        timer.cancel()
        if process.poll() is None:
            with contextlib.suppress(ProcessLookupError):
                os.killpg(process.pid, signal.SIGTERM)
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                with contextlib.suppress(ProcessLookupError):
                    os.killpg(process.pid, signal.SIGKILL)
                process.wait()
        for stream in (process.stdin, process.stdout, process.stderr):
            if stream is not None and not stream.closed:
                stream.close()


def inventory(args):
    import superpoint_lightglue_worker as worker
    paths = worker.read_request(Path(args.request))
    sources = {}; images = []
    for path in paths:
        name = 'images/' + digest(path) + '.img'
        sources[name] = path; images.append(name)
    script = Path(args.worker).resolve(strict=True)
    if not script.is_file() or script.is_symlink():
        raise ValueError('Signed sparse worker is unavailable')
    sources['superpoint_lightglue_worker.py'] = script
    files = [dict(name=name, size=path.stat().st_size, sha256=digest(path)) for name, path in sources.items()]
    if any(not 0 < row['size'] <= 1024 ** 3 for row in files) or sum(row['size'] for row in files) > 32 * 1024 ** 3:
        raise ValueError('SSH inputs exceed supported bounds')
    options = {key: value for key,value in vars(args).items()
               if not key.startswith('ssh_') and key not in ('request', 'output', 'runtime_root', 'worker')}
    config = dict(protocol=PROTOCOL, files=files, images=images, options=options,
                  runtime_root=args.ssh_runtime_folder, run_timeout=args.ssh_run_timeout,
                  result_limit=args.ssh_result_limit_mb * 1024 ** 2)
    return sources, config


class ProgressWriter:
    def __init__(self, output, total):
        self.output = output; self.total = total; self.done = 0; self.last = -1
    def write(self, data):
        result = self.output.write(data); self.done += len(data)
        percent = min(100, self.done * 100 // max(1, self.total))
        if percent > self.last:
            emit('upload', percent=percent, bytes=min(self.done, self.total), total_bytes=self.total); self.last = percent
        return result


def receive_results(stream, output, count, job_id, maximum):
    header = stream.readline(MAX_HEADER + 1)
    if not header.endswith(b'\n') or len(header) > MAX_HEADER:
        raise ValueError('Invalid SSH result manifest')
    manifest = json.loads(header)
    if manifest.get('protocol') != PROTOCOL or manifest.get('job_id') != job_id or manifest.get('image_count') != count:
        raise ValueError('SSH result identity mismatch')
    required = {'result.bin'}
    optional = set()
    entries = manifest.get('files')
    if not isinstance(entries, list) or len(entries) > len(required) + len(optional):
        raise ValueError('Invalid SSH result file count')
    names = [row.get('name') for row in entries]
    if len(set(names)) != len(names) or not required <= set(names) or set(names) - required - optional:
        raise ValueError('Unexpected or missing SSH results')
    if any(type(row.get('size')) is not int or not 0 <= row['size'] <= maximum
           or not re.fullmatch(r'[0-9a-f]{64}', row.get('sha256', '')) for row in entries):
        raise ValueError('Invalid SSH result size or digest')
    total = sum(row['size'] for row in entries)
    if total > maximum:
        raise ValueError('SSH results exceed ssh_result_limit_mb')
    result_path = Path(output)
    output = result_path.parent
    if output.is_symlink():
        raise ValueError('Result directory must not be a symlink')
    output.mkdir(parents=True, exist_ok=True)
    done = 0; last = -1
    with tempfile.TemporaryDirectory(prefix='.sparse-download-', dir=output) as staging:
        for row in entries:
            checksum = hashlib.sha256(); remaining = row['size']
            with (Path(staging) / row['name']).open('xb') as destination:
                while remaining:
                    chunk = stream.read(min(1024 * 1024, remaining))
                    if not chunk:
                        raise ValueError('Truncated SSH result')
                    checksum.update(chunk); destination.write(chunk); remaining -= len(chunk); done += len(chunk)
                    percent = min(100, done * 100 // max(1, total))
                    if percent > last:
                        emit('download', percent=percent, bytes=done, total_bytes=total); last = percent
            if checksum.hexdigest() != row['sha256']:
                raise ValueError('SSH result checksum mismatch')
        if stream.read(1):
            raise ValueError('Unexpected trailing SSH result bytes')
        for name in names:
            destination = result_path
            if destination.is_symlink():
                raise ValueError('Result file must not be a symlink')
        for name in names:
            os.replace(Path(staging) / name, result_path)
    return manifest


def execute(args):
    ssh_arguments(args)
    agent = Path(__file__).with_name('superpoint_lightglue_ssh_agent.py').read_text(encoding='utf8')
    sources, config = inventory(args)
    job_id = uuid.uuid4().hex; prepared = finished = False
    emit('connecting', target=args.ssh_target, images=len(config['images']), job_id=job_id)
    try:
        with connection(args, agent, job_id, 'prepare', stdin=subprocess.PIPE, stdout=subprocess.PIPE) as process:
            response, _ = process.communicate(json.dumps(config).encode() + b'\n')
        prepared = True
        events = [json.loads(line) for line in response.splitlines() if line.startswith(b'{')]
        if not any(event.get('protocol') == PROTOCOL and event.get('job_id') == job_id and event.get('event') == 'sparse_ssh_prepared' for event in events):
            raise ValueError('Remote Sparse frontend preparation did not return a valid identity')
        total = sum(row['size'] for row in config['files'])
        with connection(args, agent, job_id, 'upload', stdin=subprocess.PIPE, stdout=subprocess.DEVNULL) as process:
            with tarfile.open(fileobj=ProgressWriter(process.stdin, total), mode='w|') as archive:
                for name, path in sources.items():
                    archive.add(path, arcname=name, recursive=False)
            process.stdin.close()
        emit('matching', target=args.ssh_target)
        with connection(args, agent, job_id, 'run', stdout=subprocess.PIPE) as process:
            while True:
                line = process.stdout.readline(1024 * 1024 + 1)
                if not line:
                    break
                if len(line) > 1024 * 1024:
                    raise ValueError('SSH log line exceeds limit')
                sys.stdout.buffer.write(line); sys.stdout.buffer.flush()
                try:
                    event = json.loads(line)
                    if event.get('protocol') == PROTOCOL and event.get('job_id') == job_id and event.get('event') == 'sparse_ssh_finished':
                        finished = True
                except (ValueError, AttributeError):
                    pass
        if not finished:
            raise ValueError('SSH worker returned without a completion record')
        with connection(args, agent, job_id, 'download', stdout=subprocess.PIPE) as process:
            manifest = receive_results(process.stdout, args.output, len(config['images']), job_id, config['result_limit'])
        emit('complete', images=len(config['images']), files=len(manifest['files']))
        return manifest
    finally:
        if prepared:
            for operation in (['cancel'] if not finished else []) + ([] if args.ssh_keep_work_dir else ['cleanup']):
                try:
                    with connection(args, agent, job_id, operation, stdout=subprocess.DEVNULL):
                        pass
                except Exception as error:
                    print('Sparse frontend SSH ' + operation + ': ' + str(error), file=sys.stderr)


def parser():
    import superpoint_lightglue_worker as worker
    result = worker.parser()
    result.add_argument('--worker', required=True)
    result.add_argument('--ssh-target', default='remoteMAC')
    result.add_argument('--ssh-port', type=int, default=0)
    result.add_argument('--ssh-identity-file', default='')
    result.add_argument('--ssh-python-executable', required=True)
    result.add_argument('--ssh-runtime-folder', default='')
    result.add_argument('--ssh-work-dir', default='~/.cache/PoSDK/SuperPointLightGlue')
    result.add_argument('--ssh-connect-timeout', type=int, default=15)
    result.add_argument('--ssh-run-timeout', type=int, default=3600)
    result.add_argument('--ssh-result-limit-mb', type=int, default=8192)
    result.add_argument('--ssh-keep-work-dir', action='store_true')
    return result


if __name__ == '__main__':
    os.umask(0o077)
    def interrupted(_signal, _frame):
        raise InterruptedError('Sparse frontend SSH cancelled')
    signal.signal(signal.SIGTERM, interrupted)
    try:
        execute(parser().parse_args())
    except (Exception, KeyboardInterrupt) as error:
        print('Sparse frontend SSH failed: ' + str(error), file=sys.stderr, flush=True)
        raise SystemExit(1)
