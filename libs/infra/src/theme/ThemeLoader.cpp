#include "typeit/infra/theme/ThemeLoader.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <toml++/toml.hpp>
#include <vector>

#include "typeit/app/Theme.h"
#include "typeit/core/util/Result.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;

        /// The order a menu lists them in: the two everyday themes, then the
        /// two that exist for terminals that cannot do better.
        constexpr std::array<std::string_view, 4> kBuiltIn{"typeit-dark", "typeit-light", "high-contrast", "mono"};

        std::optional<std::string> text_of(const toml::table& file, std::string_view key) {
            const auto node = file.at_path(key);
            if (!node) {
                return std::nullopt;
            }
            if (const std::optional<std::string> value = node.value<std::string>(); value.has_value()) {
                return value;
            }
            return std::nullopt;
        }

        void read_colors(const toml::table& file, LoadedTheme& loaded) {
            const auto node = file.at_path("colors");
            if (!node) {
                loaded.warnings.emplace_back("no [colors] table; every colour is the default");
                return;
            }

            const toml::table* colors = node.as_table();
            if (colors == nullptr) {
                loaded.warnings.emplace_back("colors: expected a table; every colour is the default");
                return;
            }

            for (const auto& [key, value]: *colors) {
                const std::string name{key.str()};

                app::ThemeColor which{};
                if (!app::theme_color_from(name, which)) {
                    // Ignored rather than refused: a theme written for a later
                    // version should still load on this one (VERSIONING §5).
                    loaded.warnings.emplace_back("colors." + name + ": unknown colour; ignored");
                    continue;
                }

                const std::optional<std::string> spelled = value.value<std::string>();
                app::Rgb parsed;
                if (!spelled.has_value() || !app::rgb_from_hex(*spelled, parsed)) {
                    loaded.warnings.emplace_back("colors." + name + " = \"" + spelled.value_or(std::string{}) +
                                                 "\" (expected #rrggbb); using the default");
                    continue;
                }
                loaded.theme.set(which, parsed);
            }

            // Named individually, because "your theme is missing three
            // colours" sends somebody back to the file to work out which.
            for (const app::ThemeColor which: app::kAllThemeColors) {
                if (!colors->contains(app::to_string(which))) {
                    loaded.warnings.emplace_back(std::string{"colors."} + std::string{app::to_string(which)} +
                                                 ": not set; using the default");
                }
            }
        }

    }  // namespace

    core::Result<LoadedTheme> parse_theme(std::string_view text) {
        toml::table file;
        try {
            file = toml::parse(text);
        } catch (const toml::parse_error& failure) {
            // The one fatal case. A file that is not TOML is a file whose
            // author cannot see the effect of what they wrote.
            return core::fail(ErrorCode::ConfigParse, "line " + std::to_string(failure.source().begin.line) + ": " +
                                                              std::string{failure.description()});
        }

        LoadedTheme loaded;
        if (const std::optional<std::string> name = text_of(file, "name"); name.has_value()) {
            loaded.theme.name = *name;
        } else {
            loaded.warnings.emplace_back("no name; the theme cannot be selected by one");
        }
        loaded.theme.author = text_of(file, "author").value_or(std::string{});
        loaded.theme.description = text_of(file, "description").value_or(std::string{});

        // A theme from a later format still loads: unknown keys are ignored, so
        // most theme changes need no bump at all (VERSIONING §5). The version is
        // reported rather than obeyed, so a file that does need something newer
        // says so instead of quietly rendering wrong.
        if (const std::optional<std::int64_t> version = file.at_path("theme_version").value<std::int64_t>();
            version.has_value() && *version != app::kThemeVersion) {
            loaded.warnings.emplace_back("theme_version = " + std::to_string(*version) + "; this binary knows " +
                                         std::to_string(app::kThemeVersion) +
                                         ". Unknown keys are ignored, so it may still be fine.");
        }

        read_colors(file, loaded);
        return loaded;
    }

    core::Result<LoadedTheme> load_theme(const std::filesystem::path& file) {
        std::error_code failed;
        if (!std::filesystem::is_regular_file(file, failed)) {
            return core::fail(ErrorCode::FileNotFound, file.string());
        }

        std::ifstream stream{file, std::ios::binary};
        if (!stream) {
            return core::fail(ErrorCode::FileUnreadable, file.string());
        }
        std::ostringstream contents;
        contents << stream.rdbuf();

        core::Result<LoadedTheme> loaded = parse_theme(contents.str());
        if (!loaded) {
            // The path, so the message names the file rather than a line number
            // in a file nobody said the name of.
            return core::fail(loaded.error().code, file.string() + ": " + loaded.error().context);
        }
        return loaded;
    }

    std::vector<std::string> built_in_theme_names() { return {kBuiltIn.begin(), kBuiltIn.end()}; }

    core::Result<LoadedTheme> find_theme(std::string_view name, const std::filesystem::path& user_themes,
                                         const std::filesystem::path& built_in_themes) {
        const std::string file = std::string{name} + ".toml";

        // The user's directory first: copying a built-in and editing it is how
        // somebody customises a theme, and it would be no use if the original
        // kept winning.
        for (const std::filesystem::path& directory: {user_themes, built_in_themes}) {
            if (directory.empty()) {
                continue;
            }
            const std::filesystem::path candidate = directory / file;
            std::error_code failed;
            if (std::filesystem::is_regular_file(candidate, failed)) {
                return load_theme(candidate);
            }
        }
        return core::fail(ErrorCode::UnknownTheme, std::string{name});
    }

}  // namespace typeit::infra
