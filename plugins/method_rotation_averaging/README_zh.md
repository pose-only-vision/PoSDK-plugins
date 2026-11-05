# Rotation Averaging PoMVG 插件

[English](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_rotation_averaging/README.md) | 简体中文

已发布版本：**0.1.1** · [签名发布包](https://github.com/pose-only-vision/PoSDK-plugins/releases/tag/posdk-method_rotation_averaging-v0.1.1)

<!-- posdk:published-source:start -->
## 插件源码

已发布版本 **0.1.1** 的插件源码位于 [`src/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_rotation_averaging/src/)，依赖锁定及安装说明位于 [`deps/`](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_rotation_averaging/deps/)。

- [阅读 C++、Python 和配置源码](https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_rotation_averaging/src/)
- [构建说明](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_rotation_averaging/src/README.md)
- [源码文件与 SHA-256 清单](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_rotation_averaging/src/source-manifest.json)

<!-- posdk:published-source:end -->

<!-- posdk:upstream-research:v1:start -->
## 原论文与原作团队

**PoSDK 集成：** qi Cai。原始算法的作者署名及上游许可证保留如下。

### Efficient and Robust Large-Scale Rotation Averaging

- **原作者：** Avishek Chatterjee, Venu Madhav Govindu.
- **团队：** Indian Institute of Science, Bengaluru.
- **发表信息：** 2013 · ICCV.
- **论文：** [Efficient and Robust Large-Scale Rotation Averaging](https://openaccess.thecvf.com/content_iccv_2013/html/Chatterjee_Efficient_and_Robust_2013_ICCV_paper.html).
- **原作项目或实现：** [https://ee.iisc.ac.in/cvlab/research/rotaveraging/](https://ee.iisc.ac.in/cvlab/research/rotaveraging/).

### Hybrid Rotation Averaging: A Fast and Robust Rotation Averaging Approach

- **原作者：** Yu Chen, Ji Zhao, Laurent Kneip.
- **团队：** GraphOptim original author team.
- **发表信息：** 2021 · CVPR.
- **论文：** [Hybrid Rotation Averaging: A Fast and Robust Rotation Averaging Approach](https://openaccess.thecvf.com/content/CVPR2021/html/Chen_Hybrid_Rotation_Averaging_A_Fast_and_Robust_Rotation_Averaging_Approach_CVPR_2021_paper.html).
- **原作项目或实现：** [https://github.com/AIBluefisher/GraphOptim](https://github.com/AIBluefisher/GraphOptim).

Chatterjee 与 GraphOptim 是两套不同实现。Chatterjee 子插件由宿主提供，GraphOptim 从固定版本的上游源码安装。

署名依据原作项目核对于 **2026-11-01**。引用不表示原作者对 PoSDK 封装作出背书。
<!-- posdk:upstream-research:v1:end -->

**插件集成作者：** qi Cai

**联系邮箱：** <qicaiCN@gmail.com>

`method_rotation_averaging` 是用于全局旋转平均的 PoMVG 方法插件。签名市场安装包提供两个可选后端：

- **Chatterjee**：使用受支持 PoMVG Consumer 和 Developer 分发包提供的 `method_rotation_averaging_chatterjee` 后端。
- **GraphOptim**：使用开源 GraphOptim 的 `rotation_estimator`。安装器在插件私有安装状态目录内构建，并自动配置稳定的可执行文件路径。

## 安装

1. 在 PoMVG 打开 **Install & Manage Plugins → Marketplace**。
2. 在 `SfM/Optimization` 下找到 **Rotation Averaging**。
3. 选择 **Install**，核对发布者与目标平台。
4. PoMVG 下载目标平台的签名 `.pospkg`，验证 SHA-256、Ed25519 签名、ABI、分发模式及载荷哈希，再执行包内 `install.sh`。

无需手动寻找 GraphOptim 路径。PoMVG 验证安装后的 `rotation_estimator`，维护稳定的 `graphoptim_bin` 绑定，并将已验证目录写入 `graphoptim_bin_folder`。

## 支持平台

- PoMVG Factory/plugin ABI 2/2（PoSDK 2.1.1）；不兼容 ABI1 宿主。
- macOS Apple Silicon（`arm64`）。
- GraphOptim 源码构建需要 Homebrew 和 Xcode Command Line Tools。

安装器可能通过 Homebrew 联网安装缺失的固定构建前提，不会将 GraphOptim 安装到系统级前缀目录。

## 发布模式

本版本的分发元数据使用 `closed_plugin_open_dependencies`：

- PoMVG 插件实现作为签名二进制分发。
- GraphOptim 保持 BSD-3-Clause 开源许可。
- 精确的 GraphOptim 提交、源码归档 SHA-256、构建配方和前提见 [`deps/README.md`](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_rotation_averaging/deps/README.md)。
- 公开仓库提供可审阅元数据和脚本；可执行载荷只作为签名的 GitHub Release `.pospkg` 资源发布。

## 输入与输出

- 输入：`data_relative_poses`。
- 输出：`data_global_poses`。
- 分组：`SfM/Optimization`。

## 发布交接

准备好的公开元数据位于 `dist/marketplace-repository`。在共享仓库 `pose-only-vision/PoSDK-plugins` 中，将这些文件发布到 `plugins/method_rotation_averaging`，创建发布标签 `posdk-method_rotation_averaging-v0.1.1`，并按原文件名上传：

- `method_rotation_averaging-0.1.1-macos-arm64.pospkg`
- `method_rotation_averaging-0.1.1-macos-arm64.pospkg.sha256`

然后在 PoMVG 插件市场发布表单提交规范目录 URL：

`https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_rotation_averaging`

可另填 Logo URL。PoSDK.net 自动读取本 README 及所有签名插件元数据。

## 第三方依赖

| 组件 | 版本与来源 | 用途 | 许可与声明 |
|---|---|---|---|
| [GraphOptim](https://github.com/AIBluefisher/GraphOptim/tree/9d932464831bff46b1b5a118ececb2e00c42d5c6) | `9d932464831bff46b1b5a118ececb2e00c42d5c6`; 签名归档哈希见 `deps/README.md` | 可选的外部旋转估计器 | BSD-3-Clause; Copyright (c) 2022, Chenyu |
| Chatterjee rotation averaging | 宿主提供 `method_rotation_averaging_chatterjee` | 可选的内置后端 | Avishek Chatterjee and Venu Madhav Govindu; 实现声明由宿主提供 |
| CMake, Eigen, Ceres, glog, gflags, SuiteSparse, libomp, TBB, METIS | 实际 Homebrew 版本记录在安装器构建契约中 | GraphOptim 编译及链接库 | 各上游分发包保留组件许可证文件 |

插件集成作者为 qi Cai，GraphOptim 与 Chatterjee 方法保留原作者署名。GraphOptim 源码归档固定提交和 SHA-256；构建依赖的实际版本另记入本机构建契约。来源、用途、版本及许可证位置详见 [THIRD_PARTY_NOTICES.md](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_rotation_averaging/THIRD_PARTY_NOTICES.md) 和 [deps/README.md](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_rotation_averaging/deps/README.md)。

## 许可

PoMVG 插件二进制采用 `LicenseRef-PoSDK-Proprietary`。GraphOptim 及其他第三方组件保留各自许可证，详见 [LICENSE.md](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_rotation_averaging/LICENSE.md) 和 [`deps/README.md`](https://github.com/pose-only-vision/PoSDK-plugins/blob/main/plugins/method_rotation_averaging/deps/README.md)。
