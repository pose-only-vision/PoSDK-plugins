#!/usr/bin/env python3
"""Run only on the model SSH server, using a compatible operator-supplied base Python."""
import argparse,hashlib,json,os,platform,shutil,subprocess,sys,tarfile,tempfile
from pathlib import Path
def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for block in iter(lambda:f.read(8*1024*1024),b''):h.update(block)
 return h.hexdigest()
def receive(url,sha,target,cache):
 target.parent.mkdir(parents=True,exist_ok=True)
 if target.exists():
  if target.is_symlink() or not target.is_file() or digest(target)!=sha:
   raise RuntimeError('Existing runtime asset differs; no overwrite: '+str(target))
  return
 cached=cache/sha if cache else None
 if cached and cached.is_file() and not cached.is_symlink() and digest(cached)==sha:
  shutil.copyfile(cached,target);return
 with tempfile.TemporaryDirectory(prefix='posdk-model-download-',dir=target.parent) as t:
  tmp=Path(t)/'asset'
  subprocess.run(['/usr/bin/curl','--fail','--location','--proto','=https','--tlsv1.2',
    '--retry','2',url,'--output',str(tmp)],check=True)
  if digest(tmp)!=sha:raise RuntimeError('Upstream asset SHA256 mismatch')
  os.replace(tmp,target)
def unpack(archive,destination):
 if destination.exists():
  if destination.is_symlink() or not destination.is_dir():raise RuntimeError('Unsafe source destination')
  return
 destination.mkdir(parents=True)
 with tarfile.open(archive,'r:gz') as tar:
  total=0
  for m in tar:
   parts=Path(m.name).parts
   if len(parts)<2:continue
   rel=Path(*parts[1:])
   if rel.is_absolute() or '..' in rel.parts or m.issym() or m.islnk():raise RuntimeError('Unsafe upstream archive')
   if m.isdir():continue
   if not m.isfile():raise RuntimeError('Unsupported source member')
   total+=m.size
   if total>512*1024**2:raise RuntimeError('Source archive too large')
   out=destination/rel;out.parent.mkdir(parents=True,exist_ok=True)
   with out.open('xb') as f:shutil.copyfileobj(tar.extractfile(m),f)
def main():
 p=argparse.ArgumentParser();p.add_argument('--runtime-root',required=True);p.add_argument('--base-python',required=True);p.add_argument('--asset-cache',default='')
 a=p.parse_args();here=Path(__file__).resolve().parent
 lock=json.loads((here/'runtime-lock.json').read_text());root=Path(a.runtime_root).expanduser().absolute()
 if platform.system()!='Darwin' or platform.machine()!='arm64':raise RuntimeError('Only macOS arm64 is certified')
 if root.is_symlink():raise RuntimeError('Runtime root cannot be a symlink')
 root.mkdir(parents=True,exist_ok=True)
 expected=lock['runtime']['python']
 base=shutil.which(a.base_python) if not Path(a.base_python).is_absolute() else a.base_python
 if not base:raise RuntimeError('Compatible base Python is required')
 version=subprocess.check_output([base,'-I','-c','import platform;print(platform.python_version())'],text=True).strip()
 if version!=expected:raise RuntimeError('Expected base Python '+expected+', got '+version)
 env=root/'env'
 if not env.exists():subprocess.run([base,'-m','venv',str(env)],check=True)
 python=env/'bin'/('python'+'.'.join(expected.split('.')[:2]))
 if not python.exists():raise RuntimeError('Invalid private environment')
 if (env/'pyvenv.cfg').read_text().lower().find('include-system-site-packages = false')<0:
  raise RuntimeError('Ambient site packages are forbidden')
 subprocess.run([str(python),'-m','pip','install','--disable-pip-version-check','--no-input',
  '--only-binary=:all:','--require-hashes','--requirement',str(here/'remote-requirements.lock')],check=True)
 subprocess.run([str(python),'-m','pip','check'],check=True)
 cache=Path(a.asset_cache).expanduser() if a.asset_cache else None
 if 'models' in lock:
  for x in lock['models']:receive(x['url'],x['sha256'],root/'models'/x['name'],cache)
 else:
  recipe=json.loads((here/'source-downloads.json').read_text())
  for name,x in recipe['sources'].items():
   for file,sha in lock['sources'][name]['files'].items():
    receive(x['raw_base']+file,sha,root/'vendor'/name/file,cache)
  receive(recipe['model_url'],lock['model_sha256'],root/'assets/romav2.0.1.pt',cache)
 script="import importlib.metadata as m,platform,json; print(json.dumps(dict(python=platform.python_version(),"+','.join(k+"=m.version("+repr({'pillow':'Pillow','opencv':'opencv-python-headless'}.get(k,k))+")" for k in lock['runtime'] if k!='python')+")))"
 actual=json.loads(subprocess.check_output([str(python),'-I','-c',script],text=True))
 if actual!=lock['runtime']:raise RuntimeError('Installed versions differ: '+str(actual))
 receipt=dict(schema='posdk.remote-runtime.v1',runtime_root=str(root),python=str(python),versions=actual,
  model_identity='pinned source/ONNX/checkpoint hashes verified',inference_smoke='not performed by provisioning; execute the real image worker')
 (root/'provision-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
 print(json.dumps(receipt))
if __name__=='__main__':main()
