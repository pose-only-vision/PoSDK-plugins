/**
 * @file LoFTRMatcher.cpp
 * @brief LoFTR detector-free matcher C++ wrapper implementation |
 * LoFTR无检测器匹配器C++封装实现
 * @copyright Copyright (c) 2024 PoSDK
 */

#include "LoFTRMatcher.hpp"
#include "../plugin_runtime_path.hpp"
#include <po_core/po_logger.hpp>
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <random>
#include <cmath>
#include <unordered_set>
#include <unistd.h>

namespace PluginMethods
{
    namespace
    {
        void LoFTRRuntimeFileAnchor() {}
    }

    using namespace PoSDK;

    LoFTRMatcher::LoFTRMatcher(const LoFTRModelParameters &params)
        : params_(params), initialized_(false)
    {
    }

    LoFTRMatcher::~LoFTRMatcher()
    {
        if (!temp_dir_.empty() && std::filesystem::exists(temp_dir_))
        {
            std::error_code ec;
            std::filesystem::remove_all(temp_dir_, ec);
        }
    }

    bool LoFTRMatcher::Initialize()
    {
        if (initialized_)
            return true;

        if (!CheckEnvironment(params_.python_executable, params_.execution_backend != "ssh"))
        {
            LOG_ERROR_ZH << "[LoFTRMatcher] Python环境检查失败: " << params_.python_executable;
            LOG_ERROR_EN << "[LoFTRMatcher] Python environment check failed for: " << params_.python_executable;
            return false;
        }

        script_path_ = params_.script_path.empty() ? FindLoFTRScript() : params_.script_path;
        if (script_path_.empty())
        {
            LOG_ERROR_ZH << "[LoFTRMatcher] 未找到LoFTR Python脚本";
            LOG_ERROR_EN << "[LoFTRMatcher] LoFTR Python script not found";
            return false;
        }
        if (params_.execution_backend == "ssh")
        {
            const auto runner = std::filesystem::path(script_path_).parent_path() / "loftr_ssh_runner.py";
            const auto agent = runner.parent_path() / "loftr_ssh_agent.py";
            if (!std::filesystem::is_regular_file(runner) || !std::filesystem::is_regular_file(agent))
            {
                LOG_ERROR_EN << "[LoFTRMatcher] The installed plugin is missing its SSH transport resources";
                return false;
            }
            ssh_script_path_ = runner.string();
        }

        temp_dir_ = CreateTempDirectory();
        if (temp_dir_.empty())
        {
            LOG_ERROR_ZH << "[LoFTRMatcher] 创建临时目录失败";
            LOG_ERROR_EN << "[LoFTRMatcher] Failed to create temporary directory";
            return false;
        }

        initialized_ = true;
        LOG_INFO_ZH << "[LoFTRMatcher] LoFTR匹配器初始化成功";
        LOG_INFO_ZH << "[LoFTRMatcher] 脚本路径: " << script_path_;
        LOG_INFO_ZH << "[LoFTRMatcher] 临时目录: " << temp_dir_;
        LOG_INFO_ZH << "[LoFTRMatcher] 模型类型: "
                    << LoFTRParameterConverter::ModelTypeToString(params_.model_type);
        LOG_INFO_ZH << "[LoFTRMatcher] 设备: " << params_.device;
        LOG_INFO_EN << "[LoFTRMatcher] LoFTR matcher initialized successfully";
        LOG_INFO_EN << "[LoFTRMatcher] Script path: " << script_path_;
        LOG_INFO_EN << "[LoFTRMatcher] Temp dir: " << temp_dir_;
        LOG_INFO_EN << "[LoFTRMatcher] Model type: "
                    << LoFTRParameterConverter::ModelTypeToString(params_.model_type);
        LOG_INFO_EN << "[LoFTRMatcher] Device: " << params_.device;

        return true;
    }

    bool LoFTRMatcher::MatchImagePair(
        const std::string &img1_path,
        const std::string &img2_path,
        LoFTRMatchResult &result)
    {
        return MatchImagePairs({img1_path, img2_path},
            [&result](size_t, size_t, const LoFTRMatchResult &pair) {
                result = pair;
                return true;
            });
    }

    bool LoFTRMatcher::MatchImagePairs(
        const std::vector<std::string> &image_paths,
        const std::function<bool(size_t, size_t, const LoFTRMatchResult &)> &consume)
    {
        if (image_paths.size() < 2 || image_paths.size() > 1000 || !consume ||
            (!initialized_ && !Initialize())) return false;
        try {
            const auto request = temp_dir_ + "/images.txt";
            std::ofstream input(request);
            input << image_paths.size() << "\n";
            for (const auto &path : image_paths) {
                if (path.empty() || path.find_first_of("\r\n") != std::string::npos)
                    return false;
                input << path << "\n";
            }
            input.close();
            if (!input) return false;
            std::ostringstream command;
            command << PluginRuntime::QuotePosixShellArgument(params_.python_executable)
                    << " -u " << PluginRuntime::QuotePosixShellArgument(params_.execution_backend == "ssh" ? ssh_script_path_ : script_path_);
            if (params_.execution_backend == "ssh")
            {
                command << " --worker " << PluginRuntime::QuotePosixShellArgument(script_path_)
                        << " --ssh-target " << PluginRuntime::QuotePosixShellArgument(params_.ssh_target)
                        << " --ssh-port " << params_.ssh_port
                        << " --ssh-python-executable " << PluginRuntime::QuotePosixShellArgument(params_.ssh_python_executable)
                        << " --ssh-work-dir " << PluginRuntime::QuotePosixShellArgument(params_.ssh_work_dir)
                        << " --ssh-connect-timeout " << params_.ssh_connect_timeout
                        << " --ssh-run-timeout " << params_.ssh_run_timeout
                        << " --ssh-result-limit-mb " << params_.ssh_result_limit_mb;
                if (!params_.ssh_identity_file.empty())
                    command << " --ssh-identity-file " << PluginRuntime::QuotePosixShellArgument(params_.ssh_identity_file);
                if (!params_.ssh_checkpoint_path.empty())
                    command << " --ssh-checkpoint-path " << PluginRuntime::QuotePosixShellArgument(params_.ssh_checkpoint_path);
                if (params_.ssh_keep_work_dir)
                    command << " --ssh-keep-work-dir";
            }
            command
                    << " --input " << PluginRuntime::QuotePosixShellArgument(request)
                    << " --output-dir " << PluginRuntime::QuotePosixShellArgument(temp_dir_)
                    << " --model-type " << LoFTRParameterConverter::ModelTypeToString(params_.model_type)
                    << " --max-keypoints " << params_.max_keypoints
                    << " --max-keypoints-per-image " << params_.max_keypoints_per_image
                    << " --keypoint-merge-radius " << params_.keypoint_merge_radius
                    << " --keypoint-cell-size " << params_.keypoint_cell_size
                    << " --confidence-threshold " << params_.confidence_threshold
                    << " --resize-max " << params_.resize_max
                    << " --refine-endpoint " << PluginRuntime::QuotePosixShellArgument(params_.refine_endpoint)
                    << " --device " << PluginRuntime::QuotePosixShellArgument(params_.device)
                    << " --pair-geometry " << PluginRuntime::QuotePosixShellArgument(params_.pair_geometry)
                    << " --geometry-threshold " << params_.geometry_threshold
                    << " --cpu-threads " << params_.cpu_threads;
            if (!params_.checkpoint_path.empty())
                command << " --checkpoint " << PluginRuntime::QuotePosixShellArgument(params_.checkpoint_path);
            if (!params_.pair_cache_dir.empty())
                command << " --pair-cache-dir " << PluginRuntime::QuotePosixShellArgument(params_.pair_cache_dir);
            if (params_.rotation_rescue)
                command << " --rotation-rescue";
            if (!params_.base_pair_cache_dir.empty())
                command << " --base-pair-cache-dir " << PluginRuntime::QuotePosixShellArgument(params_.base_pair_cache_dir);
            command << " 2>&1";
            if (std::system(command.str().c_str()) != 0) {
                LOG_ERROR_EN << "[LoFTRMatcher] Batch matching failed";
                return false;
            }
            image_keypoints_.clear();
            image_keypoints_.resize(image_paths.size());
            for (size_t i = 0; i < image_paths.size(); ++i) {
                std::ifstream features(temp_dir_ + "/features_" + std::to_string(i) + ".txt");
                int count;
                if (!(features >> count) || count < 0 || count > 2000000 ||
                    (params_.max_keypoints_per_image > 0 && count > params_.max_keypoints_per_image))
                    return false;
                for (int k = 0; k < count; ++k) {
                    float x, y;
                    if (!(features >> x >> y) || !std::isfinite(x) || !std::isfinite(y)) return false;
                    image_keypoints_[i].emplace_back(cv::Point2f(x, y), 1.0f);
                }
                features >> std::ws;
                if (!features.eof()) return false;
            }
            for (size_t i = 0; i < image_paths.size(); ++i) {
                for (size_t j = i + 1; j < image_paths.size(); ++j) {
                    const auto path = temp_dir_ + "/pair_" + std::to_string(i) + "_" +
                                      std::to_string(j) + ".txt";
                    LoFTRMatchResult result;
                    if (!LoadResultsFromFile(path, result, i, j) || !consume(i, j, result)) {
                        LOG_ERROR_EN << "[LoFTRMatcher] Invalid pair result: " << path;
                        return false;
                    }
                    CleanupTempFiles({path});
                }
            }
            CleanupTempFiles({request});
            return true;
        } catch (const std::exception &error) {
            LOG_ERROR_EN << "[LoFTRMatcher] Batch matching error: " << error.what();
            return false;
        }
    }

    bool LoFTRMatcher::CheckEnvironment(const std::string &python_exe, bool require_ml)
    {
        const auto command = PluginRuntime::QuotePosixShellArgument(python_exe) +
            (require_ml ? " -c 'import torch, kornia, numpy, cv2' >/dev/null 2>&1" :
                          " -c 'import sys, json, subprocess, tarfile; assert sys.version_info >= (3, 9)' >/dev/null 2>&1");
        if (std::system(command.c_str()) != 0) {
            LOG_ERROR_EN << "[LoFTRMatcher] Python " << (require_ml ? "model environment" : "SSH controller") << " unavailable: " << python_exe;
            return false;
        }

        LOG_INFO_ZH << "[LoFTRMatcher] Python环境检查通过: " << python_exe;
        LOG_INFO_EN << "[LoFTRMatcher] Python environment check passed for: " << python_exe;
        return true;
    }

    std::string LoFTRMatcher::FindLoFTRScript()
    {
        const std::filesystem::path script_path =
            PoSDK::PluginRuntime::FindRuntimeFile(
                &LoFTRRuntimeFileAnchor,
                "loftr_batch_worker.py",
                POSDK_PLUGIN_DEVELOPER_FALLBACKS(
                    "plugins/methods/loftr_batch_worker.py",
                    "output/plugins/methods/loftr_batch_worker.py",
                    "../output/plugins/methods/loftr_batch_worker.py",
                    "../../output/plugins/methods/loftr_batch_worker.py",
                    "src/plugins/methods/LoFTR/loftr_batch_worker.py",
                    "../plugins/methods/LoFTR/loftr_batch_worker.py",
                    "../../plugins/methods/LoFTR/loftr_batch_worker.py"));
        if (!script_path.empty())
        {
            LOG_INFO_ZH << "[LoFTRMatcher] 在以下位置找到LoFTR脚本: " << script_path.string();
            LOG_INFO_EN << "[LoFTRMatcher] Found LoFTR script at: " << script_path.string();
            return script_path.string();
        }

        LOG_ERROR_ZH << "[LoFTRMatcher] 在标准位置未找到LoFTR脚本";
        LOG_ERROR_EN << "[LoFTRMatcher] LoFTR script not found in standard locations";
        return "";
    }

    std::string LoFTRMatcher::CreateTempDirectory()
    {
        try
        {
            const auto pattern = (std::filesystem::temp_directory_path() / "posdk-loftr-XXXXXX").string();
            std::vector<char> buffer(pattern.begin(), pattern.end());
            buffer.push_back('\0');
            if (const auto path = ::mkdtemp(buffer.data())) return path;
        }
        catch (const std::exception &e)
        {
            LOG_ERROR_ZH << "[LoFTRMatcher] 创建临时目录时出现异常: " << e.what();
            LOG_ERROR_EN << "[LoFTRMatcher] Exception creating temp directory: " << e.what();
        }

        return "";
    }

    bool LoFTRMatcher::LoadResultsFromFile(
        const std::string &output_path,
        LoFTRMatchResult &result, size_t image0, size_t image1)
    {
        try
        {
            std::ifstream file(output_path);
            if (!file.is_open())
            {
                return false;
            }

            result.keypoints0.clear();
            result.keypoints1.clear();
            result.matches.clear();
            result.confidences.clear();
            result.feature_ids0.clear();
            result.feature_ids1.clear();
            result.geometric_inliers.clear();

            int count;
            if (!(file >> count) || count < 0 || count > 2000000 ||
                (params_.max_keypoints > 0 && count > params_.max_keypoints))
                return false;
            std::unordered_set<size_t> used0, used1;
            for (int i = 0; i < count; ++i) {
                size_t id0, id1;
                float confidence;
                int geometric_inlier;
                if (!(file >> id0 >> id1 >> confidence >> geometric_inlier) ||
                    (geometric_inlier != 0 && geometric_inlier != 1) ||
                    id0 >= image_keypoints_.at(image0).size() ||
                    id1 >= image_keypoints_.at(image1).size() ||
                    !used0.insert(id0).second || !used1.insert(id1).second ||
                    !std::isfinite(confidence) || confidence < 0 || confidence > 1)
                    return false;
                result.keypoints0.push_back(image_keypoints_[image0][id0]);
                result.keypoints1.push_back(image_keypoints_[image1][id1]);
                result.feature_ids0.push_back(id0);
                result.feature_ids1.push_back(id1);
                result.geometric_inliers.push_back(geometric_inlier != 0);
                result.matches.emplace_back(i, i, 1.0f - confidence);
                result.confidences.push_back(confidence);
            }
            file >> std::ws;
            return file.eof(); // An empty pair is valid; truncated/extra data is not.
        }
        catch (const std::exception &e)
        {
            LOG_ERROR_ZH << "[LoFTRMatcher] 加载匹配结果时出现异常: " << e.what();
            LOG_ERROR_EN << "[LoFTRMatcher] Exception loading results: " << e.what();
            return false;
        }
    }

    void LoFTRMatcher::CleanupTempFiles(const std::vector<std::string> &file_paths)
    {
        for (const std::string &path : file_paths)
        {
            try
            {
                if (std::filesystem::exists(path))
                {
                    std::filesystem::remove(path);
                }
            }
            catch (const std::exception &e)
            {
                LOG_WARNING_ZH << "[LoFTRMatcher] 清理文件失败 " << path << ": " << e.what();
                LOG_WARNING_EN << "[LoFTRMatcher] Failed to cleanup file " << path << ": " << e.what();
            }
        }
    }

} // namespace PluginMethods
