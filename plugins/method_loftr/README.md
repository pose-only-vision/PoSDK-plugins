# LoFTR for PoMVG

English | [简体中文](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loftr/README_zh.md)

Published version: **0.3.1** · [Signed release](https://github.com/pose-only-vision/PoSDK-plugins/releases/tag/posdk-method_loftr-v0.3.1)

<!-- posdk:published-source:start -->
## Plugin source

The plugin source for published version **0.3.1** is in [`src/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_loftr/src/). Dependency locks and installation instructions are in [`deps/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_loftr/deps/).

- [Browse C++, Python and configuration source](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_loftr/src/)
- [Build instructions](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loftr/src/README.md)
- [Source file and SHA-256 inventory](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loftr/src/source-manifest.json)

<!-- posdk:published-source:end -->


<!-- posdk:upstream-research:v1:start -->
## Original research and authors

**PoSDK integration:** qi Cai. Original algorithm authorship and upstream licenses remain with the authors below.

### LoFTR: Detector-Free Local Feature Matching with Transformers

- **Authors:** Jiaming Sun, Zehong Shen, Yuang Wang, Hujun Bao, Xiaowei Zhou.
- **Team:** Zhejiang University CAD & CG; SenseTime Research.
- **Publication:** 2021 · CVPR.
- **Paper:** [LoFTR: Detector-Free Local Feature Matching with Transformers](https://zju3dv.github.io/loftr/).
- **Official project or implementation:** [https://github.com/zju3dv/LoFTR](https://github.com/zju3dv/LoFTR).

Attribution checked against the original projects on **2026-11-01**. Citation does not imply endorsement of the PoSDK wrapper.
<!-- posdk:upstream-research:v1:end -->

**Plugin integration author:** qi Cai

**Contact:** <qicaiCN@gmail.com>

This plugin outputs original-image pixel coordinates and a shared feature ID
space for each image. Pair-conditioned LoFTR coordinates must be associated
before building multiview tracks.

## SSH execution (default)

Version 0.3.1 defaults to `execution_backend=ssh` and `ssh_target=remoteMAC`.
The public package contains this alias, not a maintainer's host address,
username, private key, or private filesystem paths. Configure the alias in
your own `~/.ssh/config` and verify the server's host key with OpenSSH first.
The connection is noninteractive and retains strict host-key verification.

```ini
[method_loftr]
execution_backend=ssh
ssh_target=remoteMAC
ssh_port=0
ssh_identity_file=
ssh_python_executable=~/.local/share/PoSDK/LoFTR/current/bin/python3.11
ssh_work_dir=~/.cache/PoSDK/LoFTR
ssh_checkpoint_path=
ssh_run_timeout=3600
ssh_keep_work_dir=false
```

Install the supported model environment on the server before running. Its
versions are Python 3.11.11, torch 2.2.2, torchvision 0.17.2, Kornia 0.7.4,
NumPy 1.26.4, and OpenCV 4.10.0. An official CUDA build suffix is accepted for
torch/torchvision. `ssh_python_executable` can point directly at that remote
environment's `env/bin/python3.11`; `current` is an optional convenience link
to the environment root. Installing this native macOS plugin on the remote
Mac with PoMVG's signed plugin installer also prepares that environment. Use
`posdk plugin paths --id method_loftr` on the server to locate it. A Linux
inference server can use an independently prepared environment with these
same versions; the downloadable native plugin itself targets macOS arm64.

The local controller uses only Python's standard library; model imports and
inference run on the remote host. A run uploads the images once, reports
upload/matching/download progress, checks result coverage and SHA-256 hashes,
and returns the normal `data_features` / `data_matches` outputs. Existing
coordinate conversion and multiview feature association are unchanged.
Errors remain SSH errors and never silently trigger local inference.

Empty `ssh_checkpoint_path` selects the hash-verified indoor/outdoor checkpoint
in the remote environment's `runtime/weights`. Alternatively choose a remote
checkpoint there, or set the existing **local** `checkpoint_path` to upload a
file with this job. These two explicit checkpoint options are mutually
exclusive. `pair_cache_dir` and `base_pair_cache_dir` refer to **server** paths
in SSH mode and local paths in local mode; remote caches require absolute or
`~/` paths. Shared remote caches are locked while a job runs. Cache image
identifiers remain stable across uploads, allowing validated cache replay.

Job folders are private and independent. Cancellation/timeout stops the job;
downloaded results are staged and verified before use. Cleanup removes only
that job, retaining explicitly configured inference caches. Set
`ssh_keep_work_dir=true` to retain uploaded images and results for diagnostics.
To run inference on this computer, select `execution_backend=local` and use
the environment prepared by the signed installer.

## Configuration

Put options in `[method_loftr]`:

```ini
[method_loftr]
model_type=outdoor
resize_max=1024
confidence_threshold=0.2
max_keypoints=0
max_keypoints_per_image=0
keypoint_merge_radius=1.0
pair_geometry=none
geometry_threshold=1.0
```

- `max_keypoints`: maximum matches **per pair**; `0` keeps all eligible matches.
- `max_keypoints_per_image`: maximum shared features per image; `0` is unlimited.
- `keypoint_merge_radius`: association distance and fine quantization size in
  **original-image pixels**. Assignments are bounded and do not chain through
  neighboring observations. Each pair retains mutual best matches at both ends.
- `keypoint_cell_size`: size of the aggregation cell in original pixels;
  `0` follows the merge radius. Larger cells vote for the most-supported fine
  location. The final assignment distance still uses `keypoint_merge_radius`.
- `pair_geometry=magsac`: optional diagnostic filter before associating feature IDs.
  `geometry_threshold` is in **resized network pixels**. This check uses image
  correspondences, without ground truth or camera poses. The workflow still
  needs its normal calibrated two-view estimation stage. Keep this optional
  prefilter off unless validated for the target workflow.
- `magsac_post` applies the pair filter after aggregation, preserving the raw
  feature ID space. Two branches share IDs only when all aggregation settings
  and input images agree; connect each matches output to its matching features.
  `magsac_seed` retains all rows and marks preliminary geometric seeds. These
  preliminary flags do not certify a calibrated relative pose.
- `pair_cache_dir`: optional directory for uncapped, lossless network output.
  It requires an explicit `checkpoint_path`. Image hashes, checkpoint hash,
  preprocessing, backend and package versions must agree before reuse.
  Association and geometric verification run again on cache replay.
- `refine_endpoint=second` preserves Kornia's ordering. `first` follows the
  HLOC query wrapper. Endpoint ordering is part of the cache contract.
- `rotation_rescue=true` retries 90/180/270-degree orientations on unreliable
  pairs incident to weakly supported views. The retry must substantially improve
  geometric consensus; otherwise the original pair is kept. Rotations preserve
  the coarse-grid phase and map both endpoints back to the original image.
  This option defaults to `false` and requires a pinned checkpoint.
  `base_pair_cache_dir` may reference a validated, immutable baseline cache;
  otherwise the worker builds one below the new cache directory. Use a separate
  `pair_cache_dir` for rescued results. `rotation_provenance.json` records each
  attempted orientation. These pair checks do not replace calibrated pose or
  multiview validation.

Timing events distinguish `model_passes`, uniquely inferred pairs, primary cache
hits, and baseline cache hits. Cache replay is not a fresh inference benchmark.

Images use float grayscale area resizing, dimensions divisible by eight, and
independent horizontal/vertical scales. Pixel centers map back as
`(network_point + 0.5) * actual_scale - 0.5`.

Connect the calibrated two-view estimator's verified `data_matches` output
to the initial track builder. Set `match_selection_mode=inliers_only` and
`track_building_mode=legacy` there. Pass the verified matches to the initial
pose engine as well. Retain the uncapped raw matches on a separate branch for
fixed-pose Fresh reconstruction; do not activate that raw reservoir as the
initial LiGT solver tracks. These settings belong to the LoFTR workflow and
do not require changing the default SIFT workflow.

Semi-dense matching describes the resized network input. More pair matches do
not guarantee more consistent multiview tracks or triangulated 3D points.


## Third-party dependencies

**qi Cai is the PoMVG integration author.** The original LoFTR authors are
Jiaming Sun, Zehong Shen, Yu'ang Wang, Hujun Bao and Xiaowei Zhou. This plugin
runs Kornia 0.7.4's LoFTR implementation and the original dual-softmax models.

| Component | Fixed version | Role | License / notice |
|---|---|---|---|
| LoFTR / Kornia | Kornia 0.7.4 | Learned image correspondence; indoor and outdoor checkpoints | Original LoFTR and Kornia attribution; Apache-2.0 source licenses |
| CPython | 3.11.11 | Isolated interpreter | Python and bundled notices |
| PyTorch / torchvision | 2.2.2 / 0.17.2 | Model execution | Aggregate distribution licenses and notices |
| NumPy | 1.26.4 | Arrays and output coordinates | Aggregate and component license texts |
| OpenCV Python headless | 4.10.0.84 | Image processing and optional pair geometry | Wrapper and bundled-library license texts |
| micromamba | 2.3.2, installation fallback | Environment creation | BSD-3-Clause and bundled notices |
| OpenSSH | System-provided | Optional SSH transport | System distribution; no client binary or key is bundled |

See [THIRD_PARTY_NOTICES.md](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loftr/THIRD_PARTY_NOTICES.md) for original authors,
source links, checkpoint URLs/hashes and full license locations. The complete
locked package inventory and installation recipe are in
[deps/README.md](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loftr/deps/README.md). LoFTR and Kornia are separate upstream
projects; their authorship is preserved alongside the PoMVG integration credit.

References:

- [LoFTR, Sun et al., CVPR 2021](https://github.com/zju3dv/LoFTR)
- [HLOC dense SfM preprocessing and association](https://github.com/cvg/Hierarchical-Localization/blob/master/hloc/match_dense.py)
- [Kornia LoFTR implementation](https://github.com/kornia/kornia/blob/v0.7.4/kornia/feature/loftr/loftr.py)
