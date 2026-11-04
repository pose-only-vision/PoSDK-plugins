/**
 * @file method_rotation_averaging.hpp
 * @brief 旋转平均方法
 * @details 实现全局旋转估计,支持多种旋转平均算法
 *
 * @copyright Copyright (c) 2024 Qi Cai
 * SPDX-License-Identifier: LicenseRef-Custom
  * @author qi Cai
*/

#ifndef _METHOD_ROTATION_AVERAGING_HPP_
#define _METHOD_ROTATION_AVERAGING_HPP_

#include <filesystem>
#include <fstream>
#include <string>
#include <po_core.hpp>
#include <po_core/po_logger.hpp>
#include <po_core/types/pose_graph_quality.hpp>
namespace PoSDK {
using namespace Interface;
using namespace types;

class MethodRotationAveraging : public Interface::MethodPresetProfiler {
public:
        // <posdk:copyright>
        PoSDK::Interface::CopyrightData Copyright() const override
        {
        // <posdk:copyright-body>
        return {"PoMVG/method_rotation_averaging", "Copyright (c) 2024 Qi Cai\nAuthor: qi Cai\nLicense: LicenseRef-Custom\nEmail: qicaiCN@gmail.com\n\n# Rotation Averaging third-party notices / 第三方依赖声明\n\n**PoMVG plugin integration author:** qi Cai\n\n**Contact:** <qicaiCN@gmail.com>\n\nThe PoMVG integration author is separate from the authors and copyright\nholders of the rotation-averaging algorithms and their dependencies. The\nplugin's existing `LicenseRef-PoSDK-Proprietary` notice remains in `LICENSE.md`;\nit does not replace any third-party license.\n\n## GraphOptim backend\n\n- Source: <https://github.com/AIBluefisher/GraphOptim>.\n- Pinned revision: `9d932464831bff46b1b5a118ececb2e00c42d5c6`.\n- Upstream copyright: Copyright (c) 2022, Chenyu; BSD-3-Clause.\n- Source/license: <https://github.com/AIBluefisher/GraphOptim/blob/9d932464831bff46b1b5a118ececb2e00c42d5c6/LICENSE>.\n- Purpose: the optional external `rotation_estimator` backend.\n- The signed package embeds the source with host-compatibility changes at\n  `deps/GraphOptim-source.tar.gz`, SHA-256\n  `e3a3bd61123091a6ebb254c5517fd3b83c0834b74c4ea87cee316b8ff25c5251`.\n- The upstream license remains in that source archive and the installed source\n  tree. The installer builds the selected executable in plugin-private state\n  and returns the verified `graphoptim_bin` binding.\n\n## Build and linked dependencies\n\nGraphOptim's build prerequisites are CMake, Eigen, Ceres Solver, glog, gflags,\nSuiteSparse, libomp, TBB and METIS. They are supplied by Homebrew on the supported\nmacOS arm64 target; they are not relabeled as PoMVG-authored components.\nSources: <https://cmake.org/>, <https://gitlab.com/libeigen/eigen>,\n<https://github.com/ceres-solver/ceres-solver>, <https://github.com/google/glog>,\n<https://github.com/gflags/gflags>, <https://github.com/DrTimothyAldenDavis/SuiteSparse>,\n<https://github.com/llvm/llvm-project/tree/main/openmp>,\n<https://github.com/oneapi-src/oneTBB> and <https://github.com/KarypisLab/METIS>.\n\nThe installer records the actual formula versions and toolchain in its local\nbuild contract. A changed contract triggers a rebuild; these prerequisites\nare not claimed to have one immutable version across all user installations.\nTheir complete formula/distribution license and component notices remain\napplicable, including licenses for bundled or linked subcomponents. Consult\n`deps/README.md`, the installed build contract and the exact distributions.\n\n## Chatterjee and host runtime\n\nThe `chatterjee` selection delegates to the host-provided\n`method_rotation_averaging_chatterjee` plugin; this package does not copy its\nimplementation. The underlying method is credited to Avishek Chatterjee and\nVenu Madhav Govindu. Its own host distribution supplies the implementation\nand notices. The matching PoMVG host also supplies the PoSDK plugin API;\nthese host dependencies are separate from GraphOptim's private installation.\n\n插件集成由 qi Cai 维护。GraphOptim、Chatterjee 方法及各构建/运行库保留上游作者\n和许可证；源码提交、归档哈希与实际构建版本分别记录，避免把上游算法署名为\n插件作者，也不把系统提供的依赖描述成插件自带组件。\n"};
        // </posdk:copyright-body>
    }
        // </posdk:copyright>

  MethodRotationAveraging() {
    // 设置需要的输入数据包
    required_package_["data_relative_poses"] = nullptr; // 相对位姿数据

    // The wrapper exposes the exact graph used by RA.  This is intentionally
    // a package output rather than an in-place mutation of the caller's
    // RelativePoses: RA-1/RA-2 may be connected to different graph policies.
    output_types_ = {"data_global_poses", "data_relative_poses",
                     "data_relative_rotations", "data_pose_graph_quality"};

    // 初始化配置
    InitializeDefaultConfigPath();

    // 自动检测GraphOptim二进制文件目录并缓存
    graphoptim_bin_folder_ = DetectGraphOptimBinPath();
  }

  ~MethodRotationAveraging() override = default;

  // ✨ GetType() is automatically implemented by REGISTRATION_PLUGIN macro
  const std::string &GetType() const override;

  DataPtr Run() override;

private:
  /**
   * @brief 使用GraphOptim进行旋转平均
   * @param relative_poses 相对位姿数据
   * @param global_poses 全局位姿数据
   * @return 是否成功
   */
  bool RunGraphOptim(const RelativePoses &relative_poses,
                     GlobalPoses &global_poses);

  /**
   * @brief 从GraphOptim结果文件读取全局旋转
   * @param filename 结果文件名
   * @param global_poses 全局位姿数据
   * @return 是否成功
   */
  bool ImportFromGraphOptim(const std::string &filename,
                            GlobalPoses &global_poses) const;

  /**
   * @brief 检查GraphOptim工具是否存在且可执行
   * @param tool_path GraphOptim工具路径
   * @return 是否可用
   */
  bool CheckGraphOptimTool(const std::string &tool_path) const;

  /**
   * @brief 自动检测GraphOptim二进制目录路径
   * @return GraphOptim bin目录路径，失败时返回空字符串
   */
  std::string DetectGraphOptimBinPath() const;

  /**
   * @brief 打印旋转估计状态 | Print rotation estimation status
   * @param global_poses 全局位姿数据 | Global poses data
   * @param relative_poses 相对位姿数据（用于获取输入视图信息）| Relative poses
   * (for input view info)
   */
  void PrintEstimationStatus(const GlobalPoses &global_poses,
                             const RelativePoses &relative_poses) const;

  bool ApplyRotationGraphFilter(
      RelativePoses &relative_poses,
      RelativePoses::RotationGraphFilterResult &result,
      std::string &mode,
      const GlobalPoses *scoring_prior = nullptr) const;

  bool ApplyFixedCoreCommit(
      const GlobalPoses &candidate,
      const GlobalPoses &prior,
      const types::PoseGraphQuality &prior_quality,
      const Tracks &tracks,
      const RelativePoses &filtered_relative_poses,
      GlobalPoses &output,
      types::PoseGraphQuality &output_quality,
      bool update_certificate = true) const;

private:
  // GraphOptim二进制文件目录
  std::string graphoptim_bin_folder_;
  // 临时文件路径
  std::string temp_g2o_path_;
  std::string temp_result_path_;
};

} // namespace PoSDK

#endif // _METHOD_ROTATION_AVERAGING_HPP_
