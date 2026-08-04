#include <cctype>
#include <gtest/gtest.h>
#include <string_view>

#include "typeit/infra/Dependencies.h"

namespace typeit::infra {
    namespace {

        /// `major.minor.patch`, and nothing else. Not a regex: a version string
        /// is three numbers and two dots, and saying so in code is shorter than
        /// saying so in a pattern.
        bool looks_like_a_version(std::string_view version) {
            int digits = 0;
            int dots = 0;
            for (const char character: version) {
                if (std::isdigit(static_cast<unsigned char>(character)) != 0) {
                    ++digits;
                } else if (character == '.') {
                    ++dots;
                } else {
                    return false;
                }
            }
            return dots == 2 && digits >= 3;
        }

        TEST(DependenciesTest, SqliteReportsTheVersionItWasLinkedAgainst) {
            const std::string_view version = sqlite_version();

            EXPECT_FALSE(version.empty());
            EXPECT_TRUE(looks_like_a_version(version)) << version;
            EXPECT_TRUE(version.starts_with("3.")) << "SQLite 4 would be a different library: " << version;
        }

        TEST(DependenciesTest, TomlReportsTheVersionItWasCompiledAgainst) {
            const std::string_view version = toml_version();

            EXPECT_TRUE(looks_like_a_version(version)) << version;
            EXPECT_TRUE(version.starts_with("3.")) << version;
        }

        TEST(DependenciesTest, TheVersionsAreStableAcrossCalls) {
            // They are used in bug reports and in `--doctor`; a value that
            // differed between two calls would be worse than no value.
            EXPECT_EQ(sqlite_version(), sqlite_version());
            EXPECT_EQ(toml_version(), toml_version());
        }

    }  // namespace
}  // namespace typeit::infra
