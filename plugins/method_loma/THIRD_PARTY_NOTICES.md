# Third-party notices

PoSDK integration: qi Cai. Upstream algorithm authorship is preserved.

<!-- posdk:upstream-research:v1:start -->
## Original research and authors / 原论文与原作团队

**PoSDK integration / 插件封装：** qi Cai. Original algorithm authorship and upstream licenses remain with the authors below.

### LoMa: Local Feature Matching Revisited

- **Authors / 原作者：** David Nordström, Johan Edstedt, Georg Bökman, Jonathan Astermark, Anders Heyden, Viktor Larsson, Mårten Wadenbäck, Michael Felsberg, Fredrik Kahl.
- **Team / 团队：** Chalmers University of Technology; Linköping University; University of Amsterdam; Lund University.
- **Publication / 论文：** 2026 · ECCV 2026.
- **Paper / 原文：** [LoMa: Local Feature Matching Revisited](https://arxiv.org/abs/2604.04931).
- **Official project or implementation / 原作项目或实现：** [https://github.com/davnords/LoMa](https://github.com/davnords/LoMa).

This plugin currently exposes **LoMa-B / DaD + DeDoDe-G**, using ONNX assets published by the original authors and a COLMAP-compatible adapter. It does not advertise LoMa-L, LoMa-G or MVLoMa support.

Attribution checked against the original projects on **2026-11-01**. Citation does not imply endorsement of the PoSDK wrapper.
<!-- posdk:upstream-research:v1:end -->

## Separate components and licenses

- LoMa source and DaD/DeDoDe components: original authors; MIT except the
  LightGlue-derived matcher, which retains Apache-2.0.
- ONNX assets are retrieved from the original authors' davnords/storage
  release and SHA256-verified; no model weights are embedded in the plugin.
- COLMAP: BSD-3-Clause. The independent Python adapter follows the pinned
  LoMa ONNX input/output and Bitmap resampling contract.
- ONNX Runtime: Microsoft and contributors; MIT. CoreML: Apple platform framework.
- Python, NumPy, Pillow, pycolmap and their dependencies retain their
  individual distribution notices in the remote isolated environment.
