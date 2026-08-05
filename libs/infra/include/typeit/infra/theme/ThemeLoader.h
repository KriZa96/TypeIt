// Reading a theme file (TI-083, UX §4).
//
// In `infra` because it parses TOML, and `tui` may not link toml++ — the same
// split the configuration already has: `core::Config` is the value,
// `TomlConfigStore` is the parser. A theme is `app::Theme`, this is its
// parser, and `tui` receives a value it does not have to understand a file
// format to use.
//
// Nothing here is fatal that can be survived. A missing colour takes the
// default, an unknown key is ignored, and a theme that will not parse at all
// is the one case that fails — because that is the one case where continuing
// would mean rendering a theme the author cannot see the effect of.
#ifndef TYPEIT_INFRA_THEME_THEMELOADER_H
#define TYPEIT_INFRA_THEME_THEMELOADER_H

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/Theme.h"
#include "typeit/core/util/Result.h"

namespace typeit::infra {

    /// A theme that loaded, and what it complained about on the way.
    ///
    /// Warnings rather than failures: a theme with one bad colour in it is a
    /// theme somebody can still type against, and refusing to start over a
    /// typo in `#ff00zz` would be a worse answer than drawing that one element
    /// in the default.
    struct LoadedTheme {
        app::Theme theme;
        std::vector<std::string> warnings;
    };

    /// Parses `text` as a theme file.
    ///
    /// Fails only when the file is not TOML at all
    /// (`ErrorCode::ConfigParse`, naming the line). Everything else — an
    /// unknown key, a missing colour, a malformed hex value, a
    /// `theme_version` from the future — is a warning against the default.
    [[nodiscard]] core::Result<LoadedTheme> parse_theme(std::string_view text);

    /// The same, from a file. `ErrorCode::FileNotFound` when there is no such
    /// theme, so a caller can fall back to the default and say why.
    [[nodiscard]] core::Result<LoadedTheme> load_theme(const std::filesystem::path& file);

    /// The themes that ship, by name, in the order a menu should list them.
    [[nodiscard]] std::vector<std::string> built_in_theme_names();

    /// `<assets>/themes/<name>.toml`, then the user's own directory first if
    /// one is given — a user theme of the same name overrides a built-in,
    /// which is what makes "copy it and edit it" the way to customise one.
    [[nodiscard]] core::Result<LoadedTheme> find_theme(std::string_view name, const std::filesystem::path& user_themes,
                                                       const std::filesystem::path& built_in_themes);

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_THEME_THEMELOADER_H
