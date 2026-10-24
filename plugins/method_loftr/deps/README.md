# `method_loftr` private dependencies

This macOS arm64 plugin creates an isolated Python 3.11.11 runtime below
`POSDK_INSTALL_STATE_ROOT/env`. The explicit Conda lock fixes each package URL
and SHA-256. The pip lock fixes the complete binary-wheel closure, including
PyTorch 2.2.2, Kornia 0.7.4, NumPy 1.26.4 and OpenCV headless 4.10.0.84.
`install.sh` is the executable creation recipe and `install-smoke.py` loads
the real outdoor LoFTR model on CPU and checks finite correspondences.

The inference implementation is [Kornia 0.7.4 source](https://github.com/kornia/kornia/tree/v0.7.4)
(Apache-2.0), using the original [LoFTR project](https://github.com/zju3dv/LoFTR)
(Apache-2.0) and its dual-softmax checkpoints. The optional SuperGlue
optimal-transport implementation is not included. Upstream attribution and
license notices are retained in `THIRD_PARTY_NOTICES.md` and the installed
distributions.

Kornia's pinned model URLs use HTTPS at `cmp.felk.cvut.cz/~mishkdmy/models/`:

- `loftr_outdoor.ckpt`: `21f5bec5968178e8bc8b7633441836fe5de4f47d861dd2cd7dc38e271b0479ec`
- `loftr_indoor.ckpt`: `d73c54720370ba0690cd477e38404a626e881292cc0be09a6801da0cc6f53198`

The installer uses a local Conda-compatible environment creator, or downloads
micromamba 2.3.2 from its official osx-arm64 endpoint, SHA-256
`ae0b50b441fef93abd20711f18b074359a7f8f1f523a8046a4d6ab44aa68ff1b`.
It never activates or changes the user's environment. See
`THIRD_PARTY_NOTICES.md` for upstream licenses and authors; the full package
licenses remain in the installed distributions.

Native libraries included in the final package must be declared by the
plugin's same-stem `.dependencies.json` and use loader-relative paths.

## Exact Python wheel inventory

The following is the complete hash-locked wheel closure for macOS arm64.
The signed installer uses these same versions and SHA-256 values. Each
distribution retains its complete license files and bundled-library notices.

| Distribution / source index | Version | Artifact SHA-256 |
|---|---|---|
| [filelock](https://pypi.org/project/filelock/4.2.4/) | 4.2.4 | `301241438c57936ef93f4243a94f2559022268bfcf9b375a9a72eb0f8db8358e` |
| [fsspec](https://pypi.org/project/fsspec/2026.9.0/) | 2026.9.0 | `8dd6e646e99ea382bd85f97a45e6b526a442d79423a7dc673f1e2756d05fcb5f` |
| [jinja2](https://pypi.org/project/jinja2/3.1.6/) | 3.1.6 | `85ece4451f492d0c13c5dd7c13a64681a86afae63a5f347908daf103ce6d2f67` |
| [kornia](https://pypi.org/project/kornia/0.7.4/) | 0.7.4 | `1b816b4c45878a73531a500a46dbf61e646ec7a749bf8a4dd7a5833a04e28808` |
| [kornia-rs](https://pypi.org/project/kornia-rs/0.2.0/) | 0.2.0 | `291abe5874d8b746cee439d93b94be6a95313b9301c4bd0a51de25124f17b5a4` |
| [markupsafe](https://pypi.org/project/markupsafe/3.0.4/) | 3.0.4 | `7d3391b2188d18737cb2fa147028b1096236eaa7e156446c650a489fa2cadc91` |
| [mpmath](https://pypi.org/project/mpmath/1.3.0/) | 1.3.0 | `a0b2b9fe80bbcd81a6647ff13108738cfb482d481d826cc0e02f5b35e5c88d2c` |
| [networkx](https://pypi.org/project/networkx/3.6.1/) | 3.6.1 | `d47fbf302e7d9cbbb9e2555a0d267983d2aa476bac30e90dfbe5669bd57f3762` |
| [numpy](https://pypi.org/project/numpy/1.26.4/) | 1.26.4 | `edd8b5fe47dab091176d21bb6de568acdd906d1887a4584a15a9a96a1dca06ef` |
| [opencv-python-headless](https://pypi.org/project/opencv-python-headless/4.10.0.84/) | 4.10.0.84 | `a4f4bcb07d8f8a7704d9c8564c224c8b064c63f430e95b61ac0bffaa374d330e` |
| [packaging](https://pypi.org/project/packaging/26.3/) | 26.3 | `d7193f7c8e4e93f444fde0262bf90af30e16fa0ad0ad44cb553c87339b23cd1c` |
| [pillow](https://pypi.org/project/pillow/13.0.0/) | 13.0.0 | `37290fbef0c73e2d51d594eb78fded7dc7170d1c3ad0a890269e6fd8c8c4e061` |
| [sympy](https://pypi.org/project/sympy/1.14.0/) | 1.14.0 | `e091cc3e99d2141a0ba2847328f5479b05d94a6635cb96148ccb3f34671bd8f5` |
| [torch](https://pypi.org/project/torch/2.2.2/) | 2.2.2 | `49aa4126ede714c5aeef7ae92969b4b0bbe67f19665106463c39f22e0a1860d1` |
| [torchvision](https://pypi.org/project/torchvision/0.17.2/) | 0.17.2 | `e031004a1bc432c980a7bd642f6c189a3efc316e423fc30b5569837166a4e28d` |
| [typing-extensions](https://pypi.org/project/typing-extensions/4.16.0/) | 4.16.0 | `481caa481374e813c1b176ada14e97f1f67a4539ce9cfeb3f350d78d6370c2e8` |

The Python 3.11.11 base and system-library artifacts are fixed in the signed
Conda explicit lock. The installation recipe is the public `install.sh`;
upstream attribution and license locations are in `THIRD_PARTY_NOTICES.md`.
