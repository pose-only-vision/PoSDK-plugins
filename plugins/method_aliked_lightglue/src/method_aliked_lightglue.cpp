#include "method_aliked_lightglue.hpp"

#include "../plugin_runtime_path.hpp"

#include <po_core/ProfilerManager.hpp>
#include <po_core/po_logger.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <regex>
#include <set>
#include <signal.h>
#include <spawn.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char **environ;

namespace PluginMethods
{
namespace
{

using namespace PoSDK;
using namespace PoSDK::Interface;
using namespace PoSDK::types;
using namespace ALIKEDLightGlueDetail;

constexpr std::array<char, 8> kRequestMagic = {'P', 'A', 'L', 'G', 'R', 'E', 'Q', '1'};
constexpr std::array<char, 8> kResultMagic = {'P', 'A', 'L', 'G', 'R', 'E', 'S', '1'};
constexpr std::uint64_t kMaximumResultBytes = 8ULL * 1024ULL * 1024ULL * 1024ULL;
constexpr std::size_t kMaximumIdentityLength = 256;
constexpr std::size_t kMaximumImagePathLength = 1024 * 1024;
constexpr std::size_t kMaximumImages = 10000;

void ALIKEDLightGlueRuntimeFileAnchor() {}

void SetError(std::string *error, const std::string &message)
{
    if (error)
    {
        *error = message;
    }
}

void WriteU32(std::ostream &stream, std::uint32_t value)
{
    const std::array<unsigned char, 4> bytes = {
        static_cast<unsigned char>(value & 0xffU),
        static_cast<unsigned char>((value >> 8U) & 0xffU),
        static_cast<unsigned char>((value >> 16U) & 0xffU),
        static_cast<unsigned char>((value >> 24U) & 0xffU)};
    stream.write(reinterpret_cast<const char *>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

class BoundedBinaryReader
{
public:
    explicit BoundedBinaryReader(const std::filesystem::path &path)
        : stream_(path, std::ios::binary)
    {
    }

    bool IsOpen() const { return stream_.is_open(); }

    bool ReadBytes(void *destination, std::size_t count)
    {
        if (count > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
        {
            return false;
        }
        stream_.read(static_cast<char *>(destination),
                     static_cast<std::streamsize>(count));
        return stream_.good() ||
               (stream_.eof() && stream_.gcount() == static_cast<std::streamsize>(count));
    }

    bool ReadU32(std::uint32_t *value)
    {
        std::array<unsigned char, 4> bytes{};
        if (!value || !ReadBytes(bytes.data(), bytes.size()))
        {
            return false;
        }
        *value = static_cast<std::uint32_t>(bytes[0]) |
                 (static_cast<std::uint32_t>(bytes[1]) << 8U) |
                 (static_cast<std::uint32_t>(bytes[2]) << 16U) |
                 (static_cast<std::uint32_t>(bytes[3]) << 24U);
        return true;
    }

    bool ReadU64(std::uint64_t *value)
    {
        std::array<unsigned char, 8> bytes{};
        if (!value || !ReadBytes(bytes.data(), bytes.size()))
        {
            return false;
        }
        std::uint64_t decoded = 0;
        for (std::size_t index = 0; index < bytes.size(); ++index)
        {
            decoded |= static_cast<std::uint64_t>(bytes[index]) << (index * 8U);
        }
        *value = decoded;
        return true;
    }

    bool ReadFloat(float *value)
    {
        std::uint32_t bits = 0;
        if (!value || !ReadU32(&bits))
        {
            return false;
        }
        static_assert(sizeof(float) == sizeof(std::uint32_t),
                      "PoSDK worker protocol requires IEEE-754 float32");
        std::memcpy(value, &bits, sizeof(bits));
        return true;
    }

    bool ReadString(std::string *value, std::size_t maximum_length)
    {
        std::uint32_t length = 0;
        if (!value || !ReadU32(&length) || length > maximum_length)
        {
            return false;
        }
        value->assign(length, '\0');
        return length == 0 || ReadBytes(value->data(), length);
    }

    bool AtEnd()
    {
        char extra = 0;
        stream_.read(&extra, 1);
        return stream_.eof() && stream_.gcount() == 0;
    }

private:
    std::ifstream stream_;
};

std::string ReadLogTail(const std::filesystem::path &path,
                        std::size_t maximum_bytes = 32768)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
    {
        return {};
    }
    stream.seekg(0, std::ios::end);
    const auto end = stream.tellg();
    if (end <= 0)
    {
        return {};
    }
    const auto length = static_cast<std::uint64_t>(end);
    const auto start = length > maximum_bytes ? length - maximum_bytes : 0;
    stream.seekg(static_cast<std::streamoff>(start), std::ios::beg);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

bool IsExecutableRegularFile(const std::filesystem::path &path)
{
    std::error_code error;
    if (path.empty() || !path.is_absolute() || std::filesystem::is_symlink(path, error) ||
        error || !std::filesystem::is_regular_file(path, error) || error)
    {
        return false;
    }
    return ::access(path.c_str(), X_OK) == 0;
}

struct TempDirectoryGuard
{
    explicit TempDirectoryGuard(std::filesystem::path value)
        : path(std::move(value))
    {
    }

    ~TempDirectoryGuard()
    {
        if (path.empty())
        {
            return;
        }
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    std::filesystem::path path;
};

} // namespace

namespace ALIKEDLightGlueDetail
{

bool WriteWorkerRequest(const std::filesystem::path &path,
                        const std::vector<std::string> &image_paths,
                        std::string *error)
{
    if (image_paths.size() < 2 || image_paths.size() > kMaximumImages)
    {
        SetError(error, "request image count is outside the supported range");
        return false;
    }

    std::error_code file_error;
    if (std::filesystem::exists(path, file_error) || file_error)
    {
        SetError(error, "request path already exists or cannot be inspected");
        return false;
    }

    std::ofstream stream(path, std::ios::binary | std::ios::out);
    if (!stream)
    {
        SetError(error, "failed to create worker request");
        return false;
    }
    stream.write(kRequestMagic.data(),
                 static_cast<std::streamsize>(kRequestMagic.size()));
    WriteU32(stream, kProtocolVersion);
    WriteU32(stream, static_cast<std::uint32_t>(image_paths.size()));
    for (const std::string &image_path : image_paths)
    {
        if (image_path.empty() || image_path.size() > kMaximumImagePathLength ||
            image_path.find('\0') != std::string::npos)
        {
            SetError(error, "request contains an invalid image path");
            return false;
        }
        WriteU32(stream, static_cast<std::uint32_t>(image_path.size()));
        stream.write(image_path.data(),
                     static_cast<std::streamsize>(image_path.size()));
    }
    stream.flush();
    if (!stream)
    {
        SetError(error, "failed while writing worker request");
        return false;
    }
    ::chmod(path.c_str(), S_IRUSR | S_IWUSR);
    return true;
}

bool ParseWorkerResult(const std::filesystem::path &path,
                       std::size_t expected_image_count,
                       std::size_t max_keypoints_per_image,
                       std::size_t max_pair_count,
                       WorkerResult *result,
                       std::string *error)
{
    if (!result || expected_image_count < 2 || expected_image_count > kMaximumImages ||
        max_keypoints_per_image == 0 || max_keypoints_per_image > 20000)
    {
        SetError(error, "invalid worker-result parser limits");
        return false;
    }

    std::error_code file_error;
    if (std::filesystem::is_symlink(path, file_error) || file_error ||
        !std::filesystem::is_regular_file(path, file_error) || file_error)
    {
        SetError(error, "worker result is not a regular file");
        return false;
    }
    const std::uintmax_t file_size = std::filesystem::file_size(path, file_error);
    if (file_error || file_size == 0 || file_size > kMaximumResultBytes)
    {
        SetError(error, "worker result size is invalid");
        return false;
    }

    BoundedBinaryReader reader(path);
    if (!reader.IsOpen())
    {
        SetError(error, "failed to open worker result");
        return false;
    }

    std::array<char, 8> magic{};
    std::uint32_t version = 0;
    std::uint32_t image_count = 0;
    std::uint32_t pair_count = 0;
    if (!reader.ReadBytes(magic.data(), magic.size()) || magic != kResultMagic ||
        !reader.ReadU32(&version) || version != kProtocolVersion ||
        !reader.ReadU32(&image_count) || image_count != expected_image_count ||
        !reader.ReadU32(&pair_count) || pair_count != max_pair_count)
    {
        SetError(error, "worker result header or declared dimensions are invalid");
        return false;
    }

    WorkerResult parsed;
    if (!reader.ReadU64(&parsed.startup_ns) ||
        !reader.ReadU64(&parsed.extraction_ns) ||
        !reader.ReadU64(&parsed.matching_ns) ||
        !reader.ReadU64(&parsed.worker_total_ns) ||
        !reader.ReadU64(&parsed.peak_rss_bytes) ||
        !reader.ReadString(&parsed.extractor, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.matcher, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.requested_device, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.device, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.extractor_device, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.matcher_device, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.python_version, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.torch_version, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.torchvision_version, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.kornia_version, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.image_decoder, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.lightglue_commit, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.lightglue_archive_sha256, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.lightglue_tree_sha256, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.aliked_commit, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.aliked_model_sha256, kMaximumIdentityLength) ||
        !reader.ReadString(&parsed.lightglue_model_sha256, kMaximumIdentityLength))
    {
        SetError(error, "worker result identity block is truncated or invalid");
        return false;
    }

    const bool device_policy_matches =
        ((parsed.requested_device == "auto" || parsed.requested_device == "cpu") &&
         parsed.device == "cpu" && parsed.extractor_device == "cpu" &&
         parsed.matcher_device == "cpu") ||
        (parsed.requested_device == "mps" && parsed.device == "cpu+mps" &&
         parsed.extractor_device == "cpu" && parsed.matcher_device == "mps");
    const bool identity_matches =
        parsed.extractor == kExtractorName &&
        parsed.matcher == kMatcherName &&
        device_policy_matches &&
        parsed.python_version == kPythonVersion &&
        parsed.torch_version == kTorchVersion &&
        parsed.torchvision_version == kTorchvisionVersion &&
        parsed.kornia_version == kKorniaVersion &&
        parsed.image_decoder == kImageDecoderIdentity &&
        parsed.lightglue_commit == kLightGlueCommit &&
        parsed.lightglue_archive_sha256 == kLightGlueArchiveSha256 &&
        parsed.lightglue_tree_sha256 == kLightGlueTreeSha256 &&
        parsed.aliked_commit == kALIKEDCommit &&
        parsed.aliked_model_sha256 == kALIKEDModelSha256 &&
        parsed.lightglue_model_sha256 == kLightGlueModelSha256;
    if (!identity_matches)
    {
        SetError(error, "worker runtime, source, or model identity does not match the signed plugin");
        return false;
    }

    parsed.keypoints.resize(image_count);
    std::size_t total_keypoints = 0;
    for (std::uint32_t image = 0; image < image_count; ++image)
    {
        std::uint32_t keypoint_count = 0;
        if (!reader.ReadU32(&keypoint_count) || keypoint_count > max_keypoints_per_image)
        {
            SetError(error, "worker result keypoint count exceeds the configured bound");
            return false;
        }
        total_keypoints += keypoint_count;
        if (total_keypoints > expected_image_count * max_keypoints_per_image)
        {
            SetError(error, "worker result total keypoint count overflowed its bound");
            return false;
        }
        auto &image_keypoints = parsed.keypoints[image];
        image_keypoints.resize(keypoint_count);
        for (WorkerKeypoint &keypoint : image_keypoints)
        {
            if (!reader.ReadFloat(&keypoint.x) || !reader.ReadFloat(&keypoint.y) ||
                !reader.ReadFloat(&keypoint.score) || !std::isfinite(keypoint.x) ||
                !std::isfinite(keypoint.y) || !std::isfinite(keypoint.score) ||
                keypoint.score < 0.0F || keypoint.score > 1.0001F)
            {
                SetError(error, "worker result contains an invalid keypoint");
                return false;
            }
        }
    }

    parsed.pairs.resize(pair_count);
    std::set<std::uint64_t> seen_pairs;
    std::size_t total_matches = 0;
    for (WorkerPair &pair : parsed.pairs)
    {
        std::uint32_t match_count = 0;
        if (!reader.ReadU32(&pair.image_i) || !reader.ReadU32(&pair.image_j) ||
            !reader.ReadU32(&match_count) || pair.image_i >= pair.image_j ||
            pair.image_j >= image_count || match_count > max_keypoints_per_image)
        {
            SetError(error, "worker result contains an invalid image pair");
            return false;
        }
        const std::uint64_t pair_key =
            (static_cast<std::uint64_t>(pair.image_i) << 32U) | pair.image_j;
        if (!seen_pairs.insert(pair_key).second)
        {
            SetError(error, "worker result contains a duplicate image pair");
            return false;
        }
        total_matches += match_count;
        if (total_matches > max_pair_count * max_keypoints_per_image)
        {
            SetError(error, "worker result total match count overflowed its bound");
            return false;
        }
        pair.matches.resize(match_count);
        std::set<std::uint64_t> seen_matches;
        for (WorkerMatch &match : pair.matches)
        {
            if (!reader.ReadU32(&match.feature_i) ||
                !reader.ReadU32(&match.feature_j) ||
                !reader.ReadFloat(&match.score) ||
                match.feature_i >= parsed.keypoints[pair.image_i].size() ||
                match.feature_j >= parsed.keypoints[pair.image_j].size() ||
                !std::isfinite(match.score) || match.score < 0.0F ||
                match.score > 1.0001F)
            {
                SetError(error, "worker result contains an invalid feature match");
                return false;
            }
            const std::uint64_t match_key =
                (static_cast<std::uint64_t>(match.feature_i) << 32U) |
                match.feature_j;
            if (!seen_matches.insert(match_key).second)
            {
                SetError(error, "worker result contains a duplicate feature match");
                return false;
            }
        }
    }

    if (!reader.AtEnd())
    {
        SetError(error, "worker result contains trailing data");
        return false;
    }

    *result = std::move(parsed);
    return true;
}

} // namespace ALIKEDLightGlueDetail

MethodALIKEDLightGlue::MethodALIKEDLightGlue()
{
    required_package_["data_images"] = nullptr;
    output_types_ = {"data_features", "data_matches"};
    InitializeDefaultConfigPath();
}

DataPtr MethodALIKEDLightGlue::Run()
{
    DisplayConfigInfo();

    ImagePathsPtr images;
    std::string error;
    if (!ResolveImages(&images, &error) || !images)
    {
        LOG_ERROR_ZH << "[ALIKED+LightGlue] 无法解析输入图像: " << error;
        LOG_ERROR_EN << "[ALIKED+LightGlue] Failed to resolve input images: " << error;
        return nullptr;
    }

    RuntimeOptions options;
    if (!LoadAndValidateOptions(&options, &error))
    {
        LOG_ERROR_ZH << "[ALIKED+LightGlue] 配置无效: " << error;
        LOG_ERROR_EN << "[ALIKED+LightGlue] Invalid configuration: " << error;
        return nullptr;
    }

    SetProfilerLabels({
        {"frontend", "ALIKED-n16+LightGlue-v0.1_arxiv"},
        {"image_decoder", kImageDecoderIdentity},
        {"device_requested", options.device},
        {"device_effective", options.device == "mps" ? "cpu+mps" : "cpu"},
        {"pair_mode", options.pair_mode},
        {"pair_window", std::to_string(options.pair_window)},
        {"max_keypoints", std::to_string(options.max_keypoints)},
        {"resize_long_edge", std::to_string(options.resize_long_edge)},
        {"detection_threshold", std::to_string(options.detection_threshold)},
        {"filter_threshold", std::to_string(options.filter_threshold)},
        {"depth_confidence", std::to_string(options.depth_confidence)},
        {"width_confidence", std::to_string(options.width_confidence)}});

    std::vector<IndexT> active_view_ids;
    std::vector<std::string> active_paths;
    active_view_ids.reserve(images->size());
    active_paths.reserve(images->size());
    for (std::size_t view = 0; view < images->size(); ++view)
    {
        const auto &[path_text, enabled] = images->at(view);
        if (!enabled)
        {
            continue;
        }
        std::error_code path_error;
        const std::filesystem::path path(path_text);
        if (path_text.empty() || !std::filesystem::is_regular_file(path, path_error) ||
            path_error)
        {
            LOG_ERROR_ZH << "[ALIKED+LightGlue] 图像路径无效，view=" << view;
            LOG_ERROR_EN << "[ALIKED+LightGlue] Invalid image path for view " << view;
            return nullptr;
        }
        const std::filesystem::path canonical =
            std::filesystem::weakly_canonical(path, path_error);
        if (path_error || canonical.empty())
        {
            LOG_ERROR_ZH << "[ALIKED+LightGlue] 无法规范化图像路径，view=" << view;
            LOG_ERROR_EN << "[ALIKED+LightGlue] Failed to canonicalize image path for view " << view;
            return nullptr;
        }
        active_view_ids.push_back(static_cast<IndexT>(view));
        active_paths.push_back(canonical.string());
    }
    if (active_paths.size() < 2 || active_paths.size() > kMaximumImages)
    {
        LOG_ERROR_ZH << "[ALIKED+LightGlue] 至少需要两张启用图像，且图像数量不能超过 "
                     << kMaximumImages;
        LOG_ERROR_EN << "[ALIKED+LightGlue] At least two enabled images are required and the count must not exceed "
                     << kMaximumImages;
        return nullptr;
    }

    if (options.pair_mode == "exhaustive" && active_paths.size() > 2000)
    {
        LOG_ERROR_ZH << "[ALIKED+LightGlue] 穷举匹配最多支持 2000 张图像；请改用 contiguous。";
        LOG_ERROR_EN << "[ALIKED+LightGlue] Exhaustive matching is capped at 2000 images; use contiguous mode.";
        return nullptr;
    }

    const std::filesystem::path worker_script = ResolveWorkerScript();
    const std::filesystem::path python_executable = ResolvePythonExecutable();
    const std::filesystem::path runtime_root = ResolveRuntimeRoot(python_executable);
    if (worker_script.empty() || !std::filesystem::is_regular_file(worker_script) ||
        !IsExecutableRegularFile(python_executable) || runtime_root.empty())
    {
        LOG_ERROR_ZH << "[ALIKED+LightGlue] 私有运行环境不完整，请从插件市场重新安装。";
        LOG_ERROR_EN << "[ALIKED+LightGlue] The private runtime is incomplete; reinstall the plugin from Marketplace.";
        return nullptr;
    }

    const std::filesystem::path temp_directory = CreatePrivateTempDirectory();
    if (temp_directory.empty())
    {
        LOG_ERROR_ZH << "[ALIKED+LightGlue] 无法创建私有临时目录。";
        LOG_ERROR_EN << "[ALIKED+LightGlue] Failed to create a private temporary directory.";
        return nullptr;
    }
    TempDirectoryGuard cleanup(temp_directory);
    const std::filesystem::path request_path = temp_directory / "request.bin";
    const std::filesystem::path result_path = temp_directory / "result.bin";
    const std::filesystem::path worker_log_path = temp_directory / "worker.log";

    if (!WriteWorkerRequest(request_path, active_paths, &error))
    {
        LOG_ERROR_ZH << "[ALIKED+LightGlue] 写入 worker 请求失败: " << error;
        LOG_ERROR_EN << "[ALIKED+LightGlue] Failed to write worker request: " << error;
        return nullptr;
    }

    const auto parent_start = std::chrono::steady_clock::now();
    PROFILER_START_AUTO(options.enable_profiling);
    const bool worker_ok = RunWorker(
        python_executable, worker_script, runtime_root, request_path,
        result_path, worker_log_path, options, &error);
    PROFILER_END();
    const auto parent_end = std::chrono::steady_clock::now();
    const double parent_wall_ms =
        std::chrono::duration<double, std::milli>(parent_end - parent_start).count();

    if (!worker_ok)
    {
        LOG_ERROR_ZH << "[ALIKED+LightGlue] worker 执行失败: " << error;
        LOG_ERROR_EN << "[ALIKED+LightGlue] Worker execution failed: " << error;
        const std::string tail = ReadLogTail(worker_log_path);
        if (!tail.empty())
        {
            LOG_ERROR_ALL << "[ALIKED+LightGlue worker log]\n" << tail;
        }
        return nullptr;
    }

    WorkerResult worker_result;
    std::size_t expected_pairs = 0;
    if (options.pair_mode == "exhaustive")
    {
        expected_pairs = active_paths.size() * (active_paths.size() - 1U) / 2U;
    }
    else
    {
        for (std::size_t image = 0; image < active_paths.size(); ++image)
        {
            expected_pairs += std::min(
                options.pair_window, active_paths.size() - image - 1U);
        }
    }
    if (!ParseWorkerResult(result_path, active_paths.size(), options.max_keypoints,
                           expected_pairs, &worker_result, &error))
    {
        LOG_ERROR_ZH << "[ALIKED+LightGlue] worker 结果验证失败: " << error;
        LOG_ERROR_EN << "[ALIKED+LightGlue] Worker result validation failed: " << error;
        return nullptr;
    }
    if (worker_result.requested_device != options.device ||
        (options.device == "auto" && worker_result.device != "cpu") ||
        (options.device == "cpu" && worker_result.device != "cpu") ||
        (options.device == "mps" && worker_result.device != "cpu+mps"))
    {
        LOG_ERROR_ZH << "[ALIKED+LightGlue] worker 的请求设备或实际设备不符合认证策略。";
        LOG_ERROR_EN << "[ALIKED+LightGlue] Worker request or actual device violates the certified policy.";
        return nullptr;
    }

    DataPtr output = BuildOutputPackage(
        *images, active_view_ids, worker_result, &error);
    if (!output)
    {
        LOG_ERROR_ZH << "[ALIKED+LightGlue] 构建 PoSDK 输出失败: " << error;
        LOG_ERROR_EN << "[ALIKED+LightGlue] Failed to build PoSDK output: " << error;
        return nullptr;
    }

    WriteLogSummary(*images, active_view_ids, worker_result, parent_wall_ms);
    LOG_INFO_ZH << "[ALIKED+LightGlue] 完成: " << active_paths.size()
                << " 张图像, " << worker_result.pairs.size() << " 个图像对, device="
                << worker_result.device << ", 前端总耗时=" << std::fixed
                << std::setprecision(2) << parent_wall_ms << " ms";
    LOG_INFO_EN << "[ALIKED+LightGlue] Completed: " << active_paths.size()
                << " images, " << worker_result.pairs.size() << " pairs, device="
                << worker_result.device << ", frontend wall time=" << std::fixed
                << std::setprecision(2) << parent_wall_ms << " ms";
    return output;
}

bool MethodALIKEDLightGlue::ResolveImages(ImagePathsPtr *images,
                                          std::string *error)
{
    if (!images)
    {
        SetError(error, "null output pointer");
        return false;
    }

    auto input_it = required_package_.find("data_images");
    if (input_it != required_package_.end() && input_it->second)
    {
        ImagePathsPtr value = GetDataPtr<ImagePaths>(input_it->second);
        if (value && !value->empty())
        {
            *images = std::move(value);
            return true;
        }
    }

    auto prior_it = prior_info_.find("data_images");
    if (prior_it != prior_info_.end() && prior_it->second)
    {
        ImagePathsPtr value = GetDataPtr<ImagePaths>(prior_it->second);
        if (value && !value->empty())
        {
            required_package_["data_images"] = prior_it->second;
            *images = std::move(value);
            return true;
        }
    }

    const std::string image_folder = GetOptionAsPath("image_folder", "", "");
    if (image_folder.empty())
    {
        SetError(error, "data_images is empty and image_folder is not configured");
        return false;
    }
    DataPtr loaded = FactoryData::Create("data_images");
    if (!loaded || !loaded->Load(image_folder, "folder"))
    {
        SetError(error, "failed to load image_folder");
        return false;
    }
    ImagePathsPtr value = GetDataPtr<ImagePaths>(loaded);
    if (!value || value->empty())
    {
        SetError(error, "image_folder produced no images");
        return false;
    }
    required_package_["data_images"] = loaded;
    *images = std::move(value);
    return true;
}

bool MethodALIKEDLightGlue::LoadAndValidateOptions(RuntimeOptions *options,
                                                   std::string *error) const
{
    if (!options)
    {
        SetError(error, "null options pointer");
        return false;
    }
    options->device = GetOptionAsString("device", "auto");
    options->pair_mode = GetOptionAsString("pair_mode", "exhaustive");
    options->pair_window = GetOptionAsIndexT("pair_window", 5);
    options->max_keypoints = GetOptionAsIndexT("max_keypoints", 1280);
    options->resize_long_edge = GetOptionAsIndexT("resize_long_edge", 1024);
    options->detection_threshold = GetOptionAsDouble("detection_threshold", 0.01);
    options->filter_threshold = GetOptionAsDouble("filter_threshold", 0.10);
    options->depth_confidence = GetOptionAsDouble("depth_confidence", 0.95);
    options->width_confidence = GetOptionAsDouble("width_confidence", 0.99);
    options->cpu_threads = GetOptionAsIndexT("cpu_threads", 6);
    options->timeout_seconds = GetOptionAsIndexT("worker_timeout_seconds", 1800);
    options->enable_profiling = GetOptionAsBool("enable_profiling", true);

    if ((options->device != "auto" && options->device != "mps" &&
         options->device != "cpu") ||
        (options->pair_mode != "exhaustive" && options->pair_mode != "contiguous") ||
        options->pair_window < 1 || options->pair_window > 1000 ||
        options->max_keypoints < 128 || options->max_keypoints > 20000 ||
        options->resize_long_edge < 256 || options->resize_long_edge > 4096 ||
        options->detection_threshold < -1.0 || options->detection_threshold > 1.0 ||
        options->filter_threshold < 0.0 || options->filter_threshold > 1.0 ||
        options->depth_confidence < -1.0 || options->depth_confidence > 1.0 ||
        options->width_confidence < -1.0 || options->width_confidence > 1.0 ||
        options->cpu_threads < 1 || options->cpu_threads > 64 ||
        options->timeout_seconds < 30 || options->timeout_seconds > 7200)
    {
        SetError(error, "one or more options are outside their declared ranges");
        return false;
    }
    return true;
}

std::filesystem::path MethodALIKEDLightGlue::ResolveWorkerScript() const
{
    return PluginRuntime::FindRuntimeFile(
        &ALIKEDLightGlueRuntimeFileAnchor,
        "aliked_lightglue_worker.py",
        POSDK_PLUGIN_DEVELOPER_FALLBACKS(
            "../python/method_aliked_lightglue/aliked_lightglue_worker.py",
            "output/python/method_aliked_lightglue/aliked_lightglue_worker.py",
            "../../plugins/methods/ALIKEDLightGlue/aliked_lightglue_worker.py",
            "../../../plugins/methods/ALIKEDLightGlue/aliked_lightglue_worker.py"));
}

std::filesystem::path MethodALIKEDLightGlue::ResolvePythonExecutable() const
{
    std::vector<std::filesystem::path> candidates;
    const std::string explicit_python = GetOptionAsString("python_executable", "");
    if (!explicit_python.empty())
    {
        candidates.emplace_back(explicit_python);
    }
    const std::string managed_bin = GetOptionAsString("python_bin_folder", "");
    if (!managed_bin.empty())
    {
        candidates.emplace_back(std::filesystem::path(managed_bin) / "python3.11");
    }

    for (const std::filesystem::path &candidate : candidates)
    {
        std::error_code error;
        const std::filesystem::path absolute =
            std::filesystem::weakly_canonical(candidate, error);
        if (!error && IsExecutableRegularFile(absolute))
        {
            return absolute;
        }
    }
    return {};
}

std::filesystem::path MethodALIKEDLightGlue::ResolveRuntimeRoot(
    const std::filesystem::path &python_executable) const
{
    std::filesystem::path candidate;
    const std::string explicit_runtime = GetOptionAsString("runtime_folder", "");
    if (!explicit_runtime.empty())
    {
        candidate = explicit_runtime;
    }
    else if (!python_executable.empty())
    {
        candidate = python_executable.parent_path().parent_path().parent_path() / "runtime";
    }
    if (candidate.empty())
    {
        return {};
    }
    std::error_code error;
    candidate = std::filesystem::weakly_canonical(candidate, error);
    if (error || !candidate.is_absolute() ||
        !std::filesystem::is_directory(candidate / "LightGlue", error) || error ||
        !std::filesystem::is_regular_file(candidate / "identity.json", error) || error ||
        !std::filesystem::is_regular_file(
            candidate / "torch/hub/checkpoints/aliked-n16.pth", error) || error ||
        !std::filesystem::is_regular_file(
            candidate / "torch/hub/checkpoints/aliked_lightglue_v0-1_arxiv.pth",
            error) || error)
    {
        return {};
    }
    return candidate;
}

std::filesystem::path MethodALIKEDLightGlue::CreatePrivateTempDirectory() const
{
    std::random_device random_device;
    std::uniform_int_distribution<unsigned long long> distribution;
    const std::filesystem::path base = std::filesystem::temp_directory_path();
    for (int attempt = 0; attempt < 64; ++attempt)
    {
        std::ostringstream name;
        name << "posdk-aliked-lightglue-" << std::hex
             << distribution(random_device) << distribution(random_device);
        const std::filesystem::path candidate = base / name.str();
        std::error_code error;
        if (std::filesystem::create_directory(candidate, error) && !error)
        {
            ::chmod(candidate.c_str(), S_IRWXU);
            return candidate;
        }
    }
    return {};
}

bool MethodALIKEDLightGlue::RunWorker(
    const std::filesystem::path &python_executable,
    const std::filesystem::path &worker_script,
    const std::filesystem::path &runtime_root,
    const std::filesystem::path &request_path,
    const std::filesystem::path &result_path,
    const std::filesystem::path &worker_log_path,
    const RuntimeOptions &options,
    std::string *error) const
{
    std::vector<std::string> arguments = {
        python_executable.string(),
        worker_script.string(),
        "--request", request_path.string(),
        "--output", result_path.string(),
        "--runtime-root", runtime_root.string(),
        "--device", options.device,
        "--pair-mode", options.pair_mode,
        "--pair-window", std::to_string(options.pair_window),
        "--max-keypoints", std::to_string(options.max_keypoints),
        "--resize-long-edge", std::to_string(options.resize_long_edge),
        "--detection-threshold", std::to_string(options.detection_threshold),
        "--filter-threshold", std::to_string(options.filter_threshold),
        "--depth-confidence", std::to_string(options.depth_confidence),
        "--width-confidence", std::to_string(options.width_confidence),
        "--cpu-threads", std::to_string(options.cpu_threads),
        "--seed", "31",
        "--expected-lightglue-commit", kLightGlueCommit,
        "--expected-lightglue-archive-sha256", kLightGlueArchiveSha256,
        "--expected-lightglue-tree-sha256", kLightGlueTreeSha256,
        "--expected-aliked-commit", kALIKEDCommit,
        "--expected-aliked-model-sha256", kALIKEDModelSha256,
        "--expected-lightglue-model-sha256", kLightGlueModelSha256};

    std::vector<char *> argv;
    argv.reserve(arguments.size() + 1U);
    for (std::string &argument : arguments)
    {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t file_actions;
    if (posix_spawn_file_actions_init(&file_actions) != 0)
    {
        SetError(error, "failed to initialize child-process file actions");
        return false;
    }
    const int open_result = posix_spawn_file_actions_addopen(
        &file_actions, STDOUT_FILENO, worker_log_path.c_str(),
        O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    const int duplicate_result = open_result == 0
        ? posix_spawn_file_actions_adddup2(&file_actions, STDOUT_FILENO, STDERR_FILENO)
        : open_result;
    if (open_result != 0 || duplicate_result != 0)
    {
        posix_spawn_file_actions_destroy(&file_actions);
        SetError(error, "failed to configure child-process logging");
        return false;
    }

    pid_t child = -1;
    const int spawn_result = posix_spawn(
        &child, python_executable.c_str(), &file_actions, nullptr,
        argv.data(), environ);
    posix_spawn_file_actions_destroy(&file_actions);
    if (spawn_result != 0 || child <= 0)
    {
        SetError(error, "failed to start private Python worker: " +
                            std::string(std::strerror(spawn_result)));
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(options.timeout_seconds);
    int status = 0;
    while (true)
    {
        const pid_t wait_result = ::waitpid(child, &status, WNOHANG);
        if (wait_result == child)
        {
            break;
        }
        if (wait_result < 0 && errno != EINTR)
        {
            ::kill(child, SIGKILL);
            ::waitpid(child, &status, 0);
            SetError(error, "failed while waiting for private Python worker");
            return false;
        }
        if (std::chrono::steady_clock::now() >= deadline)
        {
            ::kill(child, SIGTERM);
            for (int attempt = 0; attempt < 20; ++attempt)
            {
                if (::waitpid(child, &status, WNOHANG) == child)
                {
                    SetError(error, "private Python worker exceeded its timeout");
                    return false;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            ::kill(child, SIGKILL);
            ::waitpid(child, &status, 0);
            SetError(error, "private Python worker exceeded its timeout and was terminated");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        std::ostringstream message;
        if (WIFEXITED(status))
        {
            message << "private Python worker exited with code " << WEXITSTATUS(status);
        }
        else if (WIFSIGNALED(status))
        {
            message << "private Python worker was terminated by signal " << WTERMSIG(status);
        }
        else
        {
            message << "private Python worker ended abnormally";
        }
        SetError(error, message.str());
        return false;
    }
    return true;
}

DataPtr MethodALIKEDLightGlue::BuildOutputPackage(
    const ImagePaths &images,
    const std::vector<IndexT> &active_view_ids,
    const WorkerResult &worker_result,
    std::string *error) const
{
    if (active_view_ids.size() != worker_result.keypoints.size())
    {
        SetError(error, "active-view mapping does not match worker image count");
        return nullptr;
    }

    auto features = std::make_shared<FeaturesInfo>();
    for (std::size_t view = 0; view < images.size(); ++view)
    {
        // Use the host's public construction API for ImageFeatureInfo.
        // Inlining vector<ImageFeatureInfo>::resize across SDK generations
        // couples allocation to the plugin's compiled container layout.
        features->AddImageFeatures(images[view].first, images[view].second);
    }

    for (std::size_t local_image = 0; local_image < active_view_ids.size(); ++local_image)
    {
        const IndexT view_id = active_view_ids[local_image];
        if (view_id >= features->size())
        {
            SetError(error, "active view ID is outside data_features");
            return nullptr;
        }
        ImageFeatureInfo *image_features = (*features)[view_id];
        const auto &keypoints = worker_result.keypoints[local_image];
        image_features->ReserveFeatures(keypoints.size());
        for (const WorkerKeypoint &keypoint : keypoints)
        {
            image_features->AddFeature(
                static_cast<double>(keypoint.x),
                static_cast<double>(keypoint.y), 1.0F, -1.0F);
        }
    }

    auto matches = std::make_shared<Matches>();
    for (const WorkerPair &worker_pair : worker_result.pairs)
    {
        if (worker_pair.image_i >= active_view_ids.size() ||
            worker_pair.image_j >= active_view_ids.size())
        {
            SetError(error, "worker pair cannot be mapped to original view IDs");
            return nullptr;
        }
        const ViewPair view_pair(
            active_view_ids[worker_pair.image_i],
            active_view_ids[worker_pair.image_j]);
        IdMatches id_matches;
        id_matches.reserve(worker_pair.matches.size());
        for (const WorkerMatch &worker_match : worker_pair.matches)
        {
            IdMatch match;
            match.i = static_cast<IndexT>(worker_match.feature_i);
            match.j = static_cast<IndexT>(worker_match.feature_j);
            match.is_inlier = false;
            id_matches.push_back(match);
        }
        matches->emplace(view_pair, std::move(id_matches));
    }

    DataPtr features_data = FactoryData::Create("data_features");
    DataPtr matches_data = FactoryData::Create("data_matches");
    if (!features_data || !matches_data || !features_data->GetData() ||
        !matches_data->GetData())
    {
        SetError(error, "PoSDK data factory failed to create frontend outputs");
        return nullptr;
    }
    *static_cast<FeaturesInfo *>(features_data->GetData()) = *features;
    *static_cast<Matches *>(matches_data->GetData()) = *matches;

    auto package = std::make_shared<DataPackage>();
    package->AddData("data_features", features_data);
    package->AddData("data_matches", matches_data);
    return package;
}

void MethodALIKEDLightGlue::WriteLogSummary(
    const ImagePaths &images,
    const std::vector<IndexT> &active_view_ids,
    const WorkerResult &worker_result,
    double parent_wall_ms) const
{
    const std::filesystem::path log_path = ResolveSummaryLogPath();
    if (log_path.empty())
    {
        return;
    }
    std::error_code error;
    std::filesystem::create_directories(log_path.parent_path(), error);
    if (error)
    {
        return;
    }
    std::ofstream log(log_path, std::ios::out | std::ios::trunc);
    if (!log)
    {
        return;
    }

    std::size_t total_features = 0;
    for (const auto &image_keypoints : worker_result.keypoints)
    {
        total_features += image_keypoints.size();
    }
    std::size_t total_matches = 0;
    std::size_t nonempty_pairs = 0;
    for (const WorkerPair &pair : worker_result.pairs)
    {
        total_matches += pair.matches.size();
        nonempty_pairs += pair.matches.empty() ? 0U : 1U;
    }

    log << "# @view: log_view\n"
        << "# @style: preprocessor_dashboard\n"
        << "# @title: ALIKED + LightGlue Report | ALIKED + LightGlue 报告\n"
        << "# @version: 1.0\n\n"
        << "# @section: identity | 运行身份 | Runtime Identity\n"
        << "extractor=" << worker_result.extractor << '\n'
        << "matcher=" << worker_result.matcher << '\n'
        << "requested_device=" << worker_result.requested_device << '\n'
        << "device=" << worker_result.device << '\n'
        << "extractor_device=" << worker_result.extractor_device << '\n'
        << "matcher_device=" << worker_result.matcher_device << '\n'
        << "python_version=" << worker_result.python_version << '\n'
        << "torch_version=" << worker_result.torch_version << '\n'
        << "torchvision_version=" << worker_result.torchvision_version << '\n'
        << "kornia_version=" << worker_result.kornia_version << '\n'
        << "image_decoder=" << worker_result.image_decoder << '\n'
        << "lightglue_commit=" << worker_result.lightglue_commit << '\n'
        << "lightglue_archive_sha256=" << worker_result.lightglue_archive_sha256 << '\n'
        << "lightglue_tree_sha256=" << worker_result.lightglue_tree_sha256 << '\n'
        << "aliked_commit=" << worker_result.aliked_commit << '\n'
        << "aliked_model_sha256=" << worker_result.aliked_model_sha256 << '\n'
        << "lightglue_model_sha256=" << worker_result.lightglue_model_sha256 << "\n\n"
        << "# @section: overview | 概览 | Overview\n"
        << "num_images=" << active_view_ids.size() << '\n'
        << "num_pairs=" << worker_result.pairs.size() << '\n'
        << "nonempty_pairs=" << nonempty_pairs << '\n'
        << "total_features=" << total_features << '\n'
        << "total_matches=" << total_matches << '\n'
        << "avg_features="
        << (active_view_ids.empty() ? 0.0
                                    : static_cast<double>(total_features) /
                                          active_view_ids.size())
        << '\n'
        << "avg_matches="
        << (worker_result.pairs.empty() ? 0.0
                                       : static_cast<double>(total_matches) /
                                             worker_result.pairs.size())
        << "\n\n"
        << "# @section: timing | 时间 | Timing\n"
        << std::fixed << std::setprecision(3)
        << "startup_ms=" << worker_result.startup_ns / 1.0e6 << '\n'
        << "extraction_ms=" << worker_result.extraction_ns / 1.0e6 << '\n'
        << "matching_ms=" << worker_result.matching_ns / 1.0e6 << '\n'
        << "worker_total_ms=" << worker_result.worker_total_ns / 1.0e6 << '\n'
        << "frontend_wall_ms=" << parent_wall_ms << '\n'
        << "peak_rss_bytes=" << worker_result.peak_rss_bytes << "\n\n"
        << "# @section: per_view | 每图特征 | Per-view Features\n";

    for (std::size_t local = 0; local < active_view_ids.size(); ++local)
    {
        const IndexT view_id = active_view_ids[local];
        log << "view=" << view_id
            << ",features=" << worker_result.keypoints[local].size()
            << ",filename="
            << std::filesystem::path(images[view_id].first).filename().string()
            << '\n';
    }

    log << "\n# @section: per_pair | 每对匹配 | Per-pair Matches\n";
    for (const WorkerPair &pair : worker_result.pairs)
    {
        const IndexT view_i = active_view_ids[pair.image_i];
        const IndexT view_j = active_view_ids[pair.image_j];
        const std::size_t denominator = std::min(
            worker_result.keypoints[pair.image_i].size(),
            worker_result.keypoints[pair.image_j].size());
        const double ratio = denominator == 0
            ? 0.0
            : static_cast<double>(pair.matches.size()) / denominator;
        log << "pair=" << view_i << ',' << view_j
            << ",matches=" << pair.matches.size()
            << ",ratio=" << ratio << '\n';
    }
}

std::filesystem::path MethodALIKEDLightGlue::ResolveSummaryLogPath() const
{
    const std::string config_path = GetConfigPath();
    const std::regex loop_pattern(R"(run_\d{3}_[^/]+/configs/)");
    if (std::regex_search(config_path, loop_pattern))
    {
        const std::size_t configs_position = config_path.find("/configs/");
        if (configs_position != std::string::npos)
        {
            return std::filesystem::path(config_path.substr(0, configs_position)) /
                   "output/log/method_aliked_lightglue/quality_filter.log";
        }
    }
    return std::filesystem::path(OutputDirectoryManager::GetOutputBaseDirectory()) /
           "output/log/method_aliked_lightglue/quality_filter.log";
}

} // namespace PluginMethods

REGISTRATION_PLUGIN(PluginMethods::MethodALIKEDLightGlue)
