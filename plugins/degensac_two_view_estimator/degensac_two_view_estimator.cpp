/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/**
 * @file degensac_two_view_estimator.cpp
 * @brief Degensac two-view estimator implementation (Pure C++ version)
 * @details 直接调用Degensac C代码实现双视图位姿估计，无需Python依赖
 * @copyright Copyright (c) 2024-2026 Qi Cai
 * @license MPL-2.0; vendored code includes MIT and LGPL-2.1 components.
 */

#include "degensac_two_view_estimator.hpp"
#include "degensac_rng.h"
#include <opencv2/core/eigen.hpp>
#include <po_core/po_logger.hpp>
#include <po_core/ProfilerManager.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>

namespace PluginMethods
{
    using namespace PoSDK;
    using namespace Interface;
    using namespace types;
    using namespace Eigen;

    namespace
    {
        constexpr std::size_t kMaxCorrespondences = 100000;
        constexpr std::uint64_t kMaxSamplingWork = 100000000;
    }

    DataPtr DegensacTwoViewEstimator::Run()
    {
        DisplayConfigInfo();

        // 1. Get input data | 1. 获取输入数据
        auto sample_ptr = CastToSample<IdMatches>(required_package_["data_sample"]);
        auto features_ptr = GetDataPtr<FeaturesInfo>(required_package_["data_features"]);
        auto cameras_ptr = GetDataPtr<CameraModels>(required_package_["data_camera_models"]);

        if (!sample_ptr || sample_ptr->empty() || !features_ptr || !cameras_ptr)
        {
            LOG_ERROR_ZH << "无效或空输入数据。";
            LOG_ERROR_EN << "Invalid or empty input data.";
            return nullptr;
        }

        // 2. Get view pair information from method_options_ | 2. 从method_options_获取视图对信息
        ViewPair view_pair(
            GetOptionAsIndexT("view_i", 0),
            GetOptionAsIndexT("view_j", 1));
        ViewId view_id1 = view_pair.first;
        ViewId view_id2 = view_pair.second;

        // Get camera intrinsics | 获取相机内参
        Matrix3d K1_eig;
        Matrix3d K2_eig;
        cv::Mat K1;
        cv::Mat K2;
        const CameraModel *camera1 = (*cameras_ptr)[view_id1];
        const CameraModel *camera2 = (*cameras_ptr)[view_id2];
        if (!camera1 || !camera2)
        {
            LOG_ERROR_ZH << "视图对对应的相机模型不存在。";
            LOG_ERROR_EN << "Camera models are missing for the requested view pair.";
            return nullptr;
        }
        if (!camera1->HasClassicIntrinsics()
            || !camera2->HasClassicIntrinsics()
            || camera1->GetClassicIntrinsics().GetDistortionType()
                   != DistortionType::NO_DISTORTION
            || camera2->GetClassicIntrinsics().GetDistortionType()
                   != DistortionType::NO_DISTORTION)
        {
            LOG_ERROR_ZH << "DEGENSAC当前仅接受无畸变的经典针孔相机。";
            LOG_ERROR_EN << "DEGENSAC currently accepts only undistorted classic pinhole cameras.";
            return nullptr;
        }
        try
        {
            camera1->GetKMat(K1_eig);
            camera2->GetKMat(K2_eig);
            cv::eigen2cv(K1_eig, K1);
            cv::eigen2cv(K2_eig, K2);
        }
        catch (const std::out_of_range &e)
        {
            LOG_ERROR_ZH << "无法获取视图对 (" << view_id1 << ", " << view_id2
                         << ") 的相机模型: " << e.what();
            LOG_ERROR_EN << "Failed to get camera models for view pair ("
                         << view_id1 << ", " << view_id2 << "): " << e.what();
            return nullptr;
        }
        if (!K1_eig.allFinite() || !K2_eig.allFinite()
            || std::abs(K1_eig.determinant()) < 1e-12
            || std::abs(K2_eig.determinant()) < 1e-12)
        {
            LOG_ERROR_ZH << "视图对的相机内参无效或不可逆。";
            LOG_ERROR_EN << "Camera intrinsics are invalid or singular.";
            return nullptr;
        }

        // Extract matching points | 提取匹配点
        IdMatches *matches_ptr = static_cast<IdMatches *>(sample_ptr->GetData());
        if (!matches_ptr)
        {
            LOG_ERROR_ZH << "从样本数据中获取匹配失败。";
            LOG_ERROR_EN << "Failed to get matches from sample data.";
            return nullptr;
        }
        const auto &id_matches = *matches_ptr;
        if (id_matches.size() < 8)
        {
            LOG_WARNING_ZH << "由于匹配不足而跳过视图对 (" << view_id1 << ", " << view_id2 << "): " << id_matches.size();
            LOG_WARNING_EN << "Skipping view pair (" << view_id1 << ", " << view_id2 << ") due to insufficient matches: " << id_matches.size();
            return nullptr;
        }
        if (id_matches.size() > kMaxCorrespondences)
        {
            LOG_ERROR_ZH << "DEGENSAC 匹配数量超过产品安全上限: " << id_matches.size();
            LOG_ERROR_EN << "DEGENSAC correspondence count exceeds the product safety limit: " << id_matches.size();
            return nullptr;
        }
        if (view_id1 >= features_ptr->size() || view_id2 >= features_ptr->size()
            || !(*features_ptr)[view_id1] || !(*features_ptr)[view_id2])
        {
            LOG_ERROR_ZH << "视图对对应的特征数据不存在。";
            LOG_ERROR_EN << "Feature data is missing for the requested view pair.";
            return nullptr;
        }
        const auto &features1 = (*features_ptr)[view_id1]->GetFeaturePoints();
        const auto &features2 = (*features_ptr)[view_id2]->GetFeaturePoints();
        std::vector<cv::Point2f> points1, points2;
        points1.reserve(id_matches.size());
        points2.reserve(id_matches.size());

        for (const auto &match : id_matches)
        {
            if (static_cast<size_t>(match.i) >= features1.size()
                || static_cast<size_t>(match.j) >= features2.size())
            {
                LOG_ERROR_ZH << "匹配索引超出特征数组范围。";
                LOG_ERROR_EN << "A match index is outside the feature array.";
                return nullptr;
            }
            const auto &p1 = features1[match.i].GetCoord();
            const auto &p2 = features2[match.j].GetCoord();
            if (!std::isfinite(p1.x()) || !std::isfinite(p1.y())
                || !std::isfinite(p2.x()) || !std::isfinite(p2.y()))
            {
                LOG_ERROR_ZH << "匹配点包含非有限坐标。";
                LOG_ERROR_EN << "A correspondence contains non-finite coordinates.";
                return nullptr;
            }
            points1.emplace_back(p1.x(), p1.y());
            points2.emplace_back(p2.x(), p2.y());
        }

        LOG_INFO_ZH << "处理视图对 (" << view_id1 << ", " << view_id2 << ") 使用算法: Degensac (C++)";
        LOG_INFO_EN << "Processing view pair (" << view_id1 << ", " << view_id2 << ") with algorithm: Degensac (C++)";

        // 3. Get Degensac parameters | 3. 获取Degensac参数
        double threshold = GetOptionAsDouble("ransac_threshold", 0.5);
        double confidence = GetOptionAsDouble("confidence", 0.9999);
        const std::uint64_t configured_iterations = static_cast<std::uint64_t>(
            GetOptionAsIndexT("max_iterations", 100000));
        if (configured_iterations == 0
            || configured_iterations
                   > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
        {
            LOG_ERROR_ZH << "max_iterations 超出 DEGENSAC 支持范围。";
            LOG_ERROR_EN << "max_iterations is outside the DEGENSAC range.";
            return nullptr;
        }
        if (configured_iterations
            > kMaxSamplingWork / static_cast<std::uint64_t>(points1.size()))
        {
            LOG_ERROR_ZH << "DEGENSAC 匹配规模与迭代预算的乘积超过产品安全上限。";
            LOG_ERROR_EN << "DEGENSAC correspondence count times iteration budget exceeds the product safety limit.";
            return nullptr;
        }
        const int max_iterations = static_cast<int>(configured_iterations);
        const std::uint64_t configured_seed = static_cast<std::uint64_t>(
            GetOptionAsIndexT("random_seed", 0));
        if (configured_seed > std::numeric_limits<unsigned int>::max())
        {
            LOG_ERROR_ZH << "random_seed 超出 DEGENSAC 支持范围。";
            LOG_ERROR_EN << "random_seed is outside the DEGENSAC range.";
            return nullptr;
        }
        const unsigned int random_seed =
            static_cast<unsigned int>(configured_seed);
        bool enable_degeneracy_check = GetOptionAsBool("enable_degeneracy_check", true);

        // 4. Estimate fundamental matrix using Degensac | 4. 使用Degensac估计基础矩阵
        cv::Mat F, inliers_mask;

        bool success = false;
        {
            PROFILER_START_AUTO(enable_profiling_);
            success = EstimateFundamentalMatrixWithDegensac(
                points1, points2, threshold, confidence, max_iterations, random_seed,
                F, inliers_mask, enable_degeneracy_check);
            PROFILER_END();

            if (SHOULD_LOG(DEBUG))
            {
                PROFILER_PRINT_STATS(enable_profiling_);
            }
        }

        if (!success)
        {
            LOG_ERROR_ZH << "对视图对 (" << view_id1 << ", " << view_id2 << ") 的基础矩阵估计失败。";
            LOG_ERROR_EN << "Fundamental matrix estimation failed for pair (" << view_id1 << ", " << view_id2 << ").";
            return nullptr;
        }

        // 5. Convert fundamental matrix to essential matrix | 5. 将基础矩阵转换为本质矩阵
        cv::Mat E = FundamentalToEssential(F, K1, K2);
        if (!cv::checkRange(E))
        {
            LOG_ERROR_ZH << "由基础矩阵转换得到的本质矩阵无效。";
            LOG_ERROR_EN << "The essential matrix converted from F is invalid.";
            return nullptr;
        }

        // 6. Recover pose from essential matrix | 6. 从本质矩阵恢复位姿
        std::vector<cv::Point2f> normalized_points1;
        std::vector<cv::Point2f> normalized_points2;
        cv::Mat R, t;
        int inlier_count = 0;
        try
        {
            cv::undistortPoints(points1, normalized_points1, K1, cv::noArray());
            cv::undistortPoints(points2, normalized_points2, K2, cv::noArray());
            inlier_count = cv::recoverPose(
                E, normalized_points1, normalized_points2, R, t, 1.0,
                cv::Point2d(0.0, 0.0), inliers_mask);
        }
        catch (const cv::Exception &error)
        {
            LOG_ERROR_ZH << "DEGENSAC 位姿恢复失败: " << error.what();
            LOG_ERROR_EN << "DEGENSAC pose recovery failed: " << error.what();
            return nullptr;
        }
        if (inlier_count < 5 || R.empty() || t.empty()
            || !cv::checkRange(R) || !cv::checkRange(t))
        {
            LOG_ERROR_ZH << "DEGENSAC 未恢复出有效相对位姿。";
            LOG_ERROR_EN << "DEGENSAC did not recover a valid relative pose.";
            return nullptr;
        }

        // Commit only the final cheirality-validated inlier mask.
        if (inliers_mask.rows != static_cast<int>(id_matches.size()))
        {
            LOG_ERROR_ZH << "DEGENSAC 返回的内点掩码尺寸不匹配。";
            LOG_ERROR_EN << "DEGENSAC returned an inlier mask with the wrong size.";
            return nullptr;
        }
        for (size_t i = 0; i < id_matches.size(); ++i)
        {
            matches_ptr->at(i).is_inlier =
                inliers_mask.at<uchar>(static_cast<int>(i)) != 0;
        }

        LOG_INFO_ZH << "Degensac估计完成，内点数: " << inlier_count << "/" << points1.size();
        LOG_INFO_EN << "Degensac estimation completed, inliers: " << inlier_count << "/" << points1.size();

        // 7. Save results - output pose in OpenGV format | 7. 保存结果 - 输出OpenGV格式的位姿
        RelativePose rel_pose;
        rel_pose.SetViewIdI(view_id1);
        rel_pose.SetViewIdJ(view_id2);

        Matrix3d R_opencv;
        Vector3d t_opencv;

        cv::cv2eigen(R, R_opencv);
        cv::cv2eigen(t, t_opencv);

        rel_pose.SetRotation(R_opencv.transpose());
        rel_pose.SetTranslation(-R_opencv.transpose() * t_opencv);
        rel_pose.SetWeight(static_cast<float>(inlier_count) / static_cast<float>(points1.size()));

        // Debug output | 调试输出
        LOG_DEBUG_ZH << "DegensacTwoViewEstimator 坐标转换 for pair (" << view_id1 << ", " << view_id2 << "):";
        LOG_DEBUG_ZH << "OpenCV 格式 (xj = R*xi + t):";
        LOG_DEBUG_ZH << "R_opencv = " << std::endl
                     << R_opencv;
        LOG_DEBUG_ZH << "t_opencv = " << t_opencv.transpose();
        LOG_DEBUG_EN << "DegensacTwoViewEstimator coordinate conversion for pair (" << view_id1 << ", " << view_id2 << "):";
        LOG_DEBUG_EN << "OpenCV format (xj = R*xi + t):";
        LOG_DEBUG_EN << "R_opencv = " << std::endl
                     << R_opencv;
        LOG_DEBUG_EN << "t_opencv = " << t_opencv.transpose();

        return std::make_shared<DataMap<RelativePose>>(rel_pose, "data_relative_pose");
    }

    bool DegensacTwoViewEstimator::EstimateFundamentalMatrixWithDegensac(
        const std::vector<cv::Point2f> &points1,
        const std::vector<cv::Point2f> &points2,
        double threshold,
        double confidence,
        int max_iterations,
        unsigned int random_seed,
        cv::Mat &F,
        cv::Mat &inliers_mask,
        bool enable_degeneracy_check)
    {
        size_t num_points = points1.size();
        if (num_points < 8 || points2.size() != num_points
            || num_points > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            LOG_ERROR_ZH << "[Degensac] 点对数量无效或不匹配";
            LOG_ERROR_EN << "[Degensac] Point-pair count is invalid or mismatched";
            return false;
        }
        if (!std::isfinite(threshold) || threshold <= 0.0
            || !std::isfinite(confidence) || confidence <= 0.0
            || confidence >= 1.0 || max_iterations <= 0)
        {
            LOG_ERROR_ZH << "[Degensac] RANSAC参数无效";
            LOG_ERROR_EN << "[Degensac] Invalid RANSAC parameters";
            return false;
        }

        // 1. Prepare point pair data for Degensac | 1. 准备点对数据
        // Format: [x1, y1, 1, x2, y2, 1] for each point pair
        std::vector<double> u(num_points * 6);
        PreparePointPairs(points1, points2, u);

        // 2. Prepare output buffers | 2. 准备输出缓冲区
        double F_out[9] = {0};
        std::vector<unsigned char> inl(num_points, 0);
        std::vector<int> data_out(num_points * 18, 0);
        double H_best[9] = {0};
        int I_H = 0;

        // 3. Get error type | 3. 获取误差类型
        std::string error_type_str = GetOptionAsString("error_type", "sampson");
        ErrorType error_type = ErrorType::SAMPSON;
        if (!ParseErrorType(error_type_str, error_type))
        {
            return false;
        }

        // 4. Setup error functions | 4. 设置误差函数
        FDsPtr FDS1;
        exFDsPtr EXFDS1;
        FDsidxPtr FDSidx1;

        double error_threshold = threshold * threshold; // Degensac uses squared threshold
        bool sym_check_enable = GetOptionAsBool("symmetric_error_check", true);
        double SymCheck_th = sym_check_enable ? (threshold * threshold * 3.0) : 0.0;

        if (error_type == ErrorType::SAMPSON)
        {
            FDS1 = &FDs;
            EXFDS1 = &exFDs;
            FDSidx1 = &FDsidx;
        }
        else // SYMM_EPIPOLAR
        {
            FDS1 = &FDsSym;
            EXFDS1 = &exFDsSym;
            FDSidx1 = &FDsSymidx;
        }

        // 5. LAF coefficient (set to 0 if not using LAF) | 5. LAF系数（不使用LAF时设为0）
        double laf_coef = GetOptionAsDouble("laf_coefficient", 0.0);
        if (!std::isfinite(laf_coef) || laf_coef != 0.0)
        {
            LOG_ERROR_ZH << "[Degensac] 当前点匹配契约不提供LAF数据，laf_coefficient必须为0";
            LOG_ERROR_EN << "[Degensac] The point-match contract has no LAF data; laf_coefficient must be zero";
            return false;
        }

        LOG_INFO_ZH << "[DEGENSAC_DIAG] resolved_backend=degensac-native-exp_ransacFcustomLAF"
                    << ", random_seed=" << random_seed
                    << ", max_iterations=" << max_iterations
                    << ", threshold_pixels=" << threshold
                    << ", confidence=" << confidence
                    << ", degeneracy_check=" << (enable_degeneracy_check ? "true" : "false")
                    << ", symmetric_error_check=" << (sym_check_enable ? "true" : "false")
                    << ", error_type=" << error_type_str;
        LOG_INFO_EN << "[DEGENSAC_DIAG] resolved_backend=degensac-native-exp_ransacFcustomLAF"
                    << ", random_seed=" << random_seed
                    << ", max_iterations=" << max_iterations
                    << ", threshold_pixels=" << threshold
                    << ", confidence=" << confidence
                    << ", degeneracy_check=" << (enable_degeneracy_check ? "true" : "false")
                    << ", symmetric_error_check=" << (sym_check_enable ? "true" : "false")
                    << ", error_type=" << error_type_str;

        // 6. Call Degensac | 6. 调用Degensac
        // Note: u_1 and u_2 are for LAF consistency check, set to NULL if not used
        int result = exp_ransacFcustomLAFSeeded(
            u.data(), // point pairs
            nullptr,  // u_1 (LAF points, not used)
            nullptr,  // u_2 (LAF points, not used)
            static_cast<int>(num_points),
            error_threshold,
            laf_coef,
            confidence,
            max_iterations,
            F_out,
            inl.data(),
            data_out.data(),
            1,           // do_lo (enable local optimization)
            INL_LIMIT_F, // inlier limit (from rtools.h)
            nullptr,     // PoSDK does not capture per-LO residual history
            H_best,
            &I_H,
            EXFDS1,
            FDS1,
            FDSidx1,
            SymCheck_th,
            enable_degeneracy_check ? 1 : 0,
            random_seed);

        // 7. Check result | 7. 检查结果
        if (result < 8)
        {
            LOG_WARNING_ZH << "[Degensac] RANSAC返回失败，结果代码: " << result;
            LOG_WARNING_EN << "[Degensac] RANSAC returned failure, result code: " << result;
            return false;
        }

        // 8. Convert output | 8. 转换输出
        // The C backend exposes the 3x3 model in row-major order.
        F = cv::Mat(3, 3, CV_64F);
        for (int i = 0; i < 9; ++i)
        {
            F.at<double>(i / 3, i % 3) = F_out[i];
        }

        // Check if F is valid (not all zeros)
        double F_sum = 0;
        for (int i = 0; i < 9; ++i)
        {
            F_sum += std::abs(F_out[i]);
        }
        if (!std::isfinite(F_sum) || F_sum < 1e-10 || !cv::checkRange(F))
        {
            LOG_WARNING_ZH << "[Degensac] 估计的基础矩阵全为零";
            LOG_WARNING_EN << "[Degensac] Estimated fundamental matrix is all zeros";
            return false;
        }

        // Convert inlier mask
        inliers_mask = cv::Mat(static_cast<int>(num_points), 1, CV_8UC1);
        int inlier_count = 0;
        for (size_t i = 0; i < num_points; ++i)
        {
            inliers_mask.at<uchar>(static_cast<int>(i)) = inl[i] ? 255 : 0;
            if (inl[i])
                inlier_count++;
        }
        if (inlier_count < 8)
        {
            LOG_WARNING_ZH << "[Degensac] 有效内点少于基础矩阵所需的8个";
            LOG_WARNING_EN << "[Degensac] Fewer than eight valid fundamental-matrix inliers";
            return false;
        }

        LOG_DEBUG_ZH << "[Degensac] 基础矩阵估计成功，内点数: " << inlier_count << "/" << num_points;
        LOG_DEBUG_EN << "[Degensac] Fundamental matrix estimation successful, inliers: " << inlier_count << "/" << num_points;
        LOG_INFO_ZH << "[DEGENSAC_DIAG] result=success, backend_inliers="
                    << inlier_count << ", homography_inliers=" << I_H
                    << ", sampler_draws=" << posdk_degensac_rng_draw_count()
                    << ", sampler_fingerprint="
                    << posdk_degensac_rng_fingerprint();
        LOG_INFO_EN << "[DEGENSAC_DIAG] result=success, backend_inliers="
                    << inlier_count << ", homography_inliers=" << I_H
                    << ", sampler_draws=" << posdk_degensac_rng_draw_count()
                    << ", sampler_fingerprint="
                    << posdk_degensac_rng_fingerprint();

        // Check for degeneracy detection
        if (I_H > 0 && enable_degeneracy_check)
        {
            LOG_INFO_ZH << "[Degensac] 检测到退化情况（平面场景），H内点数: " << I_H;
            LOG_INFO_EN << "[Degensac] Degeneracy detected (planar scene), H inliers: " << I_H;
        }

        return true;
    }

    cv::Mat DegensacTwoViewEstimator::FundamentalToEssential(
        const cv::Mat &F,
        const cv::Mat &K1,
        const cv::Mat &K2)
    {
        // x2^T F x1 = 0 and xn=K^-1 x imply E=K2^T F K1.
        return K2.t() * F * K1;
    }

    void DegensacTwoViewEstimator::PreparePointPairs(
        const std::vector<cv::Point2f> &points1,
        const std::vector<cv::Point2f> &points2,
        std::vector<double> &u)
    {
        size_t num_points = points1.size();
        u.resize(num_points * 6);

        for (size_t i = 0; i < num_points; ++i)
        {
            // Format: [x1, y1, 1, x2, y2, 1]
            u[i * 6 + 0] = static_cast<double>(points1[i].x);
            u[i * 6 + 1] = static_cast<double>(points1[i].y);
            u[i * 6 + 2] = 1.0;
            u[i * 6 + 3] = static_cast<double>(points2[i].x);
            u[i * 6 + 4] = static_cast<double>(points2[i].y);
            u[i * 6 + 5] = 1.0;
        }
    }

    bool DegensacTwoViewEstimator::ParseErrorType(
        const std::string &error_type_str,
        ErrorType &error_type)
    {
        std::string lower_str = error_type_str;
        std::transform(
            lower_str.begin(), lower_str.end(), lower_str.begin(),
            [](unsigned char value) { return static_cast<char>(std::tolower(value)); });

        if (lower_str == "sampson")
        {
            error_type = ErrorType::SAMPSON;
            return true;
        }
        else if (lower_str == "symm_epipolar" || lower_str == "symmetric_epipolar")
        {
            error_type = ErrorType::SYMM_EPIPOLAR;
            return true;
        }
        else
        {
            LOG_ERROR_ZH << "[Degensac] 未知的误差类型: " << error_type_str;
            LOG_ERROR_EN << "[Degensac] Refusing unknown error type: " << error_type_str;
            return false;
        }
    }

} // namespace PluginMethods

// ✅ 使用单参数模式，自动从 CMake 读取 PLUGIN_NAME（实现单一信息源）
REGISTRATION_PLUGIN(PluginMethods::DegensacTwoViewEstimator)
