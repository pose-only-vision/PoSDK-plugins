# SuperPoint + LightGlue for PoSDK

<!-- posdk:upstream-research:v1:start -->
## Original research and authors / 原论文与原作团队

**PoSDK integration / 插件封装：** qi Cai. Original algorithm authorship and upstream licenses remain with the authors below.

### SuperPoint: Self-Supervised Interest Point Detection and Description

- **Authors / 原作者：** Daniel DeTone, Tomasz Malisiewicz, Andrew Rabinovich.
- **Team / 团队：** Magic Leap.
- **Publication / 论文：** 2018 · CVPR Workshops.
- **Paper / 原文：** [SuperPoint: Self-Supervised Interest Point Detection and Description](https://openaccess.thecvf.com/content_cvpr_2018_workshops/w9/html/DeTone_SuperPoint_Self-Supervised_Interest_CVPR_2018_paper.html).
- **Official project or implementation / 原作项目或实现：** [https://github.com/magicleap/SuperPointPretrainedNetwork](https://github.com/magicleap/SuperPointPretrainedNetwork).

### LightGlue: Local Feature Matching at Light Speed

- **Authors / 原作者：** Philipp Lindenberger, Paul-Edouard Sarlin, Marc Pollefeys.
- **Team / 团队：** ETH Zurich Computer Vision and Geometry Group.
- **Publication / 论文：** 2023 · ICCV.
- **Paper / 原文：** [LightGlue: Local Feature Matching at Light Speed](https://openaccess.thecvf.com/content/ICCV2023/html/Lindenberger_LightGlue_Local_Feature_Matching_at_Light_Speed_ICCV_2023_paper.html).
- **Official project or implementation / 原作项目或实现：** [https://github.com/cvg/LightGlue](https://github.com/cvg/LightGlue).

Attribution checked against the original projects on **2026-11-01**. Citation does not imply endorsement of the PoSDK wrapper.
<!-- posdk:upstream-research:v1:end -->

Dataset-level frontend: data_images -> data_features + data_matches. Each image
is extracted once; every pair reuses its stable feature indices. Coordinates
are restored to the original image with the upstream pixel-center convention.

Choose execution_backend=local for the installer-managed isolated runtime.
For execution_backend=ssh, configure ssh_target and ssh_python_executable from
a matching remote installation. The local python_executable (or managed
python_bin_folder) runs a standard-library SSH controller. ssh_runtime_folder
may override the remote runtime; leave it empty for the managed sibling layout.
Images and the signed worker are uploaded to an owner-only per-job directory.
Input and output SHA-256 checks, bounded results, strict model/source identities,
timeouts and per-job cleanup are required. SSH uses existing keys/agent and
strict host-key checking; model or transport failures fail the run.

The initial signed package targets macOS arm64. Remote macOS arm64 is tested;
Linux/CUDA support in the worker requires the exact declared Python/package
versions and model identities and must be independently validated before use.
auto/cpu use CPU. Explicit mps keeps the extractor on CPU and runs LightGlue
on MPS; cuda explicitly requires CUDA for both stages.

Defaults: exhaustive pairs, 1280 keypoints, long edge 1024, seed 31. Run a
same-resolution/same-pair-graph comparison before selecting a larger budget.

## Wrapper source availability

Recipients of the wrapper binary can obtain the corresponding MPL-2.0-covered
wrapper source from the maintainer at qicaiCN@gmail.com. Dependency sources and weights
are obtained separately under the licenses and pinned recipes in this package.

## Research license

SuperPoint has a restrictive noncommercial research license. Read
THIRD_PARTY_NOTICES.md and deps/SUPERPOINT-LICENSE.txt before installation.
The PoSDK wrapper license does not override the upstream license.

## Source publication for 0.1.1

Plugin-owned C++, Python, CMake and configuration source is published in `src/`.
Pinned dependency locks, public source references, licenses and installer recipes
are published in sibling `deps/`. The signed package binds this exact public file
snapshot; no other plugin or PoSDK project history is included.

For a standalone source build, copy the contents of `src/` to a separate build
workspace and restore sibling `deps/` there before configuring with the matching
PoSDK Plugin SDK. The signed binary installer uses its own immutable resources.
SuperPoint remains subject to its noncommercial research license; publication
does not change the dependency licenses.
