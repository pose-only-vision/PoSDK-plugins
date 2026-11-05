# SuperPoint + LightGlue PoSDK 插件

[English](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_superpoint_lightglue/README.md) | 简体中文

已发布版本：**0.1.1** · [签名发布包](https://github.com/pose-only-vision/PoSDK-plugins/releases/tag/posdk-method_superpoint_lightglue-v0.1.1)

<!-- posdk:upstream-research:v1:start -->
## 原论文与原作团队

**PoSDK 集成：** qi Cai。原始算法的作者署名及上游许可证保留如下。

### SuperPoint: Self-Supervised Interest Point Detection and Description

- **原作者：** Daniel DeTone, Tomasz Malisiewicz, Andrew Rabinovich.
- **团队：** Magic Leap.
- **发表信息：** 2018 · CVPR Workshops.
- **论文：** [SuperPoint: Self-Supervised Interest Point Detection and Description](https://openaccess.thecvf.com/content_cvpr_2018_workshops/w9/html/DeTone_SuperPoint_Self-Supervised_Interest_CVPR_2018_paper.html).
- **原作项目或实现：** [https://github.com/magicleap/SuperPointPretrainedNetwork](https://github.com/magicleap/SuperPointPretrainedNetwork).

### LightGlue: Local Feature Matching at Light Speed

- **原作者：** Philipp Lindenberger, Paul-Edouard Sarlin, Marc Pollefeys.
- **团队：** ETH Zurich Computer Vision and Geometry Group.
- **发表信息：** 2023 · ICCV.
- **论文：** [LightGlue: Local Feature Matching at Light Speed](https://openaccess.thecvf.com/content/ICCV2023/html/Lindenberger_LightGlue_Local_Feature_Matching_at_Light_Speed_ICCV_2023_paper.html).
- **原作项目或实现：** [https://github.com/cvg/LightGlue](https://github.com/cvg/LightGlue).

署名依据原作项目核对于 **2026-11-01**。引用不表示原作者对 PoSDK 封装作出背书。
<!-- posdk:upstream-research:v1:end -->

数据集级前端：`data_images → data_features + data_matches`。每张图像只提取一次特征，所有图像对复用稳定的逐图特征编号。坐标按照上游的像素中心约定还原到原图。

## 本地与 SSH 执行

设置 `execution_backend=local`，使用安装器管理的独立运行环境。

设置 `execution_backend=ssh` 时，配置 `ssh_target`，并将 `ssh_python_executable` 指向远端匹配的安装环境。本地 `python_executable` 或受管理的 `python_bin_folder` 运行仅依赖 Python 标准库的 SSH 控制程序。`ssh_runtime_folder` 可覆盖远端运行目录；留空时使用受管理环境的相邻目录布局。

图像与签名包内的 worker 脚本上传到仅任务所有者可访问的独立工作目录。传输会检查输入、输出的 SHA-256、结果大小限制、严格的模型及源码身份、超时和逐任务清理。SSH 使用已有密钥或代理，并严格校验服务器主机密钥；模型或传输失败时，本次运行失败。

当前签名包面向 macOS arm64，远端 macOS arm64 已测试。Worker 的 Linux/CUDA 路径要求完全匹配声明的 Python、软件包版本及模型身份，使用前需要独立验证。

- `device=auto` 和 `device=cpu` 均使用 CPU。
- 显式选择 `device=mps` 时，特征提取在 CPU 上执行，LightGlue 在 MPS 上执行。
- 显式选择 `device=cuda` 时，两个阶段都要求 CUDA。

默认使用穷举图像对、每图 1280 个关键点、长边 1024、随机种子 31。增大预算前，应在相同分辨率和相同图像对图上比较。

## 封装源码获取

封装二进制的接收者可联系维护者 <qicaiCN@gmail.com>，获取 MPL-2.0 覆盖的对应封装源码。依赖源码和权重按本包所列许可证及固定版本的安装配方单独获取。

## 研究使用许可

SuperPoint 采用有限制的非商业研究许可。安装前请阅读 [THIRD_PARTY_NOTICES.md](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_superpoint_lightglue/THIRD_PARTY_NOTICES.md) 和 [deps/SUPERPOINT-LICENSE.txt](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_superpoint_lightglue/deps/SUPERPOINT-LICENSE.txt)。PoSDK 封装的许可证不覆盖或替代上游许可。

## 0.1.1 版本源码发布

插件自身的 C++、Python、CMake 和配置源码位于 [`src/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_superpoint_lightglue/src/)。固定依赖锁、公开源码引用、许可证及安装配方位于相邻的 [`deps/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_superpoint_lightglue/deps/)。签名包绑定发布时的公开文件快照；默认分支上的文档可独立更新。

独立构建时，将 `src/` 的内容复制到单独的构建工作目录，在该目录恢复相邻 `deps/`，然后使用匹配的 PoSDK Plugin SDK 配置构建。签名二进制安装器使用其内部不可变资源。SuperPoint 仍受非商业研究许可约束，源码可见性不会改变依赖许可证。
