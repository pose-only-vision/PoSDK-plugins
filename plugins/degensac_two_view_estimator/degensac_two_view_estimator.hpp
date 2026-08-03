/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/**
 * @file degensac_two_view_estimator.hpp
 * @brief Degensac两视图估计器（纯C++版本）
 * @details 直接调用Degensac C代码实现双视图位姿估计，无需Python依赖
 * @copyright Copyright (c) 2024-2026 Qi Cai
 * @license MPL-2.0; vendored code includes MIT and LGPL-2.1 components.
 */

#pragma once

#include <po_core.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <po_core/po_logger.hpp>
#include <cstdint>
#include <string>
#include <vector>

// Degensac C headers
extern "C" {
#include "exp_ranF.h"
#include "Fcustomdef.h"
#include "rtools.h"
}

namespace PluginMethods
{
    using namespace PoSDK;
    using namespace Interface;
    using namespace types;

    /**
     * @brief Degensac两视图估计器类
     * @details 使用DEGENSAC算法进行鲁棒的基础矩阵/本质矩阵估计
     * DEGENSAC特点：
     * - LO-RANSAC（带局部优化的RANSAC）
     * - 退化检测（能处理平面场景等退化情况）
     * - 高效的内点筛选
     */
    class DegensacTwoViewEstimator
        : public MethodPresetProfiler,
          public StaticTwoViewEstimatorCapabilities<
              TwoViewEstimatorInputContract::IdMatchesWithFeaturesAndCameras,
              TwoViewEstimatorPoseConvention::OpenGV,
              TwoViewEstimatorInlierContract::MatchesInPlace,
              TwoViewEstimatorRefinementContract::Unsupported>
    {
    public:
        /**
         * @brief 误差类型枚举
         */
        enum class ErrorType
        {
            SAMPSON = 0,      // Sampson误差（推荐）
            SYMM_EPIPOLAR = 1 // 对称极线误差
        };

        DegensacTwoViewEstimator()
        {
            // 设置需要的输入数据包
            required_package_["data_sample"] = nullptr;
            required_package_["data_features"] = nullptr;
            required_package_["data_camera_models"] = nullptr;

            output_types_ = {"data_relative_pose"};

            // 加载配置
            InitializeDefaultConfigPath();
        }

        ~DegensacTwoViewEstimator() override = default;

        DataPtr Run() override;

        // ✨ GetType() is automatically implemented by REGISTRATION_PLUGIN macro
        const std::string &GetType() const override;

    private:
        /**
         * @brief 使用Degensac估计基础矩阵
         * @param points1 第一视图的点（像素坐标）
         * @param points2 第二视图的点（像素坐标）
         * @param threshold 内点阈值（像素）
         * @param confidence 置信度
         * @param max_iterations 最大迭代次数
         * @param F 输出的基础矩阵
         * @param inliers_mask 输出的内点掩码
         * @param enable_degeneracy_check 是否启用退化检测
         * @return 是否成功
         */
        bool EstimateFundamentalMatrixWithDegensac(
            const std::vector<cv::Point2f> &points1,
            const std::vector<cv::Point2f> &points2,
            double threshold,
            double confidence,
            int max_iterations,
            unsigned int random_seed,
            cv::Mat &F,
            cv::Mat &inliers_mask,
            bool enable_degeneracy_check = true);

        /**
         * @brief 将基础矩阵转换为本质矩阵
         * @param F 基础矩阵
         * @param K1 第一视图相机内参矩阵
         * @param K2 第二视图相机内参矩阵
         * @return 本质矩阵
         */
        cv::Mat FundamentalToEssential(
            const cv::Mat &F,
            const cv::Mat &K1,
            const cv::Mat &K2);

        /**
         * @brief 准备点对数据用于Degensac
         * @param points1 第一视图的点
         * @param points2 第二视图的点
         * @param u 输出的点对数组（格式：[x1,y1,1,x2,y2,1] × N）
         */
        void PreparePointPairs(
            const std::vector<cv::Point2f> &points1,
            const std::vector<cv::Point2f> &points2,
            std::vector<double> &u);

        /**
         * @brief 解析误差类型字符串
         * @param error_type_str 误差类型字符串
         * @return 误差类型枚举
         */
        bool ParseErrorType(
            const std::string &error_type_str,
            ErrorType &error_type);
    };

} // namespace PluginMethods
