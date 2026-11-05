# LoFTR PoMVG 插件

[English](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loftr/README.md) | 简体中文

已发布版本：**0.3.1** · [签名发布包](https://github.com/pose-only-vision/PoSDK-plugins/releases/tag/posdk-method_loftr-v0.3.1)

<!-- posdk:published-source:start -->
## 插件源码

已发布版本 **0.3.1** 的插件源码位于 [`src/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_loftr/src/)，依赖锁定及安装说明位于 [`deps/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_loftr/deps/)。

- [阅读 C++、Python 和配置源码](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_loftr/src/)
- [构建说明](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loftr/src/README.md)
- [源码文件与 SHA-256 清单](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loftr/src/source-manifest.json)

<!-- posdk:published-source:end -->

<!-- posdk:upstream-research:v1:start -->
## 原论文与原作团队

**PoSDK 集成：** qi Cai。原始算法的作者署名及上游许可证保留如下。

### LoFTR: Detector-Free Local Feature Matching with Transformers

- **原作者：** Jiaming Sun, Zehong Shen, Yuang Wang, Hujun Bao, Xiaowei Zhou.
- **团队：** Zhejiang University CAD & CG; SenseTime Research.
- **发表信息：** 2021 · CVPR.
- **论文：** [LoFTR: Detector-Free Local Feature Matching with Transformers](https://zju3dv.github.io/loftr/).
- **原作项目或实现：** [https://github.com/zju3dv/LoFTR](https://github.com/zju3dv/LoFTR).

署名依据原作项目核对于 **2026-11-01**。引用不表示原作者对 PoSDK 封装作出背书。
<!-- posdk:upstream-research:v1:end -->

**插件集成作者：** qi Cai

**联系邮箱：** <qicaiCN@gmail.com>

本插件输出原图像素坐标，并为每张图像建立共享特征编号。LoFTR 为每个图像对生成的坐标需要先完成关联，才能建立多视图轨迹。

## SSH 执行（默认）

0.3.1 默认使用 `execution_backend=ssh` 和 `ssh_target=remoteMAC`。公开包只包含这个别名，不包含维护者的主机地址、用户名、私钥或私有文件系统路径。请在自己的 `~/.ssh/config` 中配置该别名，并事先通过 OpenSSH 核验服务器主机密钥。连接采用非交互模式，严格校验主机密钥。

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

运行前，请在服务器上安装受支持的模型环境：Python 3.11.11、torch 2.2.2、torchvision 0.17.2、Kornia 0.7.4、NumPy 1.26.4 和 OpenCV 4.10.0。torch/torchvision 允许官方 CUDA 构建后缀。`ssh_python_executable` 可直接指向远端环境的 `env/bin/python3.11`；`current` 是可选的环境根目录便捷链接。

在远端 Mac 上通过 PoMVG 的签名插件安装器安装此原生插件，也会准备该环境。可在服务器运行 `posdk plugin paths --id method_loftr` 查找路径。Linux 推理服务器可以独立准备相同版本的环境；可下载的原生插件本身仍面向 macOS arm64。

本地控制程序仅使用 Python 标准库，模型导入和推理在远端执行。每次运行只上传一次图像，显示上传、匹配和下载进度，检查结果覆盖率及 SHA-256，并返回标准 `data_features` / `data_matches`。坐标转换和多视图特征关联沿用现有实现。SSH 错误不会静默触发本地推理。

`ssh_checkpoint_path` 留空时，从远端环境的 `runtime/weights` 选择并校验室内或室外权重。也可显式指定远端权重，或用已有的**本地** `checkpoint_path` 随任务上传权重；两个显式权重选项互斥。SSH 模式下，`pair_cache_dir` 和 `base_pair_cache_dir` 都是**服务器路径**；本地模式下则为本地路径。远端缓存须使用绝对路径或以 `~/` 开头的路径。共享远端缓存在任务运行期间加锁，图像标识在上传间保持稳定，以支持验证后的缓存重放。

每个任务使用独立私有目录。取消或超时会停止任务；下载结果经暂存及验证后才使用。清理只删除本次任务目录，保留显式配置的推理缓存。设置 `ssh_keep_work_dir=true` 可保留上传图像及结果用于诊断。要在本机推理，设置 `execution_backend=local`，使用签名安装器准备的环境。

## 配置

参数放在 `[method_loftr]` 中：

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

- `max_keypoints`：每个图像对的最大匹配数；`0` 保留所有合格匹配。
- `max_keypoints_per_image`：每张图像的最大共享特征数；`0` 表示不限制。
- `keypoint_merge_radius`：关联距离及精细量化大小，单位为**原图像素**。关联范围受限，不沿相邻观测链式扩散；每个图像对在两端保留互为最佳的匹配。
- `keypoint_cell_size`：原图像素单位的聚合单元大小；`0` 跟随合并半径。较大的单元对支持度最高的精细位置投票，最终关联距离仍由 `keypoint_merge_radius` 限定。
- `pair_geometry=magsac`：在关联特征编号前执行的可选诊断过滤。`geometry_threshold` 的单位为**缩放后的网络像素**。该检查只使用图像对应，不使用真值或相机位姿，后续仍需要正常的标定双视图估计。默认关闭；在目标工作流上验证后再启用。
- `magsac_post`：特征聚合后再过滤，保留原始特征编号空间。跨分支共享编号要求所有聚合设置及输入图像一致；每路匹配输出须连接其对应的特征输出。`magsac_seed` 保留所有行，并标记初步几何种子，这些标记不能证明标定相对位姿正确。
- `pair_cache_dir`：可选的未截断、无损网络输出缓存目录，要求显式设置 `checkpoint_path`。复用前须核对图像哈希、权重哈希、预处理、后端和软件包版本；重放时仍重新进行特征关联和几何验证。
- `refine_endpoint=second`：保留 Kornia 的端点顺序；`first` 沿用 HLOC 查询封装。端点顺序属于缓存契约的一部分。
- `rotation_rescue=true`：对弱支持视图关联的不可靠图像对，重试 90/180/270 度方向。只有几何共识显著改善时才采用新结果，否则保留原图像对。旋转保持粗网格相位，并将两端坐标还原到原图。默认 `false`，要求固定权重。`base_pair_cache_dir` 可指向经过验证、不可变的基线缓存；否则 worker 在新缓存目录中建立基线。旋转补救结果须使用单独的 `pair_cache_dir`。各次尝试记录在 `rotation_provenance.json`；图像对检查不替代标定双视图及多视图验证。

计时事件区分 `model_passes`、实际推理的不同图像对、主缓存命中和基线缓存命中。缓存重放不能作为新鲜推理的基准计时。

图像采用浮点灰度及面积缩放，网络尺寸为 8 的倍数，横纵方向独立缩放。像素中心按 `(network_point + 0.5) * actual_scale - 0.5` 映射回原图。

将标定双视图估计器的已验证 `data_matches` 输出接入初始轨迹构建模块，设置 `match_selection_mode=inliers_only` 和 `track_building_mode=legacy`。初始位姿引擎同样使用已验证匹配。另一路保留未截断的原始匹配，用于固定姿态后的 Fresh 重建；不要将该原始储备直接激活为初始 LiGT 求解轨迹。这些设置属于 LoFTR 工作流，不需要修改默认 SIFT 工作流。

半稠密描述的是缩放后的网络输入。更多图像对匹配不保证更一致的多视图轨迹，也不保证更多三角化三维点。

## 第三方依赖

**qi Cai 是 PoMVG 集成作者。** LoFTR 原作者为 Jiaming Sun、Zehong Shen、Yu'ang Wang、Hujun Bao 和 Xiaowei Zhou。本插件运行 Kornia 0.7.4 的 LoFTR 实现及原始 dual-softmax 模型。

| 组件 | 固定版本 | 用途 | 许可与声明 |
|---|---|---|---|
| LoFTR / Kornia | Kornia 0.7.4 | 学习型图像对应；室内和室外权重 | 保留原始 LoFTR、Kornia 署名；源码采用 Apache-2.0 |
| CPython | 3.11.11 | 独立解释器 | Python 及其分发声明 |
| PyTorch / torchvision | 2.2.2 / 0.17.2 | 模型执行 | 分发包汇总许可与声明 |
| NumPy | 1.26.4 | 数组和输出坐标 | 汇总及组件许可文本 |
| OpenCV Python headless | 4.10.0.84 | 图像处理及可选的图像对几何处理 | 封装及所含库的许可文本 |
| micromamba | 2.3.2, 安装回退工具 | 创建环境 | BSD-3-Clause 及所含声明 |
| OpenSSH | 由系统提供 | 可选的 SSH 传输 | 使用系统分发；不附带客户端二进制或密钥 |

原作者、源码链接、权重 URL/哈希及完整许可证位置见 [THIRD_PARTY_NOTICES.md](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loftr/THIRD_PARTY_NOTICES.md)。完整锁定软件包清单和安装配方见 [deps/README.md](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loftr/deps/README.md)。LoFTR 与 Kornia 是不同的上游项目，保留各自署名及 PoMVG 集成署名。

参考：

- [LoFTR，Sun 等，CVPR 2021](https://github.com/zju3dv/LoFTR)
- [HLOC 稠密 SfM 预处理和关联](https://github.com/cvg/Hierarchical-Localization/blob/master/hloc/match_dense.py)
- [Kornia LoFTR 实现](https://github.com/kornia/kornia/blob/v0.7.4/kornia/feature/loftr/loftr.py)
