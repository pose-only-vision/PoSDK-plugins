PoMVG integration layer only. ALIKED, LightGlue, their implementations and model weights retain the upstream authors, copyright holders and licenses listed below.

# Third-party notices

`method_aliked_lightglue` is distributed as a closed PoMVG plugin with open
dependencies. The signed `.pospkg` contains the installer recipes and license
notices; the isolated runtime dependencies are downloaded from their public
upstreams during installation.

## ALIKED

- Original authors: Xiaoming Zhao, Xingming Wu, Weihai Chen, Peter C. Y. Chen, Qingsong Xu, and Zhengguo Li

- Project: <https://github.com/Shiaoming/ALIKED>
- Pinned commit: `683d7c65197395c0b3f01ebe76e1084a27e73a65`
- Component: `aliked-n16.pth` checkpoint from the pinned ALIKED repository; the
  ALIKED-derived implementation in LightGlue is separately fixed by the
  LightGlue source-tree hash
- License: BSD 3-Clause
- Copyright: Copyright (c) 2022, Zhao Xiaoming
- Basis for the checkpoint notice: the repository root BSD-3-Clause license;
  the checkpoint has no separate model-card terms
- Redistributed full text: `licenses/ALIKED-BSD-3-Clause.txt`

## LightGlue

- Original team: Computer Vision and Geometry Group (CVG), ETH Zurich
- Original authors: Philipp Lindenberger, Paul-Edouard Sarlin, and Marc Pollefeys

- Project: <https://github.com/cvg/LightGlue>
- Pinned commit: `eb42fee2d71449efb0aa5c10549752b5d75384d8`
- Component: LightGlue source and the ALIKED-compatible `v0.1_arxiv` checkpoint
- Checkpoint release tag commit:
  `c91eb892b799fa607523879d18f9ec4fe76194a5`
- Checkpoint URL:
  <https://github.com/cvg/LightGlue/releases/download/v0.1_arxiv/aliked_lightglue.pth>
- Checkpoint SHA-256:
  `d975e965b105311a6143194852297dff4f02aea5cc2e10cecfed966ca0e22503`
- License: Apache License 2.0
- Copyright: Copyright 2023 ETH Zurich
- Redistributed full text: `licenses/LightGlue-Apache-2.0.txt`

## Isolated Python runtime

The installer creates a Python 3.11.11 environment from the exact package URLs
and hashes in `deps/conda-osx-arm64.lock`, then installs the complete
hash-locked wheel closure in `deps/requirements-osx-arm64.lock`. Direct runtime
components include:

- CPython 3.11.11
- PyTorch 2.2.2 and torchvision 0.17.2
- NumPy 1.26.4
- Pillow 12.3.0
- Kornia 0.6.12

The exact open distributions retain their complete license files, notices,
third-party attributions, and any supplied SBOMs inside the installed private
environment. Those embedded texts, rather than a simplified single-license
label here, govern each distribution and its bundled libraries. The two lock
files are the authoritative inventory for direct and transitive package
versions and artifact hashes; no ambient Python package is imported.

For review without installing the environment, the signed plugin also ships
verbatim license/notice snapshots from the exact hash-locked wheels under
`licenses/runtime/`: PyTorch `LICENSE` and `NOTICE`, torchvision `LICENSE`,
NumPy's aggregate and three component licenses, Kornia `LICENSE`, and Pillow's
aggregate bundled-library `LICENSE`. The installed wheel metadata remains the
authority if a formatting-only difference exists in a copied text.

The certified runtime intentionally excludes `opencv-python-headless`.
Dataset images are decoded as RGB with Pillow 12.3.0 (license expression
`MIT-CMU`). A fail-closed compatibility stub only allows LightGlue to import
unused optional OpenCV-dependent helpers; any attempted OpenCV API call raises.
Pillow's installed wheel retains its aggregate bundled-library license file and
CycloneDX SBOM under `pillow-12.3.0.dist-info`.

If no trusted local Conda-compatible runner is available, the installer
downloads SHA-256-pinned micromamba 2.3.2 for macOS arm64. Micromamba is
distributed under the BSD 3-Clause License by the mamba-org contributors.

These notices are provided for attribution and license visibility and do not
alter the licenses of the named projects.
