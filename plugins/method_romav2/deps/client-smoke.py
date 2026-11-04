import ast,hashlib,json,platform,subprocess,sys
from pathlib import Path
assert platform.machine()=='arm64'
assert platform.python_version()=='3.11.11'
root=Path(__file__).resolve().parents[1]
for name in ['install.sh','deps/runtime-lock.json','deps/remote-requirements.lock']:
    p=root/name
    assert p.is_file() and not p.is_symlink()
json.loads((root/'deps/runtime-lock.json').read_text())
subprocess.run(['/usr/bin/ssh','-V'],check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
assert not any(n in sys.modules for n in ['torch','numpy','onnxruntime'])
print(json.dumps(dict(controller='stdlib-only',remote_model_runtime='required; validated separately',architecture=platform.machine(),python=platform.python_version())))
