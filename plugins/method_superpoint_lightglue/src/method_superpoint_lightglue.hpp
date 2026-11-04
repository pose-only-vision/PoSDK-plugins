#pragma once

#include <po_core.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace PluginMethods
{
namespace SuperPointLightGlueDetail
{

inline constexpr std::uint32_t kProtocolVersion = 2;
inline constexpr const char *kExtractorName = "superpoint";
inline constexpr const char *kMatcherName = "superpoint_lightglue";
inline constexpr const char *kPythonVersion = "3.11.11";
inline constexpr const char *kTorchVersion = "2.2.2";
inline constexpr const char *kTorchvisionVersion = "0.17.2";
inline constexpr const char *kKorniaVersion = "0.6.12";
inline constexpr const char *kImageDecoderIdentity = "Pillow-RGB-12.3.0";
inline constexpr const char *kLightGlueCommit =
    "eb42fee2d71449efb0aa5c10549752b5d75384d8";
inline constexpr const char *kLightGlueArchiveSha256 =
    "e5c696a4c69be4e335b090ce3f70da1060f8aa52e61d463e96a930ebefd57a19";
inline constexpr const char *kLightGlueTreeSha256 =
    "78cc712925a8de33ef9be7e807d0395f916dbd4eeb28f3644243ae2a00297d33";
inline constexpr const char *kSuperPointCommit =
    "eb42fee2d71449efb0aa5c10549752b5d75384d8";
inline constexpr const char *kSuperPointModelSha256 =
    "52b6708629640ca883673b5d5c097c4ddad37d8048b33f09c8ca0d69db12c40e";
inline constexpr const char *kLightGlueModelSha256 =
    "6ff7040d0a497fc6639337946d7538dae07428c18f77a067a0b5a960e7cc551a";

struct WorkerKeypoint
{
    float x = 0.0F;
    float y = 0.0F;
    float score = 0.0F;
};

struct WorkerMatch
{
    std::uint32_t feature_i = 0;
    std::uint32_t feature_j = 0;
    float score = 0.0F;
};

struct WorkerPair
{
    std::uint32_t image_i = 0;
    std::uint32_t image_j = 0;
    std::vector<WorkerMatch> matches;
};

struct WorkerResult
{
    std::string extractor;
    std::string matcher;
    std::string requested_device;
    std::string device;
    std::string extractor_device;
    std::string matcher_device;
    std::string python_version;
    std::string torch_version;
    std::string torchvision_version;
    std::string kornia_version;
    std::string image_decoder;
    std::string lightglue_commit;
    std::string lightglue_archive_sha256;
    std::string lightglue_tree_sha256;
    std::string superpoint_commit;
    std::string superpoint_model_sha256;
    std::string lightglue_model_sha256;
    std::uint64_t startup_ns = 0;
    std::uint64_t extraction_ns = 0;
    std::uint64_t matching_ns = 0;
    std::uint64_t worker_total_ns = 0;
    std::uint64_t peak_rss_bytes = 0;
    std::vector<std::vector<WorkerKeypoint>> keypoints;
    std::vector<WorkerPair> pairs;
};

bool WriteWorkerRequest(const std::filesystem::path &path,
                        const std::vector<std::string> &image_paths,
                        std::string *error);

bool ParseWorkerResult(const std::filesystem::path &path,
                       std::size_t expected_image_count,
                       std::size_t max_keypoints_per_image,
                       std::size_t max_pair_count,
                       WorkerResult *result,
                       std::string *error);

} // namespace SuperPointLightGlueDetail

class MethodSuperPointLightGlue : public PoSDK::Interface::MethodPresetProfiler
{
public:
    // <posdk:copyright>
    PoSDK::Interface::CopyrightData Copyright() const override
    {
        // <posdk:copyright-body>
        return {"PoMVG/method_superpoint_lightglue", "Copyright (c) 2026 VINF-SJTU\nAuthor: qi Cai\nLicense: MPL-2.0\nEmail: qicaiCN@gmail.com\n\n# Third-party notices\n\nThe wrapper integration is derived from the VINF-SJTU PoSDK ALIKEDLightGlue\nfrontend, maintained by qi Cai, under MPL-2.0. The wrapper contains no upstream\nmodel implementation or weights. The signed installer retrieves pinned upstream\ndependencies into a version-specific private runtime.\n\nSuperPoint (Daniel DeTone, Tomasz Malisiewicz, Andrew Rabinovich; Magic Leap)\nand its weights are subject to the upstream noncommercial research license:\nhttps://github.com/magicleap/SuperPointPretrainedNetwork/blob/master/LICENSE\nThe full license is included as licenses/SuperPoint-LICENSE.txt. Installing the\nplugin does not grant additional rights to SuperPoint. Users must have the\nrights required by that license before installing or using the dependency.\n\nLightGlue (Philipp Lindenberger, Paul-Edouard Sarlin, Marc Pollefeys; ETH Zurich)\nis Apache-2.0, except for separately licensed feature extractors.\nSource commit: eb42fee2d71449efb0aa5c10549752b5d75384d8.\nhttps://github.com/cvg/LightGlue\n\nPython 3.11.11, PyTorch 2.2.2, torchvision 0.17.2, NumPy 1.26.4,\nKornia 0.6.12 and Pillow 12.3.0 retain their upstream licenses.\nExact Conda and wheel versions/hashes are in deps/*.lock. Verbatim wheel license\nand notice snapshots are included under licenses/runtime/.\n"};
        // </posdk:copyright-body>
    }
    // </posdk:copyright>

    MethodSuperPointLightGlue();
    ~MethodSuperPointLightGlue() override = default;

    PoSDK::Interface::DataPtr Run() override;
    const std::string &GetType() const override;

private:
    struct RuntimeOptions
    {
        std::string device;
        std::string pair_mode;
        std::size_t pair_window = 5;
        std::size_t max_keypoints = 1280;
        std::size_t resize_long_edge = 1024;
        double detection_threshold = 0.01;
        double filter_threshold = 0.10;
        double depth_confidence = 0.95;
        double width_confidence = 0.99;
        std::size_t cpu_threads = 6;
        std::size_t timeout_seconds = 1800;
        bool enable_profiling = true;
    };

    bool ResolveImages(PoSDK::types::ImagePathsPtr *images,
                       std::string *error);
    bool LoadAndValidateOptions(RuntimeOptions *options,
                                std::string *error) const;
    std::filesystem::path ResolveWorkerScript() const;
    std::filesystem::path ResolvePythonExecutable() const;
    std::filesystem::path ResolveRuntimeRoot(
        const std::filesystem::path &python_executable) const;
    std::filesystem::path CreatePrivateTempDirectory() const;
    bool RunWorker(const std::filesystem::path &python_executable,
                   const std::filesystem::path &worker_script,
                   const std::filesystem::path &runtime_root,
                   const std::filesystem::path &request_path,
                   const std::filesystem::path &result_path,
                   const std::filesystem::path &worker_log_path,
                   const RuntimeOptions &options,
                   std::string *error) const;
    PoSDK::Interface::DataPtr BuildOutputPackage(
        const PoSDK::types::ImagePaths &images,
        const std::vector<PoSDK::types::IndexT> &active_view_ids,
        const SuperPointLightGlueDetail::WorkerResult &worker_result,
        std::string *error) const;
    void WriteLogSummary(
        const PoSDK::types::ImagePaths &images,
        const std::vector<PoSDK::types::IndexT> &active_view_ids,
        const SuperPointLightGlueDetail::WorkerResult &worker_result,
        double parent_wall_ms) const;
    std::filesystem::path ResolveSummaryLogPath() const;
};

} // namespace PluginMethods
