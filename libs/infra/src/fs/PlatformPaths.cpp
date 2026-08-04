#include "typeit/infra/fs/PlatformPaths.h"

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#include "typeit/core/util/Result.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;
        using core::Status;

        constexpr std::string_view kApplicationDirectory =
#ifdef _WIN32
                "TypeIt";
#else
                "typeit";
#endif

        /// An XDG value is used only when it is set, non-empty and absolute.
        /// The specification is explicit that a relative value must be ignored,
        /// and ignoring it *silently* is what would put a history database
        /// wherever the shell happened to be.
        std::optional<std::filesystem::path> absolute_from(const Environment& environment, std::string_view name) {
            const std::optional<std::string> value = environment(name);
            if (!value.has_value() || value->empty()) {
                return std::nullopt;
            }
            std::filesystem::path path{*value};
            if (!path.is_absolute()) {
                return std::nullopt;
            }
            return path;
        }

        Status missing(std::string_view what) {
            return core::fail(ErrorCode::FileNotFound,
                              std::string{what} + " is not set, so there is nowhere to keep your settings and history");
        }

    }  // namespace

    Environment system_environment() {
        return [](std::string_view name) -> std::optional<std::string> {
            // NOLINTNEXTLINE(concurrency-mt-unsafe) -- read once at startup, before any thread exists
            const char* value = std::getenv(std::string{name}.c_str());
            if (value == nullptr) {
                return std::nullopt;
            }
            return std::string{value};
        };
    }

    Result<Paths> resolve_paths(const Environment& environment) {
        Paths paths;

#ifdef _WIN32
        const std::optional<std::filesystem::path> roaming = absolute_from(environment, "APPDATA");
        const std::optional<std::filesystem::path> local = absolute_from(environment, "LOCALAPPDATA");
        if (!roaming.has_value()) {
            return std::unexpected{missing("%APPDATA%").error()};
        }
        if (!local.has_value()) {
            return std::unexpected{missing("%LOCALAPPDATA%").error()};
        }
        paths.config = *roaming / kApplicationDirectory;
        paths.data = *local / kApplicationDirectory;
        paths.cache = *local / kApplicationDirectory / "cache";
#else
        const std::optional<std::filesystem::path> home = absolute_from(environment, "HOME");
        const std::optional<std::filesystem::path> config_home = absolute_from(environment, "XDG_CONFIG_HOME");
        const std::optional<std::filesystem::path> data_home = absolute_from(environment, "XDG_DATA_HOME");
        const std::optional<std::filesystem::path> cache_home = absolute_from(environment, "XDG_CACHE_HOME");

        // HOME is only needed for the defaults. A machine with all three XDG
        // variables set and no HOME is unusual but not wrong.
        const bool needs_home = !config_home.has_value() || !data_home.has_value() || !cache_home.has_value();
        if (needs_home && !home.has_value()) {
            return std::unexpected{missing("HOME").error()};
        }

        paths.config = (config_home.has_value() ? *config_home : *home / ".config") / kApplicationDirectory;
        paths.data = (data_home.has_value() ? *data_home : *home / ".local" / "share") / kApplicationDirectory;
        paths.cache = (cache_home.has_value() ? *cache_home : *home / ".cache") / kApplicationDirectory;
#endif

        // The overrides come last and win outright, including over each other's
        // platform rules. A test that sets them gets exactly what it asked for.
        if (const std::optional<std::filesystem::path> override_config =
                    absolute_from(environment, "TYPEIT_CONFIG_DIR");
            override_config.has_value()) {
            paths.config = *override_config;
        }
        if (const std::optional<std::filesystem::path> override_data = absolute_from(environment, "TYPEIT_DATA_DIR");
            override_data.has_value()) {
            paths.data = *override_data;
            paths.cache = *override_data / "cache";
        }

        return paths;
    }

    Status ensure_directory(const std::filesystem::path& directory) {
        std::error_code failure;
        std::filesystem::create_directories(directory, failure);
        if (failure) {
            // create_directories reports no error when the directory already
            // exists, so anything here is a real failure — usually permissions,
            // or a file sitting where a directory should be.
            return core::fail(ErrorCode::FileUnreadable, directory.string() + ": " + failure.message());
        }
        if (!std::filesystem::is_directory(directory)) {
            return core::fail(ErrorCode::FileUnreadable, directory.string() + ": exists but is not a directory");
        }
        return {};
    }

}  // namespace typeit::infra
