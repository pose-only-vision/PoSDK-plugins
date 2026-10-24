# LoFTR third-party notices / 第三方依赖声明

**PoMVG plugin integration author:** qi Cai

**Contact:** <qicaiCN@gmail.com>

This authorship covers the PoMVG integration, configuration, SSH transport and
result conversion. It does not replace the authorship or copyright of LoFTR,
Kornia, pretrained models or runtime libraries. The integration retains its
existing MPL-2.0 license; upstream components retain their own terms.

## LoFTR algorithm and implementation

- Original LoFTR authors: Jiaming Sun, Zehong Shen, Yu'ang Wang, Hujun Bao and
  Xiaowei Zhou; the Visual Intelligence Group, Zhejiang University.
- Original project: <https://github.com/zju3dv/LoFTR> (Apache-2.0).
- This plugin executes the **Kornia 0.7.4 implementation**, rather than cloning
  an unpinned LoFTR branch: <https://github.com/kornia/kornia/tree/v0.7.4>.
- Implementation source:
  <https://github.com/kornia/kornia/blob/v0.7.4/kornia/feature/loftr/loftr.py>.
- Kornia wheel SHA-256:
  `1b816b4c45878a73531a500a46dbf61e646ec7a749bf8a4dd7a5833a04e28808`.
- Kornia license: Apache-2.0; verbatim installed-wheel text is redistributed
  as `licenses/runtime/KORNIA-0.7.4-LICENSE.txt`.
- The original project's optional SuperGlue optimal-transport code is not
  part of this integration. This runtime uses the dual-softmax models below.

## Pretrained model files

The installer downloads the checkpoint URLs selected by Kornia 0.7.4 and
checks the entire file before use. These are upstream pretrained model files,
not models trained by qi Cai or PoMVG.

| Model | Upstream download | SHA-256 |
|---|---|---|
| Outdoor / MegaDepth | <https://cmp.felk.cvut.cz/~mishkdmy/models/loftr_outdoor.ckpt> | `21f5bec5968178e8bc8b7633441836fe5de4f47d861dd2cd7dc38e271b0479ec` |
| Indoor / ScanNet | <https://cmp.felk.cvut.cz/~mishkdmy/models/loftr_indoor.ckpt> | `d73c54720370ba0690cd477e38404a626e881292cc0be09a6801da0cc6f53198` |

The original LoFTR project, its license and model release remain the source
of upstream attribution. No additional model license is invented here.
Models are downloaded during installation, not embedded in the native plugin.
An explicitly configured custom checkpoint retains its own provenance and terms.

## Python and inference libraries

| Component | Exact supported version | Purpose | Source and license location |
|---|---|---|---|
| CPython | 3.11.11 | Plugin-private interpreter | <https://github.com/python/cpython/tree/v3.11.11>; Python and bundled license texts in the installed Conda distribution |
| PyTorch | 2.2.2 | Model/tensor execution | <https://github.com/pytorch/pytorch/tree/v2.2.2>; aggregate LICENSE and NOTICE, copied verbatim to `licenses/runtime/TORCH-2.2.2-*` |
| torchvision | 0.17.2 | Compatible vision runtime | <https://github.com/pytorch/vision/tree/v0.17.2>; `licenses/runtime/TORCHVISION-0.17.2-LICENSE.txt` |
| NumPy | 1.26.4 | Arrays and coordinate conversion | <https://github.com/numpy/numpy/tree/v1.26.4>; aggregate and component licenses in the installed wheel, with aggregate text at `licenses/runtime/NUMPY-1.26.4-LICENSE.txt` |
| Kornia | 0.7.4 | LoFTR implementation | <https://github.com/kornia/kornia/tree/v0.7.4>; Apache-2.0, `licenses/runtime/KORNIA-0.7.4-LICENSE.txt` |
| OpenCV Python headless | 4.10.0.84 (module 4.10.0) | Image decoding/resizing and optional geometric checks | <https://github.com/opencv/opencv-python>; wrapper and bundled-library terms at `licenses/runtime/OPENCV-PYTHON-4.10.0.84-LICENSE.txt` and `OPENCV-PYTHON-4.10.0.84-LICENSE-3RD-PARTY.txt` |
| micromamba | 2.3.2, only when a local environment creator is unavailable | Environment bootstrap | <https://github.com/mamba-org/mamba>; BSD-3-Clause and bundled notices |

The signed installer uses `deps/conda-osx-arm64.lock` for the base environment
and `deps/requirements-osx-arm64.lock` for the complete binary-wheel closure.
Each artifact has a fixed URL/version and SHA-256; pip uses `--require-hashes`.
See `deps/README.md` for the public inventory and bootstrap hash. The complete
license/notice files of all direct and transitive distributions remain in the
installed environment; aggregate texts must not be reduced to one SPDX label.

## Native host libraries and SSH

The native wrapper uses the matching PoMVG host's PoSDK plugin API and OpenCV
libraries. These host ABI dependencies are separate from the private Python
wheel above and are not duplicated as private native libraries by this plugin.
SSH mode uses the local system OpenSSH client; its executable and keys are not
redistributed. A remote environment must provide the supported versions above;
official CUDA build suffixes are accepted for torch/torchvision. The signed
downloadable native plugin targets macOS arm64. The local controller imports
only Python's standard library in SSH mode.

插件作者 qi Cai 负责 PoMVG 集成；LoFTR 原算法、Kornia 实现及模型权重仍属于上游。
上述版本、下载来源、哈希和完整许可证位置分别说明，运行时不会修改用户的
Python/Conda 环境。安装成功前会进行真实 CPU 模型推理检查。
