#pragma once

#include <po_core.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace PluginMethods
{
namespace ALIKEDLightGlueDetail
{

inline constexpr std::uint32_t kProtocolVersion = 2;
inline constexpr const char *kExtractorName = "aliked-n16";
inline constexpr const char *kMatcherName = "aliked_lightglue";
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
inline constexpr const char *kALIKEDCommit =
    "683d7c65197395c0b3f01ebe76e1084a27e73a65";
inline constexpr const char *kALIKEDModelSha256 =
    "5be8704840ed662d9d8c561bf7279c222092674e7eb05fd0feab94899e9d82f2";
inline constexpr const char *kLightGlueModelSha256 =
    "d975e965b105311a6143194852297dff4f02aea5cc2e10cecfed966ca0e22503";

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
    std::string aliked_commit;
    std::string aliked_model_sha256;
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

} // namespace ALIKEDLightGlueDetail

class MethodALIKEDLightGlue : public PoSDK::Interface::MethodPresetProfiler
{
public:
        // <posdk:copyright>
        PoSDK::Interface::CopyrightData Copyright() const override
        {
        // <posdk:copyright-body>
        return {"PoMVG/method_aliked_lightglue", "Copyright (c) 2025 VINF-SJTU\nAuthor: qi Cai\nLicense: MPL-2.0\nEmail: qicaiCN@gmail.com\n\nPoMVG integration layer only. ALIKED, LightGlue, their implementations and model weights retain the upstream authors, copyright holders and licenses listed below.\n\n**Plugin integration author:** qi Cai\n\n**Contact:** <qicaiCN@gmail.com>\n\n# Third-party notices\n\n`method_aliked_lightglue` is distributed as a closed PoMVG plugin with open\ndependencies. The signed `.pospkg` contains the installer recipes and license\nnotices; the isolated runtime dependencies are downloaded from their public\nupstreams during installation.\n\n## ALIKED\n\n- Original authors: Xiaoming Zhao, Xingming Wu, Weihai Chen, Peter C. Y. Chen, Qingsong Xu, and Zhengguo Li\n\n- Project: <https://github.com/Shiaoming/ALIKED>\n- Pinned commit: `683d7c65197395c0b3f01ebe76e1084a27e73a65`\n- Checkpoint URL: <https://raw.githubusercontent.com/Shiaoming/ALIKED/683d7c65197395c0b3f01ebe76e1084a27e73a65/models/aliked-n16.pth>\n- Checkpoint SHA-256: `5be8704840ed662d9d8c561bf7279c222092674e7eb05fd0feab94899e9d82f2`\n- Component: `aliked-n16.pth` checkpoint from the pinned ALIKED repository; the\n  ALIKED-derived implementation in LightGlue is separately fixed by the\n  LightGlue source-tree hash\n- License: BSD 3-Clause\n- Copyright: Copyright (c) 2022, Zhao Xiaoming\n- Basis for the checkpoint notice: the repository root BSD-3-Clause license;\n  the checkpoint has no separate model-card terms\n- Redistributed full text: `licenses/ALIKED-BSD-3-Clause.txt`\n\n## LightGlue\n\n- Original team: Computer Vision and Geometry Group (CVG), ETH Zurich\n- Original authors: Philipp Lindenberger, Paul-Edouard Sarlin, and Marc Pollefeys\n\n- Project: <https://github.com/cvg/LightGlue>\n- Pinned commit: `eb42fee2d71449efb0aa5c10549752b5d75384d8`\n- Source archive SHA-256: `e5c696a4c69be4e335b090ce3f70da1060f8aa52e61d463e96a930ebefd57a19`\n- Canonical extracted-tree SHA-256: `78cc712925a8de33ef9be7e807d0395f916dbd4eeb28f3644243ae2a00297d33`\n- Component: LightGlue source and the ALIKED-compatible `v0.1_arxiv` checkpoint\n- Checkpoint release tag commit:\n  `c91eb892b799fa607523879d18f9ec4fe76194a5`\n- Checkpoint URL:\n  <https://github.com/cvg/LightGlue/releases/download/v0.1_arxiv/aliked_lightglue.pth>\n- Checkpoint SHA-256:\n  `d975e965b105311a6143194852297dff4f02aea5cc2e10cecfed966ca0e22503`\n- License: Apache License 2.0\n- Copyright: Copyright 2023 ETH Zurich\n- Redistributed full text: `licenses/LightGlue-Apache-2.0.txt`\n\n## Isolated Python runtime\n\nThe installer creates a Python 3.11.11 environment from the exact package URLs\nand hashes in `deps/conda-osx-arm64.lock`, then installs the complete\nhash-locked wheel closure in `deps/requirements-osx-arm64.lock`. Direct runtime\ncomponents include:\n\n- CPython 3.11.11\n- PyTorch 2.2.2 and torchvision 0.17.2\n- NumPy 1.26.4\n- Pillow 12.3.0\n- Kornia 0.6.12\n\nSource references: [CPython](https://github.com/python/cpython/tree/v3.11.11),\n[PyTorch](https://github.com/pytorch/pytorch/tree/v2.2.2),\n[torchvision](https://github.com/pytorch/vision/tree/v0.17.2),\n[NumPy](https://github.com/numpy/numpy/tree/v1.26.4),\n[Pillow](https://github.com/python-pillow/Pillow), and\n[Kornia](https://github.com/kornia/kornia/tree/v0.6.12).\n\nThe exact open distributions retain their complete license files, notices,\nthird-party attributions, and any supplied SBOMs inside the installed private\nenvironment. Those embedded texts, rather than a simplified single-license\nlabel here, govern each distribution and its bundled libraries. The two lock\nfiles are the authoritative inventory for direct and transitive package\nversions and artifact hashes; no ambient Python package is imported.\n\nFor review without installing the environment, the signed plugin also ships\nverbatim license/notice snapshots from the exact hash-locked wheels under\n`licenses/runtime/`: PyTorch `LICENSE` and `NOTICE`, torchvision `LICENSE`,\nNumPy's aggregate and three component licenses, Kornia `LICENSE`, and Pillow's\naggregate bundled-library `LICENSE`. The installed wheel metadata remains the\nauthority if a formatting-only difference exists in a copied text.\n\nThe certified runtime intentionally excludes `opencv-python-headless`.\nDataset images are decoded as RGB with Pillow 12.3.0 (license expression\n`MIT-CMU`). A fail-closed compatibility stub only allows LightGlue to import\nunused optional OpenCV-dependent helpers; any attempted OpenCV API call raises.\nPillow's installed wheel retains its aggregate bundled-library license file and\nCycloneDX SBOM under `pillow-12.3.0.dist-info`.\n\nIf no trusted local Conda-compatible runner is available, the installer\ndownloads SHA-256-pinned micromamba 2.3.2 for macOS arm64. Micromamba is\ndistributed under the BSD 3-Clause License by the mamba-org contributors.\n\nThese notices are provided for attribution and license visibility and do not\nalter the licenses of the named projects.\n"};
        // </posdk:copyright-body>
    }
        // </posdk:copyright>

    MethodALIKEDLightGlue();
    ~MethodALIKEDLightGlue() override = default;

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
        const ALIKEDLightGlueDetail::WorkerResult &worker_result,
        std::string *error) const;
    void WriteLogSummary(
        const PoSDK::types::ImagePaths &images,
        const std::vector<PoSDK::types::IndexT> &active_view_ids,
        const ALIKEDLightGlueDetail::WorkerResult &worker_result,
        double parent_wall_ms) const;
    std::filesystem::path ResolveSummaryLogPath() const;
};

} // namespace PluginMethods
