#include "typeit/infra/Dependencies.h"

#include <sqlite3.h>
#include <string>
#include <string_view>
#include <toml++/toml.h>

namespace typeit::infra {

    std::string_view sqlite_version() { return sqlite3_libversion(); }

    std::string_view toml_version() {
        // toml++ reports its version as three integer macros, so the string is
        // assembled once, on first use, and lives as long as the process.
        static const std::string version = std::to_string(TOML_LIB_MAJOR) + "." + std::to_string(TOML_LIB_MINOR) + "." +
                                           std::to_string(TOML_LIB_PATCH);
        return version;
    }

}  // namespace typeit::infra
