# Third-party notices

PoSDK integration: qi Cai. Upstream algorithm authorship is preserved.

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
# Upstream provenance

PoMVG integration maintainer: qi Cai. Upstream model authorship is preserved.

- RoMa v2: Johan Edstedt and the coauthors in the upstream README;
  https://github.com/Parskatt/RoMaV2 ; MIT (Copyright 2025 Johan Edstedt).
  Revision `95c9968145c8906b7b59383258e9f73b02853d89`, model v2.0.1:
  https://github.com/Parskatt/RoMaV2/releases/download/v2.0.1/romav2.0.1.pt .
- DINOv3: Meta and upstream contributors;
  https://github.com/facebookresearch/dinov3 ; revision
  `adc254450203739c8149213a7a69d8d905b4fcfa`. The separate upstream
  `LICENSE.md` applies; DINOv3 is not covered by RoMa's MIT license.
- PyTorch, torchvision, NumPy, Pillow and einops retain all upstream licenses
  and notices in their installed distributions. Runtime versions and source
  and model hashes are recorded by each successful worker run.

This development plugin does not redistribute model weights or DINOv3. The
explicitly selected remote runtime supplies them. A future downloadable
package requires complete license texts and a locked isolated installer.
