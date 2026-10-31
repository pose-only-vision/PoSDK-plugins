# `method_superpoint_lightglue` dependency lock

The signed macOS arm64 package creates a private Python environment and a
private model runtime below `POSDK_INSTALL_STATE_ROOT`. It never activates or
modifies another plugin's environment, and normal inference is offline.

- Python 3.11.11 is fixed by `conda-osx-arm64.lock`.
- PyTorch 2.2.2, torchvision 0.17.2, NumPy 1.26.4, Pillow 12.3.0,
  Kornia 0.6.12, and their complete wheel closure are fixed by
  `requirements-osx-arm64.lock`.
- LightGlue is downloaded from
  <https://github.com/cvg/LightGlue> at commit
  `eb42fee2d71449efb0aa5c10549752b5d75384d8`. The source archive SHA-256 is
  `e5c696a4c69be4e335b090ce3f70da1060f8aa52e61d463e96a930ebefd57a19`,
  and the canonical extracted-tree SHA-256 is
  `78cc712925a8de33ef9be7e807d0395f916dbd4eeb28f3644243ae2a00297d33`.
- SuperPoint's implementation is part of the above pinned source tree. Its
  `superpoint_v1.pth` checkpoint SHA-256 is
  `52b6708629640ca883673b5d5c097c4ddad37d8048b33f09c8ca0d69db12c40e`.
- The SuperPoint-compatible LightGlue `v0.1_arxiv` checkpoint SHA-256 is
  `6ff7040d0a497fc6639337946d7538dae07428c18f77a067a0b5a960e7cc551a`.
  Both checkpoints are fetched from https://github.com/cvg/LightGlue/releases/tag/v0.1_arxiv.
- LightGlue is Apache-2.0. SuperPoint's separately licensed implementation and
  weights are restricted to noncommercial research by Magic Leap's license.
  See `SUPERPOINT-LICENSE.txt`; the wrapper does not grant further rights.
- The certified worker decodes images with Pillow RGB. It does not install or
  import OpenCV. A fail-closed module stub only satisfies LightGlue's eager
  imports of unused optional SIFT/image helpers; calling any OpenCV API fails.
- The open Python dependencies are downloaded directly from their public
  distributions. Their full wheel/Conda license files, notices, and SBOMs
  remain inside the isolated environment; the two locks are the authoritative
  package and artifact inventory.

The installer verifies every downloaded object, validates the extracted source
tree, and executes a real CPU SuperPoint+LightGlue inference smoke test before it
returns the signed `python_bin` configuration binding.

The default runtime policy maps `auto` and `cpu` to CPU. Explicit `mps`
keeps extraction on CPU and uses Metal for LightGlue. Explicit `cuda` requires
CUDA for both stages. macOS arm64 is the initial signed package target.

## Exact Python wheel inventory

The following is the complete hash-locked wheel closure for macOS arm64.
The signed installer uses these same versions and SHA-256 values. Each
distribution retains its complete license files and bundled-library notices.

| Distribution / source index | Version | Artifact SHA-256 |
|---|---|---|

The Python 3.11.11 base and system-library artifacts are fixed in the signed
Conda explicit lock. The installation recipe is the public `install.sh`;
upstream attribution and license locations are in `THIRD_PARTY_NOTICES.md`.
