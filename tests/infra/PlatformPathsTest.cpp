#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <gtest/gtest.h>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "typeit/core/util/Result.h"
#include "typeit/infra/fs/PlatformPaths.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;
        using core::Status;

        /// An environment made of nothing but what a test says. Never the real
        /// one: a test that sets a real variable races every other test in the
        /// process, and on Windows would need a different call to do it.
        Environment environment_of(std::map<std::string, std::string, std::less<>> values) {
            return [values = std::move(values)](std::string_view name) -> std::optional<std::string> {
                const auto found = values.find(name);
                if (found == values.end()) {
                    return std::nullopt;
                }
                return found->second;
            };
        }

#ifdef _WIN32
        constexpr std::string_view kRoot = "C:\\home\\kim";
        constexpr std::string_view kElsewhere = "C:\\elsewhere";
#else
        constexpr std::string_view kRoot = "/home/kim";
        constexpr std::string_view kElsewhere = "/elsewhere";
#endif

#ifndef _WIN32
        std::filesystem::path under(std::string_view root, std::string_view child) {
            return std::filesystem::path{root} / child;
        }

        TEST(PlatformPathsTest, XdgConfigHomeIsUsedVerbatim) {
            const Result<Paths> paths = resolve_paths(
                    environment_of({{"HOME", std::string{kRoot}},
                                    {"XDG_CONFIG_HOME", (std::filesystem::path{kRoot} / "cfg").string()}}));

            ASSERT_TRUE(paths);
            EXPECT_EQ(paths->config, under(kRoot, "cfg") / "typeit");
        }

        TEST(PlatformPathsTest, TheDefaultsAreTheOnesTheSpecificationNames) {
            const Result<Paths> paths = resolve_paths(environment_of({{"HOME", std::string{kRoot}}}));

            ASSERT_TRUE(paths);
            EXPECT_EQ(paths->config, under(kRoot, ".config") / "typeit");
            EXPECT_EQ(paths->data, under(kRoot, ".local") / "share" / "typeit");
            EXPECT_EQ(paths->cache, under(kRoot, ".cache") / "typeit");
        }

        TEST(PlatformPathsTest, ARelativeXdgValueIsIgnoredAsTheSpecificationRequires) {
            // "If an implementation encounters a relative path it must be
            // ignored." Honouring it would put a history database wherever the
            // shell happened to be.
            const Result<Paths> paths =
                    resolve_paths(environment_of({{"HOME", std::string{kRoot}}, {"XDG_DATA_HOME", "relative/share"}}));

            ASSERT_TRUE(paths);
            EXPECT_EQ(paths->data, under(kRoot, ".local") / "share" / "typeit");
        }

        TEST(PlatformPathsTest, AnEmptyXdgValueIsTheSameAsUnset) {
            // `XDG_CONFIG_HOME=` is how a shell says "I have no opinion".
            const Result<Paths> paths =
                    resolve_paths(environment_of({{"HOME", std::string{kRoot}}, {"XDG_CONFIG_HOME", ""}}));

            ASSERT_TRUE(paths);
            EXPECT_EQ(paths->config, under(kRoot, ".config") / "typeit");
        }

        TEST(PlatformPathsTest, MissingHomeIsAClearErrorRatherThanAGuess) {
            const Result<Paths> paths = resolve_paths(environment_of({}));

            ASSERT_FALSE(paths);
            EXPECT_EQ(paths.error().code, ErrorCode::FileNotFound);
            EXPECT_NE(paths.error().context.find("HOME"), std::string::npos) << paths.error().context;
        }

        TEST(PlatformPathsTest, AllThreeXdgVariablesSetNeedsNoHome) {
            const Result<Paths> paths = resolve_paths(
                    environment_of({{"XDG_CONFIG_HOME", (std::filesystem::path{kElsewhere} / "c").string()},
                                    {"XDG_DATA_HOME", (std::filesystem::path{kElsewhere} / "d").string()},
                                    {"XDG_CACHE_HOME", (std::filesystem::path{kElsewhere} / "x").string()}}));

            ASSERT_TRUE(paths) << (paths ? "" : paths.error().context);
            EXPECT_EQ(paths->data, under(kElsewhere, "d") / "typeit");
        }
#else
        TEST(PlatformPathsTest, WindowsUsesRoamingAndLocalAppData) {
            const Result<Paths> paths =
                    resolve_paths(environment_of({{"APPDATA", "C:\\Users\\kim\\AppData\\Roaming"},
                                                  {"LOCALAPPDATA", "C:\\Users\\kim\\AppData\\Local"}}));

            ASSERT_TRUE(paths) << (paths ? "" : paths.error().context);
            EXPECT_EQ(paths->config, std::filesystem::path{"C:\\Users\\kim\\AppData\\Roaming"} / "TypeIt");
            EXPECT_EQ(paths->data, std::filesystem::path{"C:\\Users\\kim\\AppData\\Local"} / "TypeIt");
            EXPECT_EQ(paths->cache, std::filesystem::path{"C:\\Users\\kim\\AppData\\Local"} / "TypeIt" / "cache");
        }

        TEST(PlatformPathsTest, MissingAppDataIsAClearErrorRatherThanAGuess) {
            const Result<Paths> paths = resolve_paths(environment_of({{"LOCALAPPDATA", "C:\\Users\\kim\\Local"}}));

            ASSERT_FALSE(paths);
            EXPECT_EQ(paths.error().code, ErrorCode::FileNotFound);
            EXPECT_NE(paths.error().context.find("APPDATA"), std::string::npos) << paths.error().context;
        }
#endif

        TEST(PlatformPathsTest, TheOverridesBeatEverythingElse) {
            // The property every hermetic fixture from TI-065 on depends on.
            const std::filesystem::path config = std::filesystem::path{kElsewhere} / "cfg";
            const std::filesystem::path data = std::filesystem::path{kElsewhere} / "dat";

            const Result<Paths> paths = resolve_paths(environment_of({
                    {"HOME", std::string{kRoot}},
                    {"APPDATA", std::string{kRoot}},
                    {"LOCALAPPDATA", std::string{kRoot}},
                    {"XDG_CONFIG_HOME", (std::filesystem::path{kRoot} / "ignored").string()},
                    {"TYPEIT_CONFIG_DIR", config.string()},
                    {"TYPEIT_DATA_DIR", data.string()},
            }));

            ASSERT_TRUE(paths) << (paths ? "" : paths.error().context);
            EXPECT_EQ(paths->config, config) << "verbatim: an override names the directory itself";
            EXPECT_EQ(paths->data, data);
            EXPECT_EQ(paths->cache, data / "cache");
        }

        TEST(PlatformPathsTest, ARelativeOverrideIsIgnoredToo) {
            const Result<Paths> paths = resolve_paths(environment_of({
                    {"HOME", std::string{kRoot}},
                    {"APPDATA", std::string{kRoot}},
                    {"LOCALAPPDATA", std::string{kRoot}},
                    {"TYPEIT_CONFIG_DIR", "./somewhere"},
            }));

            ASSERT_TRUE(paths) << (paths ? "" : paths.error().context);
            EXPECT_TRUE(paths->config.is_absolute()) << paths->config.string();
        }

        TEST(PlatformPathsTest, EveryPathIsAbsolute) {
            const Result<Paths> paths = resolve_paths(environment_of({{"HOME", std::string{kRoot}},
                                                                      {"APPDATA", std::string{kRoot}},
                                                                      {"LOCALAPPDATA", std::string{kRoot}}}));

            ASSERT_TRUE(paths) << (paths ? "" : paths.error().context);
            EXPECT_TRUE(paths->config.is_absolute());
            EXPECT_TRUE(paths->data.is_absolute());
            EXPECT_TRUE(paths->cache.is_absolute());
        }

        TEST(PlatformPathsTest, TheSystemEnvironmentReadsTheRealProcess) {
            // The one test that touches the real environment, and it only
            // reads: PATH is set in every environment this can run in.
            const Environment real = system_environment();

            EXPECT_TRUE(real("PATH").has_value() || real("Path").has_value());
            EXPECT_FALSE(real("TYPEIT_A_VARIABLE_NOBODY_HAS_SET").has_value());
        }

        // Directory creation.

        class DirectoryTest : public ::testing::Test {
        protected:
            void SetUp() override {
                root_ = std::filesystem::temp_directory_path() /
                        ("typeit-paths-" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "-" +
                         std::to_string(reinterpret_cast<std::uintptr_t>(this)));
                std::filesystem::remove_all(root_);
            }

            void TearDown() override { std::filesystem::remove_all(root_); }

            std::filesystem::path root_;
        };

        TEST_F(DirectoryTest, CreatesTheWholeChain) {
            const Status created = ensure_directory(root_ / "one" / "two" / "three");

            ASSERT_TRUE(created) << (created ? "" : created.error().context);
            EXPECT_TRUE(std::filesystem::is_directory(root_ / "one" / "two" / "three"));
        }

        TEST_F(DirectoryTest, IsIdempotent) {
            // This runs on every start; succeeding when the directory is
            // already there is the normal case, not the exception.
            ASSERT_TRUE(ensure_directory(root_ / "data"));

            EXPECT_TRUE(ensure_directory(root_ / "data"));
            EXPECT_TRUE(ensure_directory(root_ / "data"));
        }

        TEST_F(DirectoryTest, AFileWhereADirectoryShouldBeIsAnError) {
            ASSERT_TRUE(ensure_directory(root_));
            const std::filesystem::path occupied = root_ / "config";
            {
                std::ofstream file{occupied};
                file << "not a directory";
            }

            const Status created = ensure_directory(occupied);

            ASSERT_FALSE(created);
            EXPECT_NE(created.error().context.find("config"), std::string::npos) << created.error().context;
        }

    }  // namespace
}  // namespace typeit::infra
