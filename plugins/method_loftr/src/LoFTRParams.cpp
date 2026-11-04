/**
 * @file LoFTRParams.cpp
 * @brief Parameter configuration implementation for LoFTR plugin |
 * LoFTR插件参数配置实现
 * @copyright Copyright (c) 2024 PoSDK
 */

#include "LoFTRParams.hpp"
#include <boost/algorithm/string.hpp>
#include <cmath>
#include <filesystem>

namespace PluginMethods
{
    using namespace PoSDK;

    void LoFTRParameters::LoadFromConfig(Interface::MethodPreset *config_loader)
    {
        // Base parameters | 基础参数
        base.num_threads = static_cast<int>(
            config_loader->GetOptionAsIndexT("num_threads", 1));
        base.enable_profiling =
            config_loader->GetOptionAsBool("enable_profiling", true);
        base.metrics_config =
            config_loader->GetOptionAsString("metrics_config", "time");
        base.log_level = static_cast<int>(
            config_loader->GetOptionAsIndexT("log_level", 0));

        std::string run_mode_str =
            config_loader->GetOptionAsString("run_mode", "fast");
        if (boost::iequals(run_mode_str, "fast"))
        {
            base.run_mode = LoFTRRunMode::Fast;
        }

        // LoFTR options belong to the main [method_loftr] section.
        std::string model_type_str =
            config_loader->GetOptionAsString("model_type", "outdoor");
        loftr.model_type = LoFTRParameterConverter::StringToModelType(model_type_str);

        loftr.max_keypoints = static_cast<int>(
            config_loader->GetOptionAsIndexT("max_keypoints", 0));
        loftr.max_keypoints_per_image = static_cast<int>(config_loader->GetOptionAsIndexT("max_keypoints_per_image", 0));
        loftr.confidence_threshold =
            config_loader->GetOptionAsFloat("confidence_threshold", 0.2f);
        loftr.resize_max = static_cast<int>(
            config_loader->GetOptionAsIndexT("resize_max", 1024));
        loftr.python_executable =
            config_loader->GetOptionAsString("python_executable", "");
        loftr.execution_backend = config_loader->GetOptionAsString("execution_backend", "ssh");
        loftr.ssh_target = config_loader->GetOptionAsString("ssh_target", "remoteMAC");
        loftr.ssh_port = static_cast<int>(config_loader->GetOptionAsIndexT("ssh_port", 0));
        loftr.ssh_identity_file = config_loader->GetOptionAsString("ssh_identity_file", "");
        loftr.ssh_python_executable = config_loader->GetOptionAsString("ssh_python_executable", "~/.local/share/PoSDK/LoFTR/current/bin/python3.11");
        loftr.ssh_work_dir = config_loader->GetOptionAsString("ssh_work_dir", "~/.cache/PoSDK/LoFTR");
        loftr.ssh_checkpoint_path = config_loader->GetOptionAsString("ssh_checkpoint_path", "");
        loftr.ssh_connect_timeout = static_cast<int>(config_loader->GetOptionAsIndexT("ssh_connect_timeout", 15));
        loftr.ssh_run_timeout = static_cast<int>(config_loader->GetOptionAsIndexT("ssh_run_timeout", 3600));
        loftr.ssh_result_limit_mb = static_cast<int>(config_loader->GetOptionAsIndexT("ssh_result_limit_mb", 8192));
        loftr.ssh_keep_work_dir = config_loader->GetOptionAsBool("ssh_keep_work_dir", false);
        loftr.script_path =
            config_loader->GetOptionAsString("script_path", "");
        loftr.device =
            config_loader->GetOptionAsString("device", "auto");
        loftr.checkpoint_path = config_loader->GetOptionAsString("checkpoint_path", "");
        loftr.pair_cache_dir = config_loader->GetOptionAsString("pair_cache_dir", "");
        loftr.rotation_rescue = config_loader->GetOptionAsBool("rotation_rescue", false);
        loftr.base_pair_cache_dir = config_loader->GetOptionAsString("base_pair_cache_dir", "");
        loftr.refine_endpoint = config_loader->GetOptionAsString("refine_endpoint", "second");
        loftr.pair_geometry = config_loader->GetOptionAsString("pair_geometry", "none");
        loftr.geometry_threshold = config_loader->GetOptionAsFloat("geometry_threshold", 1.0);
        loftr.cpu_threads = static_cast<int>(config_loader->GetOptionAsIndexT("cpu_threads", 6));
        loftr.keypoint_merge_radius = config_loader->GetOptionAsFloat("keypoint_merge_radius", 1.0);
        loftr.keypoint_cell_size = config_loader->GetOptionAsFloat("keypoint_cell_size", 0.0);
        const auto managed = config_loader->GetOptionAsString("python_bin_folder", "");
        if (!managed.empty() && loftr.python_executable.empty())
        {
            const auto python = std::filesystem::canonical(std::filesystem::path(managed) / "python3.11");
            loftr.python_executable = python.string();
            if (loftr.execution_backend == "local" && loftr.checkpoint_path.empty())
                loftr.checkpoint_path = (python.parent_path().parent_path() / "runtime/weights" /
                    (loftr.model_type == LoFTRModelType::Indoor ? "loftr_indoor.ckpt" : "loftr_outdoor.ckpt")).string();
        }

        // Export parameters | 导出参数
        export_params.export_features =
            config_loader->GetOptionAsBool("export_features", false);
        export_params.export_fea_path =
            config_loader->GetOptionAsPath("export_fea_path", "", "storage/features");
        export_params.export_matches =
            config_loader->GetOptionAsBool("export_matches", false);
        export_params.export_match_path =
            config_loader->GetOptionAsPath("export_match_path", "", "storage/matches");
    }

    bool LoFTRParameters::Validate(Interface::MethodPreset *method_ptr) const
    {
        if (loftr.execution_backend != "local" && loftr.execution_backend != "ssh")
        {
            LOG_ERROR_EN << "execution_backend must be local or ssh";
            return false;
        }
        if (loftr.execution_backend == "ssh" &&
            (loftr.ssh_target.empty() || loftr.ssh_target.front() == '-' ||
             loftr.ssh_python_executable.empty() || loftr.ssh_work_dir.empty() ||
             loftr.ssh_port < 0 || loftr.ssh_port > 65535 ||
             loftr.ssh_connect_timeout < 1 || loftr.ssh_connect_timeout > 120 ||
             loftr.ssh_run_timeout < 5 || loftr.ssh_run_timeout > 86400 ||
             loftr.ssh_result_limit_mb < 1 || loftr.ssh_result_limit_mb > 65536 ||
             (!loftr.checkpoint_path.empty() && !loftr.ssh_checkpoint_path.empty())))
        {
            LOG_ERROR_EN << "Invalid LoFTR SSH configuration; use an SSH alias and the supported remote Python environment";
            return false;
        }
        for (const auto &value : {loftr.python_executable, loftr.script_path, loftr.checkpoint_path,
             loftr.pair_cache_dir, loftr.base_pair_cache_dir, loftr.ssh_target, loftr.ssh_identity_file,
             loftr.ssh_python_executable, loftr.ssh_work_dir, loftr.ssh_checkpoint_path})
        {
            if (value.find_first_of("\r\n") != std::string::npos || value.find('\0') != std::string::npos)
                return false;
        }
        if (loftr.device != "auto" && loftr.device != "mps" && loftr.device != "cuda" && loftr.device != "cpu")
            return false;
        if (loftr.max_keypoints < 0 || loftr.max_keypoints_per_image < 0)
        {
            LOG_ERROR_ZH << "匹配或特征上限不能为负数（0为不限）";
            LOG_ERROR_EN << "Match and feature limits must be nonnegative (0 is unlimited)";
            return false;
        }
        if ((loftr.refine_endpoint != "first" && loftr.refine_endpoint != "second") ||
            (!loftr.base_pair_cache_dir.empty() && !loftr.rotation_rescue) ||
            (loftr.rotation_rescue && loftr.execution_backend == "local" && loftr.checkpoint_path.empty()))
        {
            LOG_ERROR_EN << "Invalid LoFTR refinement endpoint or rotation-rescue cache/checkpoint";
            return false;
        }
        if (!std::isfinite(loftr.confidence_threshold) || loftr.confidence_threshold < 0.0f || loftr.confidence_threshold > 1.0f)
        {
            LOG_ERROR_ZH << "confidence_threshold必须在[0, 1]范围内";
            LOG_ERROR_EN << "confidence_threshold must be in range [0, 1]";
            return false;
        }
        if (loftr.resize_max < 8 || loftr.resize_max > 4096)
        {
            LOG_ERROR_ZH << "resize_max必须在8到4096之间";
            LOG_ERROR_EN << "resize_max must be between 8 and 4096";
            return false;
        }
        if (loftr.cpu_threads < 1 || loftr.cpu_threads > 64 ||
            !(loftr.keypoint_merge_radius > 0.0 && loftr.keypoint_merge_radius <= 8.0))
        {
            LOG_ERROR_EN << "Invalid LoFTR CPU thread count or keypoint merge radius";
            return false;
        }
        if ((loftr.pair_geometry != "none" && loftr.pair_geometry != "magsac" && loftr.pair_geometry != "magsac_seed" && loftr.pair_geometry != "magsac_post") ||
            !(loftr.geometry_threshold >= .1 && loftr.geometry_threshold <= 4.0) ||
            !(loftr.keypoint_cell_size >= 0.0 && loftr.keypoint_cell_size <= 32.0)) return false;
        return true;
    }

    void LoFTRParameters::PrintSummary(Interface::MethodPreset *method_ptr) const
    {
        LOG_INFO_ZH << "========== LoFTR 参数摘要 ==========";
        LOG_INFO_EN << "========== LoFTR Parameter Summary ==========";
        LOG_INFO_ZH << "  执行后端: " << loftr.execution_backend;
        LOG_INFO_EN << "  Execution backend: " << loftr.execution_backend;
        if (loftr.execution_backend == "ssh")
        {
            LOG_INFO_ZH << "  SSH 目标: " << loftr.ssh_target;
            LOG_INFO_EN << "  SSH target: " << loftr.ssh_target;
        }
        LOG_INFO_ZH << "  模型类型: " << LoFTRParameterConverter::ModelTypeToString(loftr.model_type);
        LOG_INFO_EN << "  Model type: " << LoFTRParameterConverter::ModelTypeToString(loftr.model_type);
        LOG_INFO_ZH << "  最大关键点数: " << loftr.max_keypoints;
        LOG_INFO_EN << "  Max keypoints: " << loftr.max_keypoints;
        LOG_INFO_ZH << "  置信度阈值: " << loftr.confidence_threshold;
        LOG_INFO_EN << "  Confidence threshold: " << loftr.confidence_threshold;
        LOG_INFO_ZH << "  图像最大边长: " << loftr.resize_max;
        LOG_INFO_EN << "  Resize max: " << loftr.resize_max;
        LOG_INFO_ZH << "  Python路径: " << loftr.python_executable;
        LOG_INFO_EN << "  Python executable: " << loftr.python_executable;
        LOG_INFO_ZH << "  设备: " << loftr.device;
        LOG_INFO_EN << "  Device: " << loftr.device;
        LOG_INFO_ZH << "  线程数: " << base.num_threads;
        LOG_INFO_EN << "  Threads: " << base.num_threads;
        LOG_INFO_ZH << "==========================================";
        LOG_INFO_EN << "==========================================";
    }

    std::string LoFTRParameterConverter::ModelTypeToString(LoFTRModelType type)
    {
        switch (type)
        {
        case LoFTRModelType::Indoor:
            return "indoor";
        case LoFTRModelType::Outdoor:
            return "outdoor";
        default:
            return "outdoor";
        }
    }

    LoFTRModelType LoFTRParameterConverter::StringToModelType(const std::string &str)
    {
        if (boost::iequals(str, "indoor"))
        {
            return LoFTRModelType::Indoor;
        }
        return LoFTRModelType::Outdoor;
    }

} // namespace PluginMethods
