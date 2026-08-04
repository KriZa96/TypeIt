// Where the settings live (TECHNICAL section 6).
#ifndef TYPEIT_APP_PORTS_ICONFIGSTORE_H
#define TYPEIT_APP_PORTS_ICONFIGSTORE_H

#include <string>
#include <vector>

#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {

    /// A load that succeeded with complaints. An unknown key, or a value out of
    /// range that fell back to its default, must reach the user — silently
    /// ignoring half a config file is how a setting becomes "it does not work".
    struct LoadedConfig {
        core::Config config;
        std::vector<std::string> warnings;
    };

    class IConfigStore {
    public:
        IConfigStore() = default;
        virtual ~IConfigStore();
        IConfigStore(const IConfigStore&) = delete;
        IConfigStore& operator=(const IConfigStore&) = delete;
        IConfigStore(IConfigStore&&) = delete;
        IConfigStore& operator=(IConfigStore&&) = delete;

        /// A missing file is not a failure — it is a first run, and the
        /// defaults are the answer. A *malformed* file is a failure, and the
        /// file is never overwritten because of one.
        [[nodiscard]] virtual core::Result<LoadedConfig> load() = 0;

        [[nodiscard]] virtual core::Status save(const core::Config& config) = 0;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_PORTS_ICONFIGSTORE_H
