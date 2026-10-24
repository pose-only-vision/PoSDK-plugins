# `method_aliked_lightglue` dependency lock

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
- The ALIKED-n16 checkpoint comes from
  <https://github.com/Shiaoming/ALIKED> at commit
  `683d7c65197395c0b3f01ebe76e1084a27e73a65`; its SHA-256 is
  `5be8704840ed662d9d8c561bf7279c222092674e7eb05fd0feab94899e9d82f2`.
- The ALIKED-compatible LightGlue `v0.1_arxiv` checkpoint SHA-256 is
  `d975e965b105311a6143194852297dff4f02aea5cc2e10cecfed966ca0e22503`.
- LightGlue is Apache-2.0 licensed. The embedded ALIKED implementation and
  checkpoint repository are BSD-3-Clause licensed. Both notices ship with the
  plugin.
- The certified worker decodes images with Pillow RGB. It does not install or
  import OpenCV. A fail-closed module stub only satisfies LightGlue's eager
  imports of unused optional SIFT/image helpers; calling any OpenCV API fails.
- The open Python dependencies are downloaded directly from their public
  distributions. Their full wheel/Conda license files, notices, and SBOMs
  remain inside the isolated environment; the two locks are the authoritative
  package and artifact inventory.

The installer verifies every downloaded object, validates the extracted source
tree, and executes a real CPU ALIKED+LightGlue inference smoke test before it
returns the signed `python_bin` configuration binding.

The certified runtime policy maps both `auto` and `cpu` to the all-CPU path.
`mps` is retained only as an explicit experimental diagnostic: ALIKED stays on
CPU and LightGlue moves to Metal. The pinned runtime does not select MPS
automatically because the current Strecha smoke benchmarks show a regression in
matching time and peak memory.

## Exact Python wheel inventory

The following is the complete hash-locked wheel closure for macOS arm64.
The signed installer uses these same versions and SHA-256 values. Each
distribution retains its complete license files and bundled-library notices.

| Distribution / source index | Version | Artifact SHA-256 |
|---|---|---|

The Python 3.11.11 base and system-library artifacts are fixed in the signed
Conda explicit lock. The installation recipe is the public `install.sh`;
upstream attribution and license locations are in `THIRD_PARTY_NOTICES.md`.
