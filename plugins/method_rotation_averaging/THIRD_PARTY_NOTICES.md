# Rotation Averaging third-party notices / 第三方依赖声明

**PoMVG plugin integration author:** qi Cai

**Contact:** <qicaiCN@gmail.com>

The PoMVG integration author is separate from the authors and copyright
holders of the rotation-averaging algorithms and their dependencies. The
plugin's existing `LicenseRef-PoSDK-Proprietary` notice remains in `LICENSE.md`;
it does not replace any third-party license.

## GraphOptim backend

- Source: <https://github.com/AIBluefisher/GraphOptim>.
- Pinned revision: `9d932464831bff46b1b5a118ececb2e00c42d5c6`.
- Upstream copyright: Copyright (c) 2022, Chenyu; BSD-3-Clause.
- Source/license: <https://github.com/AIBluefisher/GraphOptim/blob/9d932464831bff46b1b5a118ececb2e00c42d5c6/LICENSE>.
- Purpose: the optional external `rotation_estimator` backend.
- The signed package embeds the source with host-compatibility changes at
  `deps/GraphOptim-source.tar.gz`, SHA-256
  `e3a3bd61123091a6ebb254c5517fd3b83c0834b74c4ea87cee316b8ff25c5251`.
- The upstream license remains in that source archive and the installed source
  tree. The installer builds the selected executable in plugin-private state
  and returns the verified `graphoptim_bin` binding.

## Build and linked dependencies

GraphOptim's build prerequisites are CMake, Eigen, Ceres Solver, glog, gflags,
SuiteSparse, libomp, TBB and METIS. They are supplied by Homebrew on the supported
macOS arm64 target; they are not relabeled as PoMVG-authored components.
Sources: <https://cmake.org/>, <https://gitlab.com/libeigen/eigen>,
<https://github.com/ceres-solver/ceres-solver>, <https://github.com/google/glog>,
<https://github.com/gflags/gflags>, <https://github.com/DrTimothyAldenDavis/SuiteSparse>,
<https://github.com/llvm/llvm-project/tree/main/openmp>,
<https://github.com/oneapi-src/oneTBB> and <https://github.com/KarypisLab/METIS>.

The installer records the actual formula versions and toolchain in its local
build contract. A changed contract triggers a rebuild; these prerequisites
are not claimed to have one immutable version across all user installations.
Their complete formula/distribution license and component notices remain
applicable, including licenses for bundled or linked subcomponents. Consult
`deps/README.md`, the installed build contract and the exact distributions.

## Chatterjee and host runtime

The `chatterjee` selection delegates to the host-provided
`method_rotation_averaging_chatterjee` plugin; this package does not copy its
implementation. The underlying method is credited to Avishek Chatterjee and
Venu Madhav Govindu. Its own host distribution supplies the implementation
and notices. The matching PoMVG host also supplies the PoSDK plugin API;
these host dependencies are separate from GraphOptim's private installation.

插件集成由 qi Cai 维护。GraphOptim、Chatterjee 方法及各构建/运行库保留上游作者
和许可证；源码提交、归档哈希与实际构建版本分别记录，避免把上游算法署名为
插件作者，也不把系统提供的依赖描述成插件自带组件。
