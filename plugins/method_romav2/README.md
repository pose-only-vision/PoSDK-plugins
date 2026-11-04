# RoMa v2 for PoSDK

<!-- posdk:published-source:start -->
## 插件源码 / Plugin source

当前已发布版本 **0.1.0** 的插件源码在 [`src/`](src/)；外部依赖锁定与安装说明在 [`deps/`](deps/)。

- [阅读 C++、Python 和配置源码](src/)
- [构建说明](src/README.md)
- [源码文件与 SHA-256 清单](src/source-manifest.json)

<!-- posdk:published-source:end -->


<!-- posdk:upstream-research:v1:start -->
## Original research and authors / 原论文与原作团队

**PoSDK integration / 插件封装：** qi Cai. Original algorithm authorship and upstream licenses remain with the authors below.

### RoMa v2: Harder Better Faster Denser Feature Matching

- **Authors / 原作者：** Johan Edstedt, David Nordström, Yushan Zhang, Georg Bökman, Jonathan Astermark, Viktor Larsson, Anders Heyden, Fredrik Kahl, Mårten Wadenbäck, Michael Felsberg.
- **Team / 团队：** Linköping University; Chalmers University of Technology; University of Amsterdam; Lund University.
- **Publication / 论文：** 2025 · arXiv preprint.
- **Paper / 原文：** [RoMa v2: Harder Better Faster Denser Feature Matching](https://arxiv.org/abs/2511.15706).
- **Official project or implementation / 原作项目或实现：** [https://github.com/Parskatt/RoMaV2](https://github.com/Parskatt/RoMaV2).

The matcher is the original pretrained **RoMa v2 v2.0.1**. DINOv3 is a separate Meta foundation-model dependency with its own license. MV-RoMa and learned PoGT adapters are not part of this plugin.

Attribution checked against the original projects on **2026-11-01**. Citation does not imply endorsement of the PoSDK wrapper.
<!-- posdk:upstream-research:v1:end -->

`method_romav2` accepts `data_images` and returns standard `data_features` and
`data_matches`. Features use the original, unoriented image grid and carry RGB.
Camera calibration and subsequent pose/track/reconstruction algorithms remain
in the existing workflow.

The local controller uses OpenSSH and Python's standard library; model inference
runs on the explicitly configured remote interpreter, without local fallback.
Host keys are checked. Each job has an exclusive private directory; uploaded
images/scripts and downloaded results are SHA-256 verified. Failed, cancelled
or timed-out jobs cannot supply partial outputs.

The remote runtime contains pinned `vendor/RoMaV2`, `vendor/DINOv3`, and
`assets/romav2.0.1.pt`; see `THIRD_PARTY_NOTICES.md`. Configure its interpreter
and root in the node. The Marketplace client installs a private standard-library controller. A configured
SSH server runtime is required; client installation does not provision a server.

Official `turbo`, `fast`, `base`, and `precise` presets are available. Apple MPS
uses FP32 (AMP and compilation disabled), strictly loaded original weights,
official dense matching and balanced sampling, without a trained residual
adapter. Collection-wide aggregation is a verbatim dependency snapshot of the existing
LoFTR implementation, checked by the contract tests rather than maintained as
a separate implementation.

RoMa's edge-coordinate pixels are shifted by -0.5 to PoMVG's zero-index pixel
centers. Sampling is seeded per pair. Assess native GT errors and the actual
PLY rather than only match or camera counts.

## Marketplace client and remote runtime

The signed installer creates a version- and target-specific controller Python
environment and returns the stable python_bin binding. It installs no model
dependencies or weights on the GUI host. The python_bin_folder selects that
managed controller; python_executable remains a Developer override
when no binding is present.

The exact remote source/model identity is in deps/runtime-lock.json;
the remote provisioning helper and dependency recipe accompany the package.
A successful ABI/load test is separate from real-image inference and from
reproducing the original paper's benchmarks.
