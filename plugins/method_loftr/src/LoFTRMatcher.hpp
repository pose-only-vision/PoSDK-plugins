/**
 * @file LoFTRMatcher.hpp
 * @brief LoFTR detector-free matcher C++ wrapper |
 * LoFTR无检测器匹配器C++封装
 * @copyright Copyright (c) 2024 PoSDK
 */

#pragma once

#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>
#include <vector>
#include <string>
#include <functional>
#include "LoFTRParams.hpp"
#include <po_core/po_logger.hpp>

namespace PluginMethods
{
    /**
     * @brief LoFTR match result for a single image pair |
     * 单个图像对的LoFTR匹配结果
     */
    struct LoFTRMatchResult
    {
        std::vector<cv::KeyPoint> keypoints0;
        std::vector<cv::KeyPoint> keypoints1;
        std::vector<cv::DMatch> matches;
        std::vector<float> confidences;
        std::vector<size_t> feature_ids0;
        std::vector<size_t> feature_ids1;
        std::vector<bool> geometric_inliers;
    };

    /**
     * @brief LoFTR detector-free matcher wrapper |
     * LoFTR无检测器匹配器封装
     *
     * Calls Python script via subprocess to perform LoFTR matching.
     * 通过子进程调用Python脚本执行LoFTR匹配。
     */
    class LoFTRMatcher
    {
    public:
        explicit LoFTRMatcher(const LoFTRModelParameters &params);
        ~LoFTRMatcher();

        bool Initialize();

        /**
         * @brief Match an image pair using LoFTR |
         * 使用LoFTR匹配图像对
         * @param img1_path Path to image 1 | 图像1路径
         * @param img2_path Path to image 2 | 图像2路径
         * @param result Output match result | 输出匹配结果
         * @return Whether matching was successful | 是否匹配成功
         */
        bool MatchImagePair(
            const std::string &img1_path,
            const std::string &img2_path,
            LoFTRMatchResult &result);

        bool MatchImagePairs(
            const std::vector<std::string> &image_paths,
            const std::function<bool(size_t, size_t, const LoFTRMatchResult &)> &consume);

        const std::vector<std::vector<cv::KeyPoint>> &ImageKeypoints() const { return image_keypoints_; }

        static bool CheckEnvironment(const std::string &python_exe = "python3", bool require_ml = true);

        static std::string GetMatcherName() { return "LOFTR"; }

    private:
        LoFTRModelParameters params_;
        std::string script_path_;
        std::string ssh_script_path_;
        std::string temp_dir_;
        bool initialized_;
        std::vector<std::vector<cv::KeyPoint>> image_keypoints_;

        std::string FindLoFTRScript();
        std::string CreateTempDirectory();

        bool LoadResultsFromFile(
            const std::string &output_path,
            LoFTRMatchResult &result, size_t image0, size_t image1);

        void CleanupTempFiles(const std::vector<std::string> &file_paths);
    };

} // namespace PluginMethods
