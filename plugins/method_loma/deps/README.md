# Pinned dependencies and remote deployment

The signed macOS arm64 installer creates only a private Python3.11.11
standard-library controller under POSDK_INSTALL_STATE_ROOT. It reports the
python_bin binding; it does not activate a user environment or install model
weights on the GUI host. The exact Conda URLs/SHA256 are embedded in the signed
deps/conda-osx-arm64.lock. Its fallback Micromamba2.3.2 archive is SHA256-pinned
in install.sh.

## Model service contract

Model execution requires a separately configured, owner-controlled SSH
server. Run the package-internal deps/provision-remote.py on that server with
a compatible base Python and a dedicated runtime directory:

    python3 provision-remote.py --base-python /absolute/compatible/python \
      --runtime-root "$HOME/.local/share/PoSDK/MODEL/0.1.0/macos-arm64"

Copy the signed deps resources together before invoking the helper. The helper
creates a new private venv, refuses system-site packages, uses pip
--require-hashes, verifies sources/assets and runtime versions, and writes a
provision receipt. It does not claim a model-inference or accuracy pass. A real
worker image test is required separately. Existing differing files are rejected,
rather than overwritten. Configure the resulting env/bin interpreter and
runtime_root in the GUI node. Optional asset-cache entries are verified by
content SHA256 before reuse.

The server is a declared external service, not a local model environment.
The client installation therefore requires SSH configuration before matching.
All returned results are job-bound and size/SHA256 verified.

## Model, source and license disclosure

LoMa: https://github.com/davnords/LoMa ; MIT except its LightGlue-derived
matcher (Apache-2.0). ONNX inference uses the original authors' public assets.
The coordinate/preprocessing reference is COLMAP at revision
bd1fcf654d2dd8fefa1466999c190a246f83f4b9:
https://github.com/colmap/colmap/tree/bd1fcf654d2dd8fefa1466999c190a246f83f4b9
(COLMAP: BSD-3-Clause). ONNX Runtime is MIT, pycolmap BSD-3-Clause, NumPy BSD,
Pillow HPND, and Python PSF; each distribution retains its complete notices.

- detector: https://github.com/davnords/storage/releases/download/loma/loma_detector.onnx
  SHA256: b6af99c5e730034ac9b675d1ebe05d0679af4569a3c26f10a6a50f91e02dc512; bytes: 26187830
- descriptor: https://github.com/davnords/storage/releases/download/loma/loma_descriptor_dedode_g.onnx
  SHA256: 5a7b9eaf7425d4513c5d7feae86080bae7ed3aceae7fb1b9f059d0752e2ad564; bytes: 1294905349
- matcher: https://github.com/davnords/storage/releases/download/loma/loma_matcher_B.onnx
  SHA256: ba5a2773b29cace19f1240e14e5a080cca3eaf9f69a7adb829a1d470557001c7; bytes: 48361254

## Exact remote environment

Expected Python: 3.11.11

Pip --require-hashes lock (platform wheels resolved on the matching Mac target):

~~~text
coloredlogs==15.0.1 --hash=sha256:612ee75c546f53e92e70049c9dbfcc18c935a2b9a53b66085ce9ef6a6e5c0934
flatbuffers==25.12.19 --hash=sha256:7634f50c427838bb021c2d66a3d1168e9d199b0607e6329399f04846d42e20b4
humanfriendly==10.0 --hash=sha256:1697e1a8a8f550fd43c2865cd84542fc175a61dcb779b6fee18cf6b6ccba1477
mpmath==1.3.0 --hash=sha256:a0b2b9fe80bbcd81a6647ff13108738cfb482d481d826cc0e02f5b35e5c88d2c
numpy==1.26.4 --hash=sha256:edd8b5fe47dab091176d21bb6de568acdd906d1887a4584a15a9a96a1dca06ef
onnxruntime==1.23.2 --hash=sha256:6f91d2c9b0965e86827a5ba01531d5b669770b01775b23199565d6c1f136616c
packaging==26.3 --hash=sha256:d7193f7c8e4e93f444fde0262bf90af30e16fa0ad0ad44cb553c87339b23cd1c
pillow==12.3.0 --hash=sha256:37d6d0a00072fd2948eb22bce7e1475f34569d90c87c59f7a2ec59541b77f7a6
protobuf==7.36.2 --hash=sha256:cbc70b17ee27e28894c7fee8bb04be1abead49e936bc70eb60052531eee2079e
pycolmap==4.2.1 --hash=sha256:73204e55c75ab637231d4ca559153d658ec7eda9dec39a1aacad96fbc0300cb9
sympy==1.14.0 --hash=sha256:e091cc3e99d2141a0ba2847328f5479b05d94a6635cb96148ccb3f34671bd8f5
~~~
