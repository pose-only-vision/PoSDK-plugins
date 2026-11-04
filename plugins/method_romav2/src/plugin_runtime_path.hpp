/**
 * @file plugin_runtime_path.hpp
 * @brief Resolve declared plugin runtime files relative to the loaded module.
 *
 * A plugin runs in posdk_runner, so the process executable and current working
 * directory do not identify the active plugin generation.  Callers must pass
 * an address of a translation-unit-local function compiled into their own
 * plugin.  dladdr then identifies the exact dylib/so selected by the loader.
 */
#pragma once

#include <filesystem>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <system_error>
#include <vector>

#if defined(__APPLE__) || defined(__linux__)
#include <dlfcn.h>
#include <unistd.h>
#endif

#ifndef POSDK_PLUGIN_CONSUMER_BUILD
#define POSDK_PLUGIN_CONSUMER_BUILD 0
#endif
#ifndef POSDK_PLUGIN_PACKAGED_BUILD
#define POSDK_PLUGIN_PACKAGED_BUILD POSDK_PLUGIN_CONSUMER_BUILD
#endif

// Do not merely ignore fallbacks at runtime: in packaged builds the variadic
// tokens are omitted by the preprocessor, so source-tree paths and absolute
// build-machine paths never enter the binary.
#if POSDK_PLUGIN_PACKAGED_BUILD
#define POSDK_PLUGIN_DEVELOPER_FALLBACKS(...) {}
#else
#define POSDK_PLUGIN_DEVELOPER_FALLBACKS(...) {__VA_ARGS__}
#endif

namespace PoSDK
{
namespace PluginRuntime
{
namespace detail
{
namespace fs = std::filesystem;

inline bool IsPortableRelativePath(const fs::path &path)
{
    if (path.empty() || path.is_absolute() || path.has_root_name() ||
        path.has_root_directory())
    {
        return false;
    }

    for (const fs::path &component : path)
    {
        if (component.empty() || component == "." || component == "..")
        {
            return false;
        }
    }
    return true;
}

inline fs::path CanonicalRegularFile(const fs::path &candidate)
{
    std::error_code ec;
    if (!fs::is_regular_file(candidate, ec) || ec)
    {
        return {};
    }

    fs::path canonical = fs::canonical(candidate, ec);
    return ec ? fs::path{} : canonical;
}

inline fs::path CanonicalDirectory(const fs::path &directory)
{
    std::error_code ec;
    if (!fs::is_directory(directory, ec) || ec)
    {
        return {};
    }

    fs::path canonical = fs::canonical(directory, ec);
    return ec ? fs::path{} : canonical;
}

inline bool IsStrictlyWithin(const fs::path &root, const fs::path &candidate)
{
    const fs::path relative = candidate.lexically_relative(root);
    if (relative.empty() || relative.is_absolute())
    {
        return false;
    }

    const auto first = relative.begin();
    return first != relative.end() && *first != "..";
}

inline bool IsPortablePathComponent(const std::string &value)
{
    if (value.empty() || value == "." || value == "..")
    {
        return false;
    }
    return value.find('/') == std::string::npos &&
           value.find('\\') == std::string::npos &&
           value.find('\0') == std::string::npos;
}

inline bool IsExecutableFile(const fs::path &candidate)
{
    std::error_code ec;
    if (!fs::is_regular_file(candidate, ec) || ec)
    {
        return false;
    }
#if defined(__APPLE__) || defined(__linux__)
    return ::access(candidate.c_str(), X_OK) == 0;
#else
    return true;
#endif
}
} // namespace detail

using ModuleAnchor = void (*)();

/**
 * Quote exactly one argument for a POSIX shell.  Single quotes suppress command
 * substitution, variable expansion, globbing, and whitespace splitting.  An
 * embedded quote is represented by closing the quote, emitting an escaped
 * quote, and reopening it.  The Python-backed plugins currently execute
 * through a POSIX shell on macOS/Linux; Windows process launching is a separate
 * command-line contract and must not use this helper.
 */
inline std::string QuotePosixShellArgument(const std::string &argument)
{
    std::string quoted;
    quoted.reserve(argument.size() + 2);
    quoted.push_back('\'');
    for (const char character : argument)
    {
        if (character == '\'')
        {
            quoted += "'\\''";
        }
        else
        {
            quoted.push_back(character);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

inline std::filesystem::path ModuleFileForAddress(ModuleAnchor anchor)
{
#if defined(__APPLE__) || defined(__linux__)
    if (anchor == nullptr)
    {
        return {};
    }

    Dl_info info{};
    if (dladdr(reinterpret_cast<const void *>(anchor), &info) == 0 ||
        info.dli_fname == nullptr || info.dli_fname[0] == '\0')
    {
        return {};
    }

    std::filesystem::path module_file(info.dli_fname);
    if (module_file.is_relative())
    {
        std::error_code ec;
        module_file = std::filesystem::absolute(module_file, ec);
        if (ec)
        {
            return {};
        }
    }

    std::error_code ec;
    const std::filesystem::path canonical =
        std::filesystem::canonical(module_file, ec);
    return ec ? std::filesystem::path{} : canonical;
#else
    (void)anchor;
    return {};
#endif
}

inline std::filesystem::path ModuleDirectoryForAddress(ModuleAnchor anchor)
{
    const std::filesystem::path module_file = ModuleFileForAddress(anchor);
    return module_file.empty() ? std::filesystem::path{}
                               : module_file.parent_path();
}

/**
 * Pure path-resolution half of FindRuntimeFile(), exposed for deterministic
 * tests.  The module-adjacent file always wins.  A symlink escaping the module
 * directory is not accepted as a module-owned runtime file.
 */
inline std::filesystem::path FindRuntimeFileFromDirectory(
    const std::filesystem::path &module_directory,
    const std::filesystem::path &relative_runtime_file,
    std::initializer_list<std::filesystem::path> developer_fallbacks = {})
{
    namespace fs = std::filesystem;
    if (!detail::IsPortableRelativePath(relative_runtime_file))
    {
        return {};
    }

    const fs::path canonical_module_dir =
        detail::CanonicalDirectory(module_directory);
    if (!canonical_module_dir.empty())
    {
        const fs::path adjacent = detail::CanonicalRegularFile(
            canonical_module_dir / relative_runtime_file);
        if (!adjacent.empty() &&
            detail::IsStrictlyWithin(canonical_module_dir, adjacent))
        {
            return adjacent;
        }
    }

    // These candidates exist only for source-tree/build-tree compatibility.
#if POSDK_PLUGIN_PACKAGED_BUILD
    (void)developer_fallbacks;
    return {};
#else
    for (const fs::path &fallback : developer_fallbacks)
    {
        if (fallback.empty())
        {
            continue;
        }

        const fs::path canonical = detail::CanonicalRegularFile(fallback);
        if (!canonical.empty())
        {
            return canonical;
        }
    }
    return {};
#endif
}

inline std::filesystem::path FindRuntimeFile(
    ModuleAnchor anchor,
    const std::filesystem::path &relative_runtime_file,
    std::initializer_list<std::filesystem::path> developer_fallbacks = {})
{
    return FindRuntimeFileFromDirectory(ModuleDirectoryForAddress(anchor),
                                        relative_runtime_file,
                                        developer_fallbacks);
}

/**
 * Resolve an executable produced by a signed .pospkg v3 installer.  Installer
 * output is intentionally outside the immutable plugin generation and lives
 * below the distribution-specific PoSDK state root:
 *
 *   installer/<plugin>/<version>/<system>-<arch>/bin/<executable>
 *
 * The pure roots overload is exposed for deterministic tests.  A final
 * symlink or a path escaping the exact installer state is rejected.
 */
inline std::filesystem::path FindInstalledExecutableFromStateRoots(
    const std::vector<std::filesystem::path> &state_roots,
    const std::string &plugin_id,
    const std::string &plugin_version,
    const std::string &target_system,
    const std::string &target_architecture,
    const std::string &executable_name)
{
    namespace fs = std::filesystem;
    if (!detail::IsPortablePathComponent(plugin_id) ||
        !detail::IsPortablePathComponent(plugin_version) ||
        !detail::IsPortablePathComponent(target_system) ||
        !detail::IsPortablePathComponent(target_architecture) ||
        !detail::IsPortablePathComponent(executable_name))
    {
        return {};
    }

    const fs::path relative_root = fs::path("installer") / plugin_id /
                                   plugin_version /
                                   (target_system + "-" + target_architecture);
    for (const fs::path &state_root : state_roots)
    {
        const fs::path canonical_state = detail::CanonicalDirectory(state_root);
        if (canonical_state.empty())
        {
            continue;
        }
        const fs::path install_root = detail::CanonicalDirectory(
            canonical_state / relative_root);
        if (install_root.empty() ||
            !detail::IsStrictlyWithin(canonical_state, install_root))
        {
            continue;
        }
        const fs::path candidate = install_root / "bin" / executable_name;
        if (fs::is_symlink(candidate) || !detail::IsExecutableFile(candidate))
        {
            continue;
        }
        std::error_code ec;
        const fs::path canonical = fs::canonical(candidate, ec);
        if (!ec && detail::IsStrictlyWithin(install_root, canonical))
        {
            return canonical;
        }
    }
    return {};
}

/**
 * Resolve an installer binding through its version-independent stable path:
 *
 *   plugin-bin/<plugin>/<binding>/<executable>
 *
 * The binding itself must be a symlink whose canonical target remains below
 * this plugin's installer tree.  The executable must be a real executable
 * file inside the verified target directory.  This lets plugin upgrades move
 * to a new versioned state without changing workflow configuration.
 */
inline std::filesystem::path FindStableInstalledExecutableFromStateRoots(
    const std::vector<std::filesystem::path> &state_roots,
    const std::string &plugin_id,
    const std::string &binding_id,
    const std::string &executable_name)
{
    namespace fs = std::filesystem;
    if (!detail::IsPortablePathComponent(plugin_id) ||
        !detail::IsPortablePathComponent(binding_id) ||
        !detail::IsPortablePathComponent(executable_name))
    {
        return {};
    }
    for (const fs::path &state_root : state_roots)
    {
        const fs::path canonical_state = detail::CanonicalDirectory(state_root);
        if (canonical_state.empty())
        {
            continue;
        }
        const fs::path plugin_installer_root = detail::CanonicalDirectory(
            canonical_state / "installer" / plugin_id);
        const fs::path binding_root = canonical_state / "plugin-bin" /
                                      plugin_id / binding_id;
        std::error_code ec;
        if (plugin_installer_root.empty() || !fs::is_symlink(binding_root, ec) || ec)
        {
            continue;
        }
        const fs::path canonical_binding = detail::CanonicalDirectory(binding_root);
        if (canonical_binding.empty() ||
            !detail::IsStrictlyWithin(plugin_installer_root, canonical_binding))
        {
            continue;
        }
        const fs::path candidate = binding_root / executable_name;
        if (fs::is_symlink(candidate, ec) || ec ||
            !detail::IsExecutableFile(candidate))
        {
            continue;
        }
        const fs::path canonical = fs::canonical(candidate, ec);
        if (!ec && detail::IsStrictlyWithin(canonical_binding, canonical))
        {
            return canonical;
        }
    }
    return {};
}

inline std::vector<std::filesystem::path> DefaultUserStateRoots()
{
    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
    const char *configured = std::getenv("POSDK_USER_STATE_ROOT");
    if (configured != nullptr && configured[0] != '\0')
    {
        roots.emplace_back(configured);
    }
    const char *home = std::getenv("HOME");
    if (home == nullptr || home[0] == '\0')
    {
        return roots;
    }
#if defined(__APPLE__)
    const fs::path application_support =
        fs::path(home) / "Library" / "Application Support" / "PoSDK";
    roots.push_back(application_support / "PoSDK GUI");
    roots.push_back(application_support / "PoSDK GUI Developer");
#else
    roots.push_back(fs::path(home) / ".posdk");
#endif
    return roots;
}

inline std::filesystem::path FindInstalledExecutable(
    const std::string &plugin_id,
    const std::string &plugin_version,
    const std::string &target_system,
    const std::string &target_architecture,
    const std::string &executable_name)
{
    return FindInstalledExecutableFromStateRoots(
        DefaultUserStateRoots(), plugin_id, plugin_version, target_system,
        target_architecture, executable_name);
}

inline std::filesystem::path FindStableInstalledExecutable(
    const std::string &plugin_id,
    const std::string &binding_id,
    const std::string &executable_name)
{
    return FindStableInstalledExecutableFromStateRoots(
        DefaultUserStateRoots(), plugin_id, binding_id, executable_name);
}

inline std::string CurrentTargetSystem()
{
#if defined(__APPLE__)
    return "macos";
#elif defined(_WIN32)
    return "windows";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

inline std::string CurrentTargetArchitecture()
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

/** Resolve one command through PATH and return an absolute executable path. */
inline std::filesystem::path FindExecutableOnSearchPath(
    const std::string &executable_name, const std::string &search_path)
{
    namespace fs = std::filesystem;
    if (!detail::IsPortablePathComponent(executable_name))
    {
        return {};
    }
    std::size_t begin = 0;
    while (begin <= search_path.size())
    {
        const std::size_t end = search_path.find(':', begin);
        const std::string component = search_path.substr(
            begin, end == std::string::npos ? std::string::npos : end - begin);
        if (!component.empty())
        {
            const fs::path candidate = fs::path(component) / executable_name;
            if (!fs::is_symlink(candidate) && detail::IsExecutableFile(candidate))
            {
                std::error_code ec;
                const fs::path canonical = fs::canonical(candidate, ec);
                if (!ec)
                {
                    return canonical;
                }
            }
        }
        if (end == std::string::npos)
        {
            break;
        }
        begin = end + 1;
    }
    return {};
}

inline std::filesystem::path FindExecutableOnPath(
    const std::string &executable_name)
{
    const char *path = std::getenv("PATH");
    return FindExecutableOnSearchPath(executable_name,
                                      path == nullptr ? "" : path);
}

} // namespace PluginRuntime
} // namespace PoSDK
