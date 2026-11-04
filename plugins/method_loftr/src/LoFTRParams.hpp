/**
 * @file LoFTRParams.hpp
 * @brief Parameter configuration for LoFTR detector-free matching plugin |
 * LoFTR无检测器匹配插件的参数配置
 * @copyright Copyright (c) 2024 PoSDK
 */

#pragma once

#include <po_core.hpp>
#include <po_core/po_logger.hpp>
#include <string>

namespace PluginMethods
{
    using namespace PoSDK;
    using namespace Interface;

    /**
     * @brief LoFTR model type enumeration | LoFTR模型类型枚举
     */
    enum class LoFTRModelType
    {
        Indoor,  // Indoor pretrained model | 室内预训练模型
        Outdoor  // Outdoor pretrained model | 室外预训练模型
    };

    /**
     * @brief Run mode enumeration | 运行模式枚举
     */
    enum class LoFTRRunMode
    {
        Fast  // Fast mode (no GUI) | 快速模式（无GUI）
    };

    /**
     * @brief Base parameters | 基础参数
     */
    struct LoFTRBaseParameters
    {
        LoFTRRunMode run_mode = LoFTRRunMode::Fast;
        int num_threads = 1;
        bool enable_profiling = true;
        std::string metrics_config = "time";
        int log_level = 0;
    };

    /**
     * @brief LoFTR model parameters | LoFTR模型参数
     */
    struct LoFTRModelParameters
    {
        LoFTRModelType model_type = LoFTRModelType::Outdoor;
        int max_keypoints = 0;
        int max_keypoints_per_image = 0;
        float confidence_threshold = 0.2f;
        int resize_max = 1024;
        std::string python_executable;
        std::string execution_backend = "ssh";
        std::string ssh_target = "remoteMAC";
        int ssh_port = 0;
        std::string ssh_identity_file;
        std::string ssh_python_executable = "~/.local/share/PoSDK/LoFTR/current/bin/python3.11";
        std::string ssh_work_dir = "~/.cache/PoSDK/LoFTR";
        std::string ssh_checkpoint_path;
        int ssh_connect_timeout = 15;
        int ssh_run_timeout = 3600;
        int ssh_result_limit_mb = 8192;
        bool ssh_keep_work_dir = false;
        std::string script_path = "";
        std::string device = "auto";  // "auto", "mps", "cuda", "cpu"
        std::string checkpoint_path;
        std::string pair_cache_dir;
        bool rotation_rescue = false;
        std::string base_pair_cache_dir;
        std::string refine_endpoint = "second";
        std::string pair_geometry = "none";
        double geometry_threshold = 1.0;
        int cpu_threads = 6;
        double keypoint_merge_radius = 1.0;  // Original-image pixels.
        double keypoint_cell_size = 0.0;  // 0 follows merge radius; original-image pixels.
    };

    /**
     * @brief Feature/match export parameters | 特征/匹配导出参数
     */
    struct LoFTRExportParameters
    {
        bool export_features = false;
        std::string export_fea_path = "storage/features";
        bool export_matches = false;
        std::string export_match_path = "storage/matches";
    };

    /**
     * @brief Overall LoFTR plugin parameter container | LoFTR插件总参数容器
     */
    struct LoFTRParameters
    {
        LoFTRBaseParameters base;
        LoFTRModelParameters loftr;
        LoFTRExportParameters export_params;

        void LoadFromConfig(Interface::MethodPreset *config_loader);
        bool Validate(Interface::MethodPreset *method_ptr = nullptr) const;
        void PrintSummary(Interface::MethodPreset *method_ptr = nullptr) const;
    };

    /**
     * @brief Parameter conversion utilities | 参数转换工具
     */
    class LoFTRParameterConverter
    {
    public:
        static std::string ModelTypeToString(LoFTRModelType type);
        static LoFTRModelType StringToModelType(const std::string &str);
    };

} // namespace PluginMethods
