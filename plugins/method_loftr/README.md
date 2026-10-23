# LoFTR matching

This plugin outputs original-image pixel coordinates and a shared feature ID
space for each image. Pair-conditioned LoFTR coordinates must be associated
before building multiview tracks.

## SSH execution (default)

Version 0.3.0 defaults to `execution_backend=ssh` and `ssh_target=remoteMAC`.
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
Mac with PoSDK's signed installer also prepares that environment. Use
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

# LoFTR 匹配

0.3.0 默认通过 `remoteMAC` SSH 别名执行。连接使用本机 OpenSSH 配置、密钥或
代理，严格检查服务器主机密钥；公开包不包含维护者的私有服务器地址或私钥。
`ssh_python_executable` 填写远端锁定版本的 LoFTR 环境中的 Python，
`ssh_work_dir` 是服务器工作目录，`ssh_checkpoint_path` 可选指定服务器权重。
默认会从远端 Python 环境的 `runtime/weights` 选择并核对室内或室外模型。
本机控制程序不导入模型，运行记录显示上传、匹配和下载进度，失败不会转到本地推理。
下载结果通过完整性和 SHA-256 校验后，仍输出标准特征和匹配数据。
超时或取消会停止本次任务，默认清理本次上传图像和结果；显式缓存保留。
SSH 模式下，匹配缓存和基础缓存参数均填写服务器路径。
需要本地推理时，将 `execution_backend` 改为 `local`。

在 `[method_loftr]` 中设置参数。默认长边为 1024，置信度为 0.2；
`max_keypoints=0` 取消每对图像的匹配数量上限，
`max_keypoints_per_image=0` 取消每张图像的共享特征数量上限。

插件为每张图像建立共享特征编号，输出原图像素坐标。
`keypoint_merge_radius` 使用原图像素单位；`geometry_threshold` 使用缩放后的网络像素单位。
将双视图估计的已验证匹配输出连接到初始轨迹构建模块，并设置
`match_selection_mode=inliers_only`、`track_building_mode=legacy`。
初始位姿模块同样使用已验证匹配；原始匹配另接到固定姿态后的 Fresh 重建，
不要将原始匹配储备直接激活为初始 LiGT 求解轨迹。
`pair_geometry=magsac` 仅为可选诊断项，默认关闭。
`magsac_post` 在特征关联后筛选，保留原始特征编号空间；跨分支复用编号时，
输入图像与所有关联参数必须相同。两条分支可使用相同的逐场景缓存目录，避免重复网络推理。

设置 `checkpoint_path` 和 `pair_cache_dir` 可保存未截断的网络匹配。
复用前会核对图像、模型和预处理信息；复用时仍重新进行几何筛选与特征关联。

`refine_endpoint=second` 沿用 Kornia 的端点顺序；`first` 使用 HLOC 查询端顺序，
两种模式分别校验缓存。`rotation_rescue=true` 对弱支持视图关联的不可靠图像对
进行 90/180/270 度重试，只有几何支持显著改善时才采用新匹配；否则保留原结果。
旋转时保持粗网格对齐，并将坐标还原到原图。该选项默认关闭，须指定固定权重。
可用 `base_pair_cache_dir` 复用原方向缓存，重试结果须使用另一个 `pair_cache_dir`。
各次重试记录在 `rotation_provenance.json`，后续仍需进行标定双视图和多视图验证。

半稠密是相对于网络输入图像而言，最终三维点数还取决于多视图关联、几何一致性和三角化。

References:

- [LoFTR, Sun et al., CVPR 2021](https://github.com/zju3dv/LoFTR)
- [HLOC dense SfM preprocessing and association](https://github.com/cvg/Hierarchical-Localization/blob/master/hloc/match_dense.py)
- [Kornia LoFTR implementation](https://github.com/kornia/kornia/blob/v0.7.4/kornia/feature/loftr/loftr.py)
