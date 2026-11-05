# LoMa-B PoSDK 插件

[English](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loma/README.md) | 简体中文

已发布版本：**0.1.0** · [签名发布包](https://github.com/pose-only-vision/PoSDK-plugins/releases/tag/posdk-method_loma-v0.1.0)

<!-- posdk:published-source:start -->
## 插件源码

已发布版本 **0.1.0** 的插件源码位于 [`src/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_loma/src/)，依赖锁定及安装说明位于 [`deps/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_loma/deps/)。

- [阅读 C++、Python 和配置源码](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_loma/src/)
- [构建说明](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loma/src/README.md)
- [源码文件与 SHA-256 清单](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loma/src/source-manifest.json)

<!-- posdk:published-source:end -->

<!-- posdk:upstream-research:v1:start -->
## 原论文与原作团队

**PoSDK 集成：** qi Cai。原始算法的作者署名及上游许可证保留如下。

### LoMa: Local Feature Matching Revisited

- **原作者：** David Nordström, Johan Edstedt, Georg Bökman, Jonathan Astermark, Anders Heyden, Viktor Larsson, Mårten Wadenbäck, Michael Felsberg, Fredrik Kahl.
- **团队：** Chalmers University of Technology; Linköping University; University of Amsterdam; Lund University.
- **发表信息：** 2026 · ECCV 2026.
- **论文：** [LoMa: Local Feature Matching Revisited](https://arxiv.org/abs/2604.04931).
- **原作项目或实现：** [https://github.com/davnords/LoMa](https://github.com/davnords/LoMa).

本插件当前提供 **LoMa-B / DaD + DeDoDe-G**，使用原作者发布的 ONNX 资源及兼容 COLMAP 的适配器。当前未提供 LoMa-L、LoMa-G 或 MVLoMa 支持。

署名依据原作项目核对于 **2026-11-01**。引用不表示原作者对 PoSDK 封装作出背书。
<!-- posdk:upstream-research:v1:end -->

`method_loma` 接收 `data_images`，输出 `data_features` 和 `data_matches`。每张图像只提取一次特征；所有图像对引用同一套不可变的逐图特征编号，现有 PoSDK 后端可据此建立轨迹。RGB 来自未经方向变换的原始输入图像。

## 执行与安装

签名的 macOS arm64 包安装独立的、**仅依赖 Python 标准库的 SSH 控制程序**。GUI 所在机器不安装 PyTorch、ONNX Runtime 或模型权重。在 Configuration 中配置 SSH 主机及兼容的远端解释器和运行环境；须事先通过 OpenSSH 验证服务器主机密钥。

远端使用原作者发布并固定版本的三个 ONNX 资源：DaD 检测、DeDoDe-G 描述和 LoMa-B 匹配。选择 CPU 时，所有阶段均在 CPU 上运行；选择 CoreML 时，描述子使用 CoreML，并显式声明 CPU 分区，检测器和匹配器仍使用 CPU。模型或设备不会静默回退。

签名包的安装资源包含运行环境部署助手，以及精确的模型和依赖锁。部署助手须在 **SSH 服务器上**执行，详见 [deps/README.md](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loma/deps/README.md)。仅安装市场客户端插件不会为任意远端服务器部署环境或授予访问权限。

## 配置

- `ssh_target`：SSH 别名或 `user@host`。
- `ssh_python_executable`：远端解释器的绝对路径，或以 `~/` 开头的路径。
- `runtime_root`：远端运行目录，其 `models/` 中包含上述三个资源。
- `device`：`coreml` 或 `cpu`。
- `max_keypoints`：64–8192，默认 1280。
- `resize_long_edge`：检测器缩放长边，默认 1024；描述子的输入固定为 784²。
- `confidence_threshold`：匹配置信度阈值，默认 0.1。
- `remote_image_root`：可选的远端图像复用目录，通过文件大小和 SHA-256 确保逐字节一致。
- `python_bin_folder`：安装器管理的本地控制程序绑定；旧的 `python_executable` 覆盖项保留供 Developer 诊断使用。

## 坐标与后续几何处理

COLMAP 的边缘坐标减去 0.5，转换为从零开始的像素中心坐标。前端内部不执行依图像对调整坐标、稠密关键点合并、位姿估计、几何内点标记或后端优化。后续双视图估计、轨迹构建和位姿求解继续使用工作流中的现有模块。

## 验证

Quick Test 检查原生插件身份、ABI、平台和声明的运行文件；匹配精度需要另行验证。真实远端图像推理和标准输出解析记录在独立的发布证据中。Worker 传输拒绝路径穿越、链接、哈希损坏、重复特征关联，以及不完整或超时的结果。

## 许可

PoSDK 桥接二进制采用 `LicenseRef-Custom`（专有桥接许可）。LoMa、其基于 LightGlue 的匹配器、ONNX Runtime、COLMAP 及其他组件保留各自上游许可证，详见 [THIRD_PARTY_NOTICES.md](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_loma/THIRD_PARTY_NOTICES.md)。

首次创建 CoreML 描述子会话可能需要数分钟编译计算图。缓存初始化和匹配是不同阶段；冷启动编译时间不代表稳态推理时间。
