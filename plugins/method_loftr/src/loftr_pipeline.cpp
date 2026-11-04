/**
 * @file loftr_pipeline.cpp
 * @brief LoFTR detector-free feature matching pipeline implementation |
 * LoFTR无检测器特征匹配流水线实现
 * @copyright Copyright (c) 2024 PoSDK
  * @author qi Cai
 * SPDX-License-Identifier: MPL-2.0
*/

#include "loftr_pipeline.hpp"
#include <po_core/types.hpp>
#include <po_core/po_logger.hpp>
#include <filesystem>

namespace PluginMethods
{
    using namespace PoSDK;

    LoFTRPipeline::LoFTRPipeline()
    {
        LOG_DEBUG_ZH << "初始化 LoFTRPipeline...";
        LOG_DEBUG_EN << "Initializing LoFTRPipeline...";

        required_package_["data_images"] = nullptr;

        output_types_ = {"data_features", "data_matches"};

        InitializeDefaultConfigPath();
    }

    DataPtr LoFTRPipeline::Run()
    {
        try
        {
            LoadConfigurationAtRuntime();

            if (!params_.Validate(this))
            {
                LOG_ERROR_ZH << "参数验证失败";
                LOG_ERROR_EN << "Parameter validation failed";
                return nullptr;
            }

            params_.PrintSummary(this);

            switch (params_.base.run_mode)
            {
            case LoFTRRunMode::Fast:
            default:
                return RunFastMode();
            }
        }
        catch (const std::exception &e)
        {
            LOG_ERROR_ZH << "[LoFTRPipeline] 运行时错误: " << e.what();
            LOG_ERROR_EN << "[LoFTRPipeline] Error in Run: " << e.what();
            return nullptr;
        }
    }

    void LoFTRPipeline::LoadConfigurationAtRuntime()
    {
        params_.LoadFromConfig(this);

        LOG_DEBUG_ZH << "LoFTR配置加载完成";
        LOG_DEBUG_EN << "LoFTR configuration loaded";
    }

    void LoFTRPipeline::ExportResults(DataPtr features_data_ptr, DataPtr matches_data_ptr)
    {
        if (params_.export_params.export_features && features_data_ptr)
        {
            std::string fea_path = params_.export_params.export_fea_path;
            if (!fea_path.empty())
            {
                std::filesystem::create_directories(
                    std::filesystem::path(fea_path).parent_path());
                if (features_data_ptr->Save(fea_path))
                {
                    LOG_INFO_ZH << "特征数据已导出到: " << fea_path;
                    LOG_INFO_EN << "Features exported to: " << fea_path;
                }
            }
        }

        if (params_.export_params.export_matches && matches_data_ptr)
        {
            std::string match_path = params_.export_params.export_match_path;
            if (!match_path.empty())
            {
                std::filesystem::create_directories(
                    std::filesystem::path(match_path).parent_path());
                if (matches_data_ptr->Save(match_path))
                {
                    LOG_INFO_ZH << "匹配数据已导出到: " << match_path;
                    LOG_INFO_EN << "Matches exported to: " << match_path;
                }
            }
        }
    }

} // namespace PluginMethods

REGISTRATION_PLUGIN(PluginMethods::LoFTRPipeline)
