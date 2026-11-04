#include "method_romav2.hpp"
#include "plugin_runtime_path.hpp"
#include <po_core/po_logger.hpp>
#include <po_core/ProfilerManager.hpp>
#include <opencv2/imgcodecs.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <signal.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

extern char **environ;
namespace PluginMethods {
using namespace PoSDK;
using namespace PoSDK::Interface;
using namespace PoSDK::types;
namespace {
void RuntimeAnchor() {}
struct Temp {
    std::filesystem::path path;
    ~Temp() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};
std::size_t Count(std::istream &in, std::size_t maximum) {
    std::size_t value = 0;
    if (!(in >> value) || value > maximum)
        throw std::runtime_error("Invalid RoMa v2 result count");
    return value;
}
void End(std::istream &in) {
    std::string extra;
    if (in >> extra) throw std::runtime_error("Unexpected RoMa v2 result data");
}
bool RunProcess(std::vector<std::string> command, int seconds) {
    std::vector<char *> argv;
    for (auto &arg : command) argv.push_back(arg.data());
    argv.push_back(nullptr);
    posix_spawnattr_t attributes;
    if (posix_spawnattr_init(&attributes) != 0)
        throw std::runtime_error("Cannot create process attributes");
    posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
    posix_spawnattr_setpgroup(&attributes, 0);
    pid_t child = -1;
    int error = posix_spawn(&child, argv[0], nullptr, &attributes, argv.data(), environ);
    posix_spawnattr_destroy(&attributes);
    if (error != 0) throw std::runtime_error("Cannot launch RoMa v2 SSH controller");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    int status = 0;
    for (;;) {
        const auto waited = waitpid(child, &status, WNOHANG);
        if (waited == child) return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        if (waited < 0 && errno != EINTR) {
            kill(-child, SIGTERM);
            throw std::runtime_error("Cannot monitor RoMa v2 controller");
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            kill(-child, SIGTERM);
            for (int retry = 0; retry < 100; ++retry) {
                if (waitpid(child, &status, WNOHANG) == child) return false;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            kill(-child, SIGKILL);
            waitpid(child, &status, 0);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}
}

MethodRoMaV2::MethodRoMaV2() {
    required_package_["data_images"] = nullptr;
    output_types_ = {"data_features", "data_matches"};
    InitializeDefaultConfigPath();
}

DataPtr MethodRoMaV2::Run() {
    try {
        auto images = GetRequiredDataPtr<ImagePaths>("data_images");
        if (!images || images->size() < 2 || images->size() > 200)
            throw std::runtime_error("RoMa v2 requires 2..200 input images");
        const auto worker = PluginRuntime::FindRuntimeFile(&RuntimeAnchor, "romav2_worker.py");
        if (worker.empty()) throw std::runtime_error("Missing declared RoMa v2 worker");
        const auto python_folder = GetOptionAsString("python_bin_folder", "");
        const auto python = python_folder.empty()
            ? GetOptionAsString("python_executable", "/usr/bin/python3")
            : (std::filesystem::path(python_folder) / "python3.11").string();
        if (!std::filesystem::path(python).is_absolute() || access(python.c_str(), X_OK))
            throw std::runtime_error("Controller Python must be an absolute executable");
        std::string pattern = (std::filesystem::temp_directory_path() / "posdk-romav2-XXXXXX").string();
        char *created = mkdtemp(pattern.data());
        if (!created) throw std::runtime_error("Cannot create private RoMa v2 workspace");
        Temp temp{created};
        const auto output = temp.path / "output";
        const auto request = temp.path / "images.txt";
        {
            std::ofstream list(request);
            list << images->size() << '\n';
            for (const auto &image : *images) {
                if (image.first.find_first_of("\r\n") != std::string::npos ||
                    !std::filesystem::is_regular_file(image.first))
                    throw std::runtime_error("Invalid input image path");
                list << image.first << '\n';
            }
            if (!list.good()) throw std::runtime_error("Cannot write RoMa v2 request");
        }
        int timeout = static_cast<int>(GetOptionAsIndexT("timeout_seconds", 7200));
        if (timeout < 30 || timeout > 86400) throw std::runtime_error("Invalid RoMa v2 timeout");
        std::vector<std::string> command{python, "-u", worker.string(),
            "--input", request.string(), "--output-dir", output.string()};
        const std::vector<std::pair<std::string, std::string>> defaults = {
            {"ssh_target", "remoteMAC"}, {"ssh_python_executable", "python3"},
            {"ssh_work_dir", "~/.cache/PoSDK/RoMaV2"}, {"runtime_root", ""},
            {"remote_image_root", ""},
            {"setting", "fast"}, {"device", "mps"}, {"cpu_threads", "4"},
            {"num_matches", "5000"}, {"confidence_threshold", "0.2"},
            {"keypoint_merge_radius", "1.0"}, {"seed", "31"}};
        for (const auto &option : defaults) {
            auto flag = option.first;
            std::replace(flag.begin(), flag.end(), '_', '-');
            command.push_back("--" + flag);
            command.push_back(GetOptionAsString(option.first, option.second));
        }
        command.insert(command.end(), {"--timeout-seconds", std::to_string(timeout)});
        LOG_INFO_EN << "[RoMa v2] SSH matching started: " << images->size() << " images";
        bool ok;
        {
            POSDK_START(GetOptionAsBool("enable_profiling", true));
            PROFILER_STAGE("RoMa v2 SSH matching");
            ok = RunProcess(std::move(command), timeout + 120);
            PROFILER_END();
        }
        if (!ok) throw std::runtime_error("RoMa v2 SSH execution failed; no partial output accepted");
        auto feature_data = FactoryData::Create("data_features");
        auto match_data = FactoryData::Create("data_matches");
        auto features = GetDataPtr<FeaturesInfo>(feature_data);
        auto matches = GetDataPtr<Matches>(match_data);
        if (!features || !matches) throw std::runtime_error("Cannot create standard output data");
        std::vector<std::size_t> counts;
        std::size_t feature_count = 0, match_count = 0;
        for (std::size_t i = 0; i < images->size(); ++i) {
            std::ifstream stream(output / ("features_" + std::to_string(i) + ".txt"));
            const auto count = Count(stream, 2000000);
            counts.push_back(count);
            const auto color = cv::imread((*images)[i].first,
                cv::IMREAD_COLOR | cv::IMREAD_IGNORE_ORIENTATION);
            if (color.empty()) throw std::runtime_error("Cannot decode original-image RGB");
            features->AddImageFeatures((*images)[i].first, true);
            auto &image_features = features->back();
            for (std::size_t k = 0; k < count; ++k) {
                float x, y;
                if (!(stream >> x >> y) || !std::isfinite(x) || !std::isfinite(y) ||
                    x < -.501f || y < -.501f || x > color.cols-.499f || y > color.rows-.499f)
                    throw std::runtime_error("Invalid original-image feature coordinate");
                image_features.AddFeature(x, y, 1.f, -1.f);
                const auto rgb = color.at<cv::Vec3b>(
                    std::lround(std::clamp(y, 0.f, float(color.rows-1))),
                    std::lround(std::clamp(x, 0.f, float(color.cols-1))));
                image_features.GetFeature(k).SetColorRGB(rgb[2], rgb[1], rgb[0]);
            }
            End(stream);
            feature_count += count;
        }
        for (std::size_t i = 0; i < images->size(); ++i) {
            for (std::size_t j = i+1; j < images->size(); ++j) {
                std::ifstream stream(output / ("pair_" + std::to_string(i) + "_" + std::to_string(j) + ".txt"));
                const auto count = Count(stream, 10000);
                IdMatches pair;
                std::set<std::size_t> left, right;
                for (std::size_t k = 0; k < count; ++k) {
                    const auto a = Count(stream, counts[i] ? counts[i]-1 : 0);
                    const auto b = Count(stream, counts[j] ? counts[j]-1 : 0);
                    double confidence;
                    int flag;
                    if (!counts[i] || !counts[j] || !(stream >> confidence >> flag) ||
                        !std::isfinite(confidence) || confidence < 0 || confidence > 1 ||
                        flag != 0 || !left.insert(a).second || !right.insert(b).second)
                        throw std::runtime_error("Invalid/duplicate RoMa v2 association");
                    IdMatch match;
                    match.i = static_cast<IndexT>(a);
                    match.j = static_cast<IndexT>(b);
                    match.is_inlier = false; // Geometric verification belongs to TwoViewEstimator.
                    pair.push_back(match);
                }
                End(stream);
                match_count += count;
                if (count) (*matches)[ViewPair(i, j)] = std::move(pair);
            }
        }
        const auto diagnostics = GetOptionAsString("diagnostics_dir", "");
        if (!diagnostics.empty()) {
            const std::filesystem::path destination(diagnostics);
            if (!destination.is_absolute()) throw std::runtime_error("Diagnostics path must be absolute");
            std::filesystem::create_directories(destination);
            for (const char *name : {"inference.json", "association.json", "ssh-receipt.json", "worker.log"})
                std::filesystem::copy_file(output / name, destination / name,
                    std::filesystem::copy_options::overwrite_existing);
        }
        LOG_INFO_EN << "[RoMa v2] Complete: " << feature_count << " RGB features, "
                    << match_count << " matches, " << matches->size() << " nonempty pairs";
        auto package = std::make_shared<DataPackage>();
        package->AddData("data_features", feature_data);
        package->AddData("data_matches", match_data);
        return std::static_pointer_cast<DataIO>(package);
    } catch (const std::exception &error) {
        LOG_ERROR_EN << "[RoMa v2] " << error.what();
        return nullptr;
    }
}
}
REGISTRATION_PLUGIN(PluginMethods::MethodRoMaV2)
