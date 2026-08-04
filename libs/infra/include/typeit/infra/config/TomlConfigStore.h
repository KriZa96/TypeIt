// The configuration file, read and written (TECHNICAL section 6).
//
// The rules this type exists to keep:
//
//   * a missing file is a first run, not a failure — the defaults are the
//     answer, and a commented template is written so the file is discoverable;
//   * a *malformed* file is a failure that is reported with a line number and
//     **never overwritten**. Somebody hand-wrote that file; losing it because
//     we could not parse it would be the worst thing this class could do;
//   * an unknown key is a warning, not an error, so a config written by a
//     newer TypeIt still loads on an older one;
//   * a value out of range falls back to its default and says so, naming the
//     key — silently ignoring half a file is how a setting becomes "it does
//     not work".
#ifndef TYPEIT_INFRA_CONFIG_TOMLCONFIGSTORE_H
#define TYPEIT_INFRA_CONFIG_TOMLCONFIGSTORE_H

#include <filesystem>
#include <string>
#include <utility>

#include "typeit/app/ports/IConfigStore.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"

namespace typeit::infra {

    class TomlConfigStore final : public app::IConfigStore {
    public:
        explicit TomlConfigStore(std::filesystem::path file) : file_{std::move(file)} {}

        [[nodiscard]] core::Result<app::LoadedConfig> load() override;

        /// Written to a temporary file and renamed over the original, so a
        /// crash or a full disk mid-write leaves the previous configuration
        /// intact rather than a truncated one.
        [[nodiscard]] core::Status save(const core::Config& config) override;

        [[nodiscard]] const std::filesystem::path& path() const noexcept { return file_; }

        /// The file written on a first run: every default, with the comment
        /// that explains it. A configuration nobody can read is a configuration
        /// nobody changes.
        [[nodiscard]] static std::string render(const core::Config& config);

    private:
        std::filesystem::path file_;
    };

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_CONFIG_TOMLCONFIGSTORE_H
