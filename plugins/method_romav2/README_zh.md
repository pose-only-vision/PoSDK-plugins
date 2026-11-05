# RoMa v2 PoSDK 插件

[English](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_romav2/README.md) | 简体中文

已发布版本：**0.1.0** · [签名发布包](https://github.com/pose-only-vision/PoSDK-plugins/releases/tag/posdk-method_romav2-v0.1.0)

<!-- posdk:published-source:start -->
## 插件源码

已发布版本 **0.1.0** 的插件源码位于 [`src/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_romav2/src/)，依赖锁定及安装说明位于 [`deps/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_romav2/deps/)。

- [阅读 C++、Python 和配置源码](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_romav2/src/)
- [构建说明](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_romav2/src/README.md)
- [源码文件与 SHA-256 清单](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_romav2/src/source-manifest.json)

<!-- posdk:published-source:end -->

<!-- posdk:upstream-research:v1:start -->
## 原论文与原作团队

**PoSDK 集成：** qi Cai。原始算法的作者署名及上游许可证保留如下。

### RoMa v2: Harder Better Faster Denser Feature Matching

- **原作者：** Johan Edstedt, David Nordström, Yushan Zhang, Georg Bökman, Jonathan Astermark, Viktor Larsson, Anders Heyden, Fredrik Kahl, Mårten Wadenbäck, Michael Felsberg.
- **团队：** Linköping University; Chalmers University of Technology; University of Amsterdam; Lund University.
- **发表信息：** 2025 · arXiv 预印本。
- **论文：** [RoMa v2: Harder Better Faster Denser Feature Matching](https://arxiv.org/abs/2511.15706).
- **原作项目或实现：** [https://github.com/Parskatt/RoMaV2](https://github.com/Parskatt/RoMaV2).

匹配器采用原始预训练 **RoMa v2 v2.0.1**。DINOv3 是独立的 Meta 基础模型依赖，适用其自身许可证。插件不包含 MV-RoMa 或经过学习的 PoGT 适配器。

署名依据原作项目核对于 **2026-11-01**。引用不表示原作者对 PoSDK 封装作出背书。
<!-- posdk:upstream-research:v1:end -->

`method_romav2` 接收 `data_images`，输出标准的 `data_features` 和 `data_matches`。特征采用未经方向变换的原图网格，并携带 RGB。相机标定，以及后续位姿、轨迹和重建算法，继续由现有工作流处理。

## SSH 执行

本地控制程序使用 OpenSSH 和 Python 标准库。模型推理在明确配置的远端解释器中执行，不回退到本地。连接校验主机密钥；每个任务使用独立的私有工作目录，上传的图像、脚本及下载结果均校验 SHA-256。失败、取消或超时的任务不会提供部分输出。

远端运行环境包含固定版本的 `vendor/RoMaV2`、`vendor/DINOv3` 和 `assets/romav2.0.1.pt`，详见 [THIRD_PARTY_NOTICES.md](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_romav2/THIRD_PARTY_NOTICES.md)。在节点中配置远端解释器和运行根目录。市场客户端安装独立的标准库控制程序；运行需要已配置好的 SSH 服务器环境，客户端安装不会部署服务器环境。

## 模型、设备与坐标

支持官方的 `turbo`、`fast`、`base` 和 `precise` 预设。Apple MPS 使用 FP32，关闭 AMP 和编译；严格加载原始权重，采用官方稠密匹配及均衡采样，不使用经过训练的残差适配器。集合级特征聚合逐字节复用现有 LoFTR 实现的依赖快照，由契约测试检查，不另行维护一套实现。

RoMa 的边缘像素坐标减去 0.5，转换为 PoMVG 从零开始的像素中心坐标。采样按图像对设置随机种子。评估时应检查原生 GT 误差及实际 PLY，而不只看匹配数或相机数。

## 市场客户端与远端运行环境

签名安装器创建与版本和目标平台对应的控制程序 Python 环境，并返回稳定的 `python_bin` 绑定。GUI 所在机器不安装模型依赖或权重。`python_bin_folder` 选择受管理的控制程序；未建立绑定时，`python_executable` 保留为 Developer 覆盖项。

精确的远端源码和模型身份记录在 `deps/runtime-lock.json`。包内同时提供远端部署助手和依赖配方。ABI 或加载测试通过、真实图像推理通过，以及复现原论文基准，是不同的验证事项。
