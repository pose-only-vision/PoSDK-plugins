# Rotation Averaging for PoMVG

<!-- posdk:upstream-research:v1:start -->
## Original research and authors / 原论文与原作团队

**PoSDK integration / 插件封装：** qi Cai. Original algorithm authorship and upstream licenses remain with the authors below.

### Efficient and Robust Large-Scale Rotation Averaging

- **Authors / 原作者：** Avishek Chatterjee, Venu Madhav Govindu.
- **Team / 团队：** Indian Institute of Science, Bengaluru.
- **Publication / 论文：** 2013 · ICCV.
- **Paper / 原文：** [Efficient and Robust Large-Scale Rotation Averaging](https://openaccess.thecvf.com/content_iccv_2013/html/Chatterjee_Efficient_and_Robust_2013_ICCV_paper.html).
- **Official project or implementation / 原作项目或实现：** [https://ee.iisc.ac.in/cvlab/research/rotaveraging/](https://ee.iisc.ac.in/cvlab/research/rotaveraging/).

### Hybrid Rotation Averaging: A Fast and Robust Rotation Averaging Approach

- **Authors / 原作者：** Yu Chen, Ji Zhao, Laurent Kneip.
- **Team / 团队：** GraphOptim original author team.
- **Publication / 论文：** 2021 · CVPR.
- **Paper / 原文：** [Hybrid Rotation Averaging: A Fast and Robust Rotation Averaging Approach](https://openaccess.thecvf.com/content/CVPR2021/html/Chen_Hybrid_Rotation_Averaging_A_Fast_and_Robust_Rotation_Averaging_Approach_CVPR_2021_paper.html).
- **Official project or implementation / 原作项目或实现：** [https://github.com/AIBluefisher/GraphOptim](https://github.com/AIBluefisher/GraphOptim).

The Chatterjee and GraphOptim backends are distinct implementations. The host supplies the Chatterjee child plugin; GraphOptim is installed from its pinned upstream source.

Attribution checked against the original projects on **2026-11-01**. Citation does not imply endorsement of the PoSDK wrapper.
<!-- posdk:upstream-research:v1:end -->

**Plugin integration author / 插件集成作者:** qi Cai

**Contact / 联系邮箱:** <qicaiCN@gmail.com>

`method_rotation_averaging` is a PoMVG method plugin for global rotation
averaging. The signed Marketplace package provides two selectable backends:

- **Chatterjee** uses the `method_rotation_averaging_chatterjee` backend shipped
  with supported PoMVG Consumer and Developer distributions.
- **GraphOptim** uses the open-source GraphOptim `rotation_estimator`. The
  package installer builds it inside this plugin's private installation state
  and configures the stable executable path automatically.

## Installation

1. Open **Install & Manage Plugins → Marketplace** in PoMVG.
2. Find **Rotation Averaging** under `SfM/Optimization`.
3. Select **Install** and review the publisher and target information.
4. PoMVG downloads the signed target-specific `.pospkg`, verifies its SHA-256,
   Ed25519 signature, ABI, distribution model, and payload hashes, then runs the
   package-internal `install.sh`.

No manual GraphOptim path is required. PoMVG validates the installed
`rotation_estimator`, maintains the stable `graphoptim_bin` binding, and writes
the verified directory to `graphoptim_bin_folder`.

## Supported target

- PoMVG with Factory/plugin ABI 2/2 (PoSDK 2.1.1); ABI1 hosts are incompatible
- macOS on Apple Silicon (`arm64`)
- Homebrew and Xcode Command Line Tools for the GraphOptim source build

The installer may use the network to install missing pinned build prerequisites
through Homebrew. It does not install GraphOptim into a system-wide prefix.

## Publication model

This release uses `closed_plugin_open_dependencies`:

- the PoMVG plugin implementation is distributed as a signed binary;
- GraphOptim remains open source under BSD-3-Clause;
- the exact GraphOptim revision, source-archive SHA-256, build recipe, and
  prerequisites are documented in [`deps/README.md`](deps/README.md);
- the public repository contains review metadata and scripts, while executable
  payloads are published only as signed GitHub Release `.pospkg` assets.

## Inputs and outputs

- Input: `data_relative_poses`
- Output: `data_global_poses`
- Group: `SfM/Optimization`

## Publisher handoff

The prepared public metadata is `dist/marketplace-repository`. In the shared
`pose-only-vision/PoSDK-plugins` repository, publish those files below
`plugins/method_rotation_averaging`, create Release tag
`posdk-method_rotation_averaging-v0.1.1`, and upload
these two files without renaming them:

- `method_rotation_averaging-0.1.1-macos-arm64.pospkg`
- `method_rotation_averaging-0.1.1-macos-arm64.pospkg.sha256`

Then open the PoMVG plugin marketplace publisher form and submit the canonical plugin
directory URL
`https://github.com/pose-only-vision/PoSDK-plugins/tree/main/plugins/method_rotation_averaging`
and an optional Logo URL. PoSDK.net reads this README and all signed plugin
metadata automatically.

## Third-party dependencies / 第三方依赖

| Component | Version / provenance | Role | License / notice |
|---|---|---|---|
| [GraphOptim](https://github.com/AIBluefisher/GraphOptim/tree/9d932464831bff46b1b5a118ececb2e00c42d5c6) | `9d932464831bff46b1b5a118ececb2e00c42d5c6`; signed archive hash in `deps/README.md` | Optional external rotation estimator | BSD-3-Clause; Copyright (c) 2022, Chenyu |
| Chatterjee rotation averaging | Host-provided `method_rotation_averaging_chatterjee` | Alternate built-in backend | Avishek Chatterjee and Venu Madhav Govindu; implementation notices supplied by the host |
| CMake, Eigen, Ceres, glog, gflags, SuiteSparse, libomp, TBB, METIS | Actual Homebrew versions recorded in the installer's build contract | GraphOptim compilation and linked libraries | Each upstream distribution retains its component license files |

插件集成作者为 qi Cai；GraphOptim 与 Chatterjee 方法保留原始作者署名。
GraphOptim 源码归档固定提交和 SHA-256，构建依赖的实际版本另记入本机构建契约。
详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 与
[deps/README.md](deps/README.md) 中的来源、用途、版本和许可证位置。

## License

The PoMVG plugin binary is distributed under `LicenseRef-PoSDK-Proprietary`.
GraphOptim and all other third-party components retain their respective
licenses. See [LICENSE.md](LICENSE.md) and
[`deps/README.md`](deps/README.md).
