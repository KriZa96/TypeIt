#include "typeit/infra/fs/AssetLocator.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "typeit/core/util/Result.h"
#include "typeit/infra/fs/PlatformPaths.h"

#ifdef _WIN32
#include <windows.h>
#elifdef __APPLE__
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;

        constexpr std::string_view kAssetDirectoryName = "typeit";

#ifdef _WIN32
        constexpr char kPathSeparator = ';';
#else
        constexpr char kPathSeparator = ':';
#endif

        /// `$XDG_DATA_DIRS` is a list. Its documented default is used when it
        /// is unset or empty, which is the case on a surprising number of
        /// desktop sessions.
        std::vector<std::filesystem::path> data_dirs(const Environment& environment) {
            std::string value;
            if (const std::optional<std::string> configured = environment("XDG_DATA_DIRS");
                configured.has_value() && !configured->empty()) {
                value = *configured;
            } else {
                value = "/usr/local/share:/usr/share";
            }

            std::vector<std::filesystem::path> directories;
            std::size_t start = 0;
            while (start <= value.size()) {
                const std::size_t end = value.find(kPathSeparator, start);
                const std::string entry =
                        value.substr(start, end == std::string::npos ? std::string::npos : end - start);
                if (!entry.empty()) {
                    directories.emplace_back(entry);
                }
                if (end == std::string::npos) {
                    break;
                }
                start = end + 1;
            }
            return directories;
        }

    }  // namespace

    std::filesystem::path configured_install_prefix() { return TYPEIT_INSTALL_PREFIX; }

    std::vector<std::filesystem::path> asset_search_path(const AssetSearch& search) {
        std::vector<std::filesystem::path> candidates;

        if (search.environment) {
            if (const std::optional<std::string> configured = search.environment("TYPEIT_ASSETS_DIR");
                configured.has_value() && !configured->empty()) {
                candidates.emplace_back(*configured);
            }
        }

        if (!search.executable.empty()) {
            // `<prefix>/bin/typeit` → `<prefix>/share/typeit`. This is the entry
            // that makes a relocated install work: it asks where the binary
            // actually is, not where it was built.
            candidates.push_back(search.executable.parent_path().parent_path() / "share" / kAssetDirectoryName);
        }

        if (!search.install_prefix.empty()) {
            candidates.push_back(search.install_prefix / "share" / kAssetDirectoryName);
        }

        if (search.environment) {
            for (const std::filesystem::path& directory: data_dirs(search.environment)) {
                candidates.push_back(directory / kAssetDirectoryName);
            }
        }

        if (!search.working_directory.empty()) {
            // Development only: running straight out of a build tree, where
            // there is no install to have been relocated.
            candidates.push_back(search.working_directory / "assets");
        }

        return candidates;
    }

    Result<std::filesystem::path> locate_assets(const AssetSearch& search) {
        const std::vector<std::filesystem::path> candidates = asset_search_path(search);

        for (const std::filesystem::path& candidate: candidates) {
            std::error_code ignored;
            if (std::filesystem::is_directory(candidate, ignored)) {
                return candidate;
            }
        }

        std::string tried;
        for (const std::filesystem::path& candidate: candidates) {
            if (!tried.empty()) {
                tried += ", ";
            }
            tried += candidate.string();
        }
        return core::fail(ErrorCode::FileNotFound, "no bundled texts found; looked in: " + tried);
    }

    Result<std::filesystem::path> current_executable() {
        std::error_code failure;

#ifdef _WIN32
        std::wstring buffer(MAX_PATH, L'\0');
        for (;;) {
            const DWORD written = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (written == 0) {
                return core::fail(ErrorCode::FileNotFound, "GetModuleFileNameW failed");
            }
            if (written < buffer.size()) {
                buffer.resize(written);
                break;
            }
            buffer.resize(buffer.size() * 2);
        }
        const std::filesystem::path self{buffer};
#elifdef __APPLE__
        std::uint32_t size = 0;
        _NSGetExecutablePath(nullptr, &size);
        std::string buffer(size, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            return core::fail(ErrorCode::FileNotFound, "_NSGetExecutablePath failed");
        }
        const std::filesystem::path self{buffer.c_str()};
#else
        const std::filesystem::path self{"/proc/self/exe"};
#endif

        // canonical() resolves the symlink, which is the whole point on the
        // `/usr/games/TypeIt -> …/build/TypeIt` path: looking beside the
        // symlink finds nothing.
        std::filesystem::path resolved = std::filesystem::canonical(self, failure);
        if (failure) {
            return core::fail(ErrorCode::FileNotFound, "cannot resolve the running executable: " + failure.message());
        }
        return resolved;
    }

}  // namespace typeit::infra
