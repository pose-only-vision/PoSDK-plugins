/**
 * @file loftr_fast_mode.cpp
 * @brief LoFTR fast mode implementation - core matching logic |
 * LoFTR快速模式实现 - 核心匹配逻辑
 * @copyright Copyright (c) 2024 PoSDK
 */

#include "loftr_pipeline.hpp"
#include "LoFTRMatcher.hpp"
#include <po_core/po_logger.hpp>
#include <po_core/types.hpp>
#include <po_core/ProfilerManager.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <algorithm>
#include <numeric>
#include <unordered_set>

namespace PluginMethods
{
    using namespace PoSDK;

    DataPtr LoFTRPipeline::RunFastMode()
    {
        try
        {
            // 1. Get input image paths | 获取输入图像路径
            DataPtr data_images_ptr = nullptr;
            if (!prior_info_.empty() &&
                prior_info_.find("data_images") != prior_info_.end() &&
                prior_info_["data_images"] != nullptr)
            {
                data_images_ptr = prior_info_["data_images"];
            }
            else
            {
                data_images_ptr = required_package_["data_images"];
            }

            if (!data_images_ptr)
            {
                LOG_ERROR_ZH << "没有输入图像数据！";
                LOG_ERROR_EN << "No input images data!";
                return nullptr;
            }

            // 2. Get image path list | 获取图像路径列表
            ImagePathsPtr image_paths_ptr = GetDataPtr<ImagePaths>(data_images_ptr);
            if (!image_paths_ptr || image_paths_ptr->empty())
            {
                LOG_ERROR_ZH << "图像路径为空！";
                LOG_ERROR_EN << "Empty image paths!";
                return nullptr;
            }

            size_t num_images = image_paths_ptr->size();
            LOG_INFO_ZH << "LoFTR快速模式: 共 " << num_images << " 张图像";
            LOG_INFO_EN << "LoFTR fast mode: " << num_images << " images total";

            // 3. Create output data containers | 创建输出数据容器
            DataPtr features_data_ptr = FactoryData::Create("data_features");
            DataPtr matches_data_ptr = FactoryData::Create("data_matches");
            if (!features_data_ptr || !matches_data_ptr)
            {
                LOG_ERROR_ZH << "创建数据容器失败！";
                LOG_ERROR_EN << "Failed to create data containers!";
                return nullptr;
            }

            FeaturesInfoPtr features_info_ptr = GetDataPtr<FeaturesInfo>(features_data_ptr);
            MatchesPtr matches_ptr = GetDataPtr<Matches>(matches_data_ptr);

            // 4. Initialize LoFTR matcher | 初始化LoFTR匹配器
            LoFTRMatcher matcher(params_.loftr);
            if (!matcher.Initialize())
            {
                LOG_ERROR_ZH << "LoFTR匹配器初始化失败";
                LOG_ERROR_EN << "LoFTR matcher initialization failed";
                return nullptr;
            }

            // 5. Build image path vector and initialize per-image keypoints |
            //    构建图像路径向量，并初始化每张图像的关键点
            std::vector<std::string> image_path_list;
            std::vector<IndexT> view_ids;

            for (size_t i = 0; i < num_images; ++i)
            {
                image_path_list.push_back((*image_paths_ptr)[i].first);
                view_ids.push_back(static_cast<IndexT>(i));
            }

            // 6. Perform pairwise matching | 执行全对匹配
            std::string metrics_config = GetOptionAsString("metrics_config", "time");

            LOG_INFO_ZH << "========== 开始LoFTR端到端匹配流程 ==========";
            LOG_INFO_EN << "========== Starting LoFTR End-to-End Matching ==========";

            size_t total_pairs = num_images * (num_images - 1) / 2;
            size_t successful_pairs = 0;


            {
                POSDK_START(params_.base.enable_profiling, metrics_config);
                PROFILER_STAGE("LoFTR Matching");

                const bool matched = matcher.MatchImagePairs(image_path_list,
                    [&](size_t i, size_t j, const LoFTRMatchResult &result) {
                        types::IdMatches id_matches;
                        id_matches.reserve(result.matches.size());
                        for (size_t k = 0; k < result.matches.size(); ++k) {
                            types::IdMatch remapped;
                            remapped.i = static_cast<IndexT>(result.feature_ids0.at(k));
                            remapped.j = static_cast<IndexT>(result.feature_ids1.at(k));
                            remapped.is_inlier = result.geometric_inliers.at(k);
                            id_matches.push_back(remapped);
                        }
                        if (!id_matches.empty()) {
                            (*matches_ptr)[types::ViewPair(view_ids[i], view_ids[j])] =
                                std::move(id_matches);
                            ++successful_pairs;
                        }
                        return true;
                    });
                if (!matched) return nullptr;

                PROFILER_END();
                if (SHOULD_LOG(DEBUG))
                {
                    PROFILER_PRINT_STATS(params_.base.enable_profiling);
                }
            }

            LOG_INFO_ZH << "========== LoFTR匹配完成 ==========";
            LOG_INFO_EN << "========== LoFTR Matching Complete ==========";
            LOG_INFO_ZH << "成功匹配对数: " << successful_pairs << "/" << total_pairs;
            LOG_INFO_EN << "Successful pairs: " << successful_pairs << "/" << total_pairs;

            // 7. Build FeaturesInfo from accumulated keypoints |
            //    从累积的关键点构建FeaturesInfo
            for (size_t i = 0; i < num_images; ++i)
            {
                IndexT vid = view_ids[i];
                const auto &kps = matcher.ImageKeypoints().at(i);

                features_info_ptr->AddImageFeatures(image_path_list[i], true);
                auto &img_feature = features_info_ptr->back();

                for (const auto &kp : kps)
                {
                    img_feature.AddFeature(kp.pt.x, kp.pt.y, kp.size, kp.angle);
                }

                LOG_DEBUG_ZH << "图像 " << vid << ": " << kps.size() << " 个关键点";
                LOG_DEBUG_EN << "Image " << vid << ": " << kps.size() << " keypoints";
            }

            // 8. Export results | 导出结果
            ExportResults(features_data_ptr, matches_data_ptr);

            LOG_INFO_ZH << "LoFTR快速模式成功完成，成功匹配对数: " << successful_pairs;
            LOG_INFO_EN << "LoFTR fast mode completed successfully with " << successful_pairs << " successful matches";

            // 9. Create output data package | 创建输出数据包
            auto data_package = std::make_shared<DataPackage>();
            data_package->AddData("data_features", features_data_ptr);
            data_package->AddData("data_matches", matches_data_ptr);

            return std::static_pointer_cast<DataIO>(data_package);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR_ZH << "RunFastMode中发生错误: " << e.what();
            LOG_ERROR_EN << "Error in RunFastMode: " << e.what();
            return nullptr;
        }
    }

} // namespace PluginMethods
