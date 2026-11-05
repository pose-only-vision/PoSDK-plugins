# ALIKED + LightGlue PoMVG 插件

[English](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_aliked_lightglue/README.md) | 简体中文

已发布版本：**0.1.2** · [签名发布包](https://github.com/pose-only-vision/PoSDK-plugins/releases/tag/posdk-method_aliked_lightglue-v0.1.2)

<!-- posdk:published-source:start -->
## 插件源码

已发布版本 **0.1.2** 的插件源码位于 [`src/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_aliked_lightglue/src/)，依赖锁定及安装说明位于 [`deps/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_aliked_lightglue/deps/)。

- [阅读 C++、Python 和配置源码](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_aliked_lightglue/src/)
- [构建说明](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_aliked_lightglue/src/README.md)
- [源码文件与 SHA-256 清单](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_aliked_lightglue/src/source-manifest.json)

<!-- posdk:published-source:end -->

<!-- posdk:upstream-research:v1:start -->
## 原论文与原作团队

**PoSDK 集成：** qi Cai。原始算法的作者署名及上游许可证保留如下。

### ALIKED: A Lighter Keypoint and Descriptor Extraction Network via Deformable Transformation

- **原作者：** Xiaoming Zhao, Xingming Wu, Weihai Chen, Peter C. Y. Chen, Qingsong Xu, Zhengguo Li.
- **团队：** ALIKED original author team.
- **发表信息：** 2023 · IEEE Transactions on Instrumentation and Measurement.
- **论文：** [ALIKED: A Lighter Keypoint and Descriptor Extraction Network via Deformable Transformation](https://arxiv.org/abs/2304.03608).
- **原作项目或实现：** [https://github.com/Shiaoming/ALIKED](https://github.com/Shiaoming/ALIKED).

### LightGlue: Local Feature Matching at Light Speed

- **原作者：** Philipp Lindenberger, Paul-Edouard Sarlin, Marc Pollefeys.
- **团队：** ETH Zurich Computer Vision and Geometry Group.
- **发表信息：** 2023 · ICCV.
- **论文：** [LightGlue: Local Feature Matching at Light Speed](https://openaccess.thecvf.com/content/ICCV2023/html/Lindenberger_LightGlue_Local_Feature_Matching_at_Light_Speed_ICCV_2023_paper.html).
- **原作项目或实现：** [https://github.com/cvg/LightGlue](https://github.com/cvg/LightGlue).

署名依据原作项目核对于 **2026-11-01**。引用不表示原作者对 PoSDK 封装作出背书。
<!-- posdk:upstream-research:v1:end -->

**插件集成作者：** qi Cai

**联系邮箱：** <qicaiCN@gmail.com>

用于数据集级 [ALIKED](https://github.com/Shiaoming/ALIKED) 特征提取与 [LightGlue](https://github.com/cvg/LightGlue) 匹配的实验性 PoMVG 前端。接收 `data_images`，输出 `data_features` 和 `data_matches`。每张图像使用稳定的特征编号空间，使图像对匹配可组成有效的多视图轨迹。

> **发布状态：Experimental（实验性）。** 插件具有可复现的运行环境和 Strecha 测试证据，但在精度、耗时、覆盖率和点密度各项指标上，尚不能全面优于基于 SIFT 的 `PoSDKPreprocessor`，因此未标记为 Stable。

## 平台与安装

- PoMVG Factory/plugin ABI 2/2（PoSDK 2.1.1）；不兼容 ABI1 宿主。
- macOS Apple silicon（`arm64`）。
- 通过市场 `.pospkg` 安装，不内置于 PoMVG App。
- 首次安装需要联网，独立 Python/模型运行环境约 **0.8–0.9 GiB**；为慢速网络预留最多 60 分钟。
- 安装后，正常推理可离线运行。

安装器创建插件独立的 Python 3.11.11 环境，只下载按版本和 SHA-256 固定的源码及模型，并在提交 `python_bin` 绑定前执行真实的 CPU ALIKED+LightGlue 推理。插件不使用用户环境中的 Python，也不静默回退到 SIFT 或传统匹配器。

## 预设

| 预设 | `resize_long_edge` | `max_keypoints` | 用途 |
|---|---:|---:|---|
| **Balanced（默认）** | 1024 | 1280 | 侧重精度的通用配置 |
| **Fast** | 896 | 512 | 延迟和内存更低；重建更稀疏 |
| Dense（手动、实验性） | 1600 | 4096 | 更多稀疏地图点；HJ8 冒烟测试峰值 RSS 约 5.4 GB |
| Extreme（手动、仅诊断） | 1600 | 8192 | CPU/RSS 开销很高；不建议日常使用 |

其余经过验证的比较设置为：穷举图像对、`detection_threshold=0.01`、`filter_threshold=0.10`、`depth_confidence=0.95`、`width_confidence=0.99`、6 个 CPU 线程和随机种子 31。穷举匹配的成本随图像数呈二次增长，长序列可使用 `pair_mode=contiguous`。

`device=auto` 和 `device=cpu` 均选择经过验证的全 CPU 路径。显式的 `device=mps` 为实验性模式：ALIKED 留在 CPU，LightGlue 使用 Metal。固定运行环境下的现有冒烟测试显示该模式更慢，因此不会自动选择。

## 在 PoMVG 中使用

1. 从 PoMVG 插件市场安装 **ALIKED + LightGlue**。
2. 在 **SfM / Feature Extraction and Matching** 下添加该方法。
3. 将 `data_images` 输出接入其输入端口。
4. 将 `data_features` 和 `data_matches` 接入比较工作流中相同的标定双视图估计、轨迹构建及重建阶段。

`image_folder` 只用于 Developer 回退调试。发布的工作流应显式连接 `data_images`。

## 更正后的探索性测试证据

> **实验协议更正（2026-08-26）：** 旧版 README 曾将 PoSDK 的 `opencv-poengine-strecha` 工作流结果表述为 LiRP 论文工作流证据，相关说法已撤回。下列更正测试采用当前内置的 `paper-gnc-ransac-gate-poengine-strecha` 后端，属于前端消融，不是对论文原始表格的复现。

更正比较固定数据集加载器、GNC-RANSAC LiRP 估计器、图门控、旋转平均、轨迹构建、PoRobust 重建、评估及导出，只在相同节点和端口替换前端。测试在 Apple M4 Max 上使用经过验证的 CPU 路径。每项只有一次未预热的冒烟运行，因此耗时仅作描述性记录。

数值顺序为 `PoSDKPreprocessor → ALIKED + LightGlue Balanced`，误差越低越好。

| 场景 | 前端耗时 | 相对图像对覆盖率 | 相对 R / t 中位数 | 全局 R / t 中位数 | PLY 点数 |
|---|---:|---:|---:|---:|---:|
| Herz-Jesus-P8 | 11.718 → 12.892 s | 75.0% → 100% | 0.03169° / 0.06255° → 0.04942° / 0.06040° | 0.02427° / 0.004034 → 0.05094° / 0.004681 | 21,467 → 1,606 |
| entry-P10 | 11.494 → 16.961 s | 82.2% → 100% | 0.02924° / 0.04960° → 0.09730° / 0.13648° | 0.01695° / 0.004632 → 0.09260° / 0.013239 | 18,766 → 2,188 |
| fountain-P11 | 18.238 → 18.124 s | 76.4% → 94.5% | 0.04251° / 0.04816° → 0.06692° / 0.08677° | 0.03401° / 0.002506 → 0.08678° / 0.004119 | 40,980 → 2,434 |

Balanced 在三个场景中都提高了相对图像对覆盖率，但在 12 个“场景 × 位姿指标”单元中，只有一个取得更低误差。因此，这些结果不能证明其精度或耗时优于默认 SIFT 前端。

在 HJ8 上，将每图关键点预算从 1280 提高到 4096、8192 时，PLY 点数从 1606 增至 5284、9678；对应前端耗时为 12.9 s、56.6 s、223.4 s，候选方法峰值 RSS 约为 2.9 GB、5.4 GB、9.0 GB。8192 设置仍只有默认 SIFT 点数的 45.1%。在 4096 关键点时，将 LightGlue 过滤阈值从 0.10 降至 0.05，仅增加 23 个 PLY 点，且四项位姿误差中位数均变差。

原始图像 ETH3D 前端检查目前提供的是负面兼容性证据，不能作为发布效果声明。论文 ETH3D 协议使用已有对应关系评估 N 点求解器；替换为原图穷举匹配属于不同实验。在当前 `pipes` 和 `meadow` 检查中，至少一个分支未达到完整相机集合的可发布要求，或出现明显精度退化。

插件继续保持 **Experimental**。稳定的多视图编号及通常更广的图像对覆盖率，使其可作为可选的学习型前端，但现有证据不能证明跨场景或跨数据集优势。除非采用很高的关键点预算，其稀疏地图密度也明显低于默认 SIFT 前端。

## 可复现性与运行环境身份

运行环境固定为：

- Python 3.11.11、PyTorch 2.2.2、torchvision 0.17.2。
- NumPy 1.26.4、Pillow 12.3.0 RGB 解码、Kornia 0.6.12。
- LightGlue 提交 `eb42fee2d71449efb0aa5c10549752b5d75384d8`。
- ALIKED 提交 `683d7c65197395c0b3f01ebe76e1084a27e73a65`。

Worker 和原生插件均检查源码归档、规范源码树、权重、Python 及软件包版本、Pillow 解码器身份、请求及实际设备、模型哈希。日志记录提取、匹配、worker 及前端墙钟耗时、峰值 RSS、特征数、图像对数，以及完整运行环境身份。Profiler 结构化标签还记录实际预设参数，便于区分不同配置的运行。

## 限制

- 当前安装包仅面向 macOS `arm64`。
- 当前输出包含点坐标和稳定编号，不包含学习型描述子张量；后续模块使用 LightGlue 匹配。
- 即使相机位姿误差改善，稀疏学习型特征产生的重建点也可能明显少于 SIFT 基线。
- 不提供稠密或半稠密对应的规范化处理。
- 独立运行环境有意不安装 OpenCV。遇错即停止的 shim 只满足 LightGlue 对未使用的可选辅助模块的即时导入，不实现替代图像或特征路径。
- 独立运行环境缺失或身份校验失败时，插件停止，不回退到其他算法。

## 第三方依赖

qi Cai 编写并维护 PoMVG 集成。ALIKED、LightGlue、模型权重及 Python 库保留其上游作者署名与许可证。

| 组件 | 固定版本或提交 | 用途 | 上游许可与声明 |
|---|---|---|---|
| [ALIKED](https://github.com/Shiaoming/ALIKED/tree/683d7c65197395c0b3f01ebe76e1084a27e73a65) | `683d7c65197395c0b3f01ebe76e1084a27e73a65`; ALIKED-n16 | 特征提取及权重 | BSD-3-Clause; Xiaoming Zhao et al. |
| [LightGlue](https://github.com/cvg/LightGlue/tree/eb42fee2d71449efb0aa5c10549752b5d75384d8) | `eb42fee2d71449efb0aa5c10549752b5d75384d8`; `v0.1_arxiv` checkpoint | 学习型特征匹配及 ALIKED 实现 | Apache-2.0; Philipp Lindenberger, Paul-Edouard Sarlin, Marc Pollefeys / ETH Zurich |
| [CPython](https://github.com/python/cpython/tree/v3.11.11) | 3.11.11 | 独立解释器 | Python 许可及所含声明 |
| [PyTorch](https://github.com/pytorch/pytorch/tree/v2.2.2) / [torchvision](https://github.com/pytorch/vision/tree/v0.17.2) | 2.2.2 / 0.17.2 | 张量与模型运行环境 | 对应分发包中的汇总许可与声明 |
| [NumPy](https://github.com/numpy/numpy/tree/v1.26.4) | 1.26.4 | 数组运算 | 汇总分发及组件许可文本 |
| [Pillow](https://github.com/python-pillow/Pillow) | 12.3.0 | RGB 图像解码 | MIT-CMU 及所含库的声明 |
| [Kornia](https://github.com/kornia/kornia/tree/v0.6.12) | 0.6.12 | 固定前端使用的几何工具 | Apache-2.0 |
| [micromamba](https://github.com/mamba-org/mamba) | 2.3.2, 本地环境创建工具不可用时 | 安装引导 | BSD-3-Clause |

安装时创建插件独立环境，校验源码、模型及软件包的 SHA-256，不修改用户 Python/Conda 环境。插件使用 Pillow 解码图像，运行环境不安装 OpenCV。PoMVG 集成不改变 ALIKED 和 LightGlue 的原始作者署名及许可。

上游作者、权重 URL 及许可位置见 [THIRD_PARTY_NOTICES.md](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_aliked_lightglue/THIRD_PARTY_NOTICES.md)；完整版本和哈希清单及安装配方见 [deps/README.md](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_aliked_lightglue/deps/README.md)。
