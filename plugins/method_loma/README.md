# LoMa-B for PoSDK

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

The method_loma plugin accepts data_images and returns data_features and
data_matches. Each image is extracted once. All image pairs refer to the same
immutable per-image feature indices, which can be assembled into tracks by
the existing PoSDK backend. RGB comes from the original, unoriented input image.

## Execution and installation

The signed macOS arm64 package installs an isolated **Python-standard-library
SSH controller**. It does not install PyTorch, ONNX Runtime or model weights on
the GUI host. Configure an SSH host and a compatible remote interpreter/runtime
in Configuration. OpenSSH host keys must already be verified.

The remote server uses the original authors' three pinned ONNX assets:
DaD detection, DeDoDe-G description and LoMa-B matching. Select CPU for all-CPU
execution, or CoreML for the descriptor with explicitly declared CPU
partitions; detector and matcher remain on CPU. There is no silent model or
device fallback.

A runtime provisioning helper and exact model/dependency locks are included in
the signed package's installer resources. Run provisioning **on the SSH server**;
see deps/README.md. The Marketplace client installation alone does not
provision or authorize an arbitrary remote server.

## Configuration

- ssh_target: SSH alias or user@host.
- ssh_python_executable: absolute remote interpreter or ~/ interpreter.
- runtime_root: remote directory containing models/ with the three assets.
- device: coreml or cpu; max_keypoints: 64–8192, default1280.
- resize_long_edge: detector resize, default1024; descriptor input is fixed784².
- confidence_threshold: match threshold, default0.1.
- remote_image_root: optional exact-byte image reuse, checked by size/SHA256.
- python_bin_folder: installer-owned local controller binding; the legacy
  python_executable override remains for Developer diagnostics.

## Coordinates and downstream geometry

COLMAP-style edge coordinates are shifted by -0.5 to zero-index pixel centers.
No pair-conditioned snapping, dense keypoint merging, pose estimation,
geometric inlier marking or backend refinement runs inside this frontend.
The downstream two-view estimator, track builder and pose solver remain unchanged.

## Validation

Quick Test checks native identity, ABI, platform and declared runtime files.
It does not establish matching accuracy. Real remote image inference and
standard-output parsing are recorded separately in release evidence.
Worker transport rejects traversal, links, corrupted hashes, duplicate feature
associations and partial or timed-out results.

## License

The PoSDK bridge binary uses LicenseRef-Custom (proprietary bridge). LoMa, its
LightGlue-derived matcher, ONNX Runtime, COLMAP and all other components retain
their separate upstream licenses. See THIRD_PARTY_NOTICES.md.

The first CoreML descriptor session may spend several minutes compiling its graph. Cached initialization and matching are separate stages; cold compilation is not a steady-state inference timing.
