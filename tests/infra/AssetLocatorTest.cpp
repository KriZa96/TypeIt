#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/core/util/Result.h"
#include "typeit/infra/fs/AssetLocator.h"
#include "typeit/infra/fs/PlatformPaths.h"
#include "typeit/testing/TempEnv.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;

#ifdef _WIN32
        constexpr char kListSeparator = ';';
#else
        constexpr char kListSeparator = ':';
#endif

        Environment environment_of(std::map<std::string, std::string, std::less<>> values) {
            return [values = std::move(values)](std::string_view name) -> std::optional<std::string> {
                const auto found = values.find(name);
                if (found == values.end()) {
                    return std::nullopt;
                }
                return found->second;
            };
        }

        /// A directory that looks like somewhere assets live: it exists and it
        /// has a text in it, because "exists" is what the locator checks and a
        /// reader wants to see why.
        void make_assets(const std::filesystem::path& directory) {
            std::filesystem::create_directories(directory / "texts");
            std::ofstream{directory / "texts" / "simple.txt"} << "the quick brown fox\n";
        }

        /// An install tree: `<prefix>/bin/typeit` and `<prefix>/share/typeit`.
        std::filesystem::path make_install_tree(const std::filesystem::path& prefix) {
            std::filesystem::create_directories(prefix / "bin");
            const std::filesystem::path binary = prefix / "bin" / "typeit";
            std::ofstream{binary} << "#!/bin/sh\n";
            std::filesystem::permissions(binary, std::filesystem::perms::owner_all, std::filesystem::perm_options::add);
            make_assets(prefix / "share" / "typeit");
            return binary;
        }

        class AssetLocatorTest : public ::testing::Test {
        protected:
            [[nodiscard]] std::filesystem::path root() const { return env_.root(); }

            testing::TempEnv env_;
        };

        TEST_F(AssetLocatorTest, TheSearchPathIsTheDocumentedOrder) {
            const AssetSearch search{
                    .environment = environment_of({{"TYPEIT_ASSETS_DIR", (root() / "override").string()},
                                                   {"XDG_DATA_DIRS", (root() / "xdg").string()}}),
                    .executable = root() / "prefix" / "bin" / "typeit",
                    .working_directory = root() / "cwd",
                    .install_prefix = root() / "configured",
            };

            const std::vector<std::filesystem::path> candidates = asset_search_path(search);

            ASSERT_EQ(candidates.size(), 5U);
            EXPECT_EQ(candidates[0], root() / "override");
            EXPECT_EQ(candidates[1], root() / "prefix" / "share" / "typeit");
            EXPECT_EQ(candidates[2], root() / "configured" / "share" / "typeit");
            EXPECT_EQ(candidates[3], root() / "xdg" / "typeit");
            EXPECT_EQ(candidates[4], root() / "cwd" / "assets");
        }

        TEST_F(AssetLocatorTest, TheFirstHitWins) {
            make_assets(root() / "override");
            make_assets(root() / "prefix" / "share" / "typeit");

            const Result<std::filesystem::path> found = locate_assets({
                    .environment = environment_of({{"TYPEIT_ASSETS_DIR", (root() / "override").string()}}),
                    .executable = root() / "prefix" / "bin" / "typeit",
                    .working_directory = {},
                    .install_prefix = {},
            });

            ASSERT_TRUE(found) << (found ? "" : found.error().context);
            EXPECT_EQ(*found, root() / "override");
        }

        TEST_F(AssetLocatorTest, AnOverrideThatDoesNotExistFallsThroughToTheNextEntry) {
            // Pointing the variable at a typo should not disable the assets
            // that are there.
            make_assets(root() / "prefix" / "share" / "typeit");

            const Result<std::filesystem::path> found = locate_assets({
                    .environment = environment_of({{"TYPEIT_ASSETS_DIR", (root() / "typo").string()}}),
                    .executable = root() / "prefix" / "bin" / "typeit",
                    .working_directory = {},
                    .install_prefix = {},
            });

            ASSERT_TRUE(found) << (found ? "" : found.error().context);
            EXPECT_EQ(*found, root() / "prefix" / "share" / "typeit");
        }

        TEST_F(AssetLocatorTest, ResolvesFromAnInstallTreeInATemporaryDirectory) {
            // The C2 case in miniature: a binary that has never heard of the
            // source directory finds its assets by looking beside itself.
            const std::filesystem::path binary = make_install_tree(root() / "opt");

            const Result<std::filesystem::path> found = locate_assets({
                    .environment = environment_of({}),
                    .executable = binary,
                    .working_directory = {},
                    .install_prefix = {},
            });

            ASSERT_TRUE(found) << (found ? "" : found.error().context);
            EXPECT_EQ(*found, root() / "opt" / "share" / "typeit");
            EXPECT_TRUE(std::filesystem::exists(*found / "texts" / "simple.txt"));
        }

        TEST_F(AssetLocatorTest, TheInstallPrefixIsTriedWhenTheBinaryIsElsewhere) {
            make_assets(root() / "usr" / "share" / "typeit");

            const Result<std::filesystem::path> found = locate_assets({
                    .environment = environment_of({}),
                    .executable = root() / "somewhere" / "bin" / "typeit",
                    .working_directory = {},
                    .install_prefix = root() / "usr",
            });

            ASSERT_TRUE(found) << (found ? "" : found.error().context);
            EXPECT_EQ(*found, root() / "usr" / "share" / "typeit");
        }

        TEST_F(AssetLocatorTest, EveryXdgDataDirIsTriedInOrder) {
            make_assets(root() / "second" / "typeit");

            const Result<std::filesystem::path> found = locate_assets({
                    .environment = environment_of(
                            {{"XDG_DATA_DIRS", (root() / "first").string() + std::string(1, kListSeparator) +
                                                       (root() / "second").string()}}),
                    .executable = {},
                    .working_directory = {},
                    .install_prefix = {},
            });

            ASSERT_TRUE(found) << (found ? "" : found.error().context);
            EXPECT_EQ(*found, root() / "second" / "typeit");
        }

        TEST_F(AssetLocatorTest, TheDevelopmentDirectoryIsLast) {
            make_assets(root() / "cwd" / "assets");

            const Result<std::filesystem::path> found = locate_assets({
                    .environment = environment_of({}),
                    .executable = {},
                    .working_directory = root() / "cwd",
                    .install_prefix = {},
            });

            ASSERT_TRUE(found) << (found ? "" : found.error().context);
            EXPECT_EQ(*found, root() / "cwd" / "assets");
        }

        TEST_F(AssetLocatorTest, NothingFoundIsAClearErrorThatListsWhereItLooked) {
            // Not fatal: the bundled corpora are a convenience, and a user with
            // imported texts of their own loses nothing. But "assets not found"
            // with no list is a message nobody can act on.
            const Result<std::filesystem::path> found = locate_assets({
                    .environment = environment_of({{"XDG_DATA_DIRS", (root() / "nowhere").string()}}),
                    .executable = root() / "opt" / "bin" / "typeit",
                    .working_directory = root() / "cwd",
                    .install_prefix = root() / "usr",
            });

            ASSERT_FALSE(found);
            EXPECT_EQ(found.error().code, ErrorCode::FileNotFound);
            EXPECT_NE(found.error().context.find((root() / "opt" / "share" / "typeit").string()), std::string::npos)
                    << found.error().context;
            EXPECT_NE(found.error().context.find((root() / "cwd" / "assets").string()), std::string::npos)
                    << found.error().context;
        }

#ifndef _WIN32
        TEST_F(AssetLocatorTest, ASymlinkedExecutableResolvesToTheRealFile) {
            // The `/usr/games/TypeIt -> …/build/TypeIt` case from the README.
            // Looking beside the symlink finds nothing; looking beside the real
            // binary finds the install.
            const std::filesystem::path binary = make_install_tree(root() / "opt");
            std::filesystem::create_directories(root() / "usr" / "games");
            const std::filesystem::path link = root() / "usr" / "games" / "TypeIt";
            std::filesystem::create_symlink(binary, link);

            const Result<std::filesystem::path> beside_the_link = locate_assets({
                    .environment = environment_of({}),
                    .executable = link,
                    .working_directory = {},
                    .install_prefix = {},
            });
            EXPECT_FALSE(beside_the_link) << "which is why the executable must be canonicalised first";

            const Result<std::filesystem::path> beside_the_real_file = locate_assets({
                    .environment = environment_of({}),
                    .executable = std::filesystem::canonical(link),
                    .working_directory = {},
                    .install_prefix = {},
            });
            ASSERT_TRUE(beside_the_real_file) << (beside_the_real_file ? "" : beside_the_real_file.error().context);
            // Compared canonical to canonical: on macOS /var is itself a
            // symlink to /private/var, so the two spellings of the same
            // directory differ. The assertion is about which directory, not
            // about how it is spelled.
            EXPECT_EQ(std::filesystem::canonical(*beside_the_real_file),
                      std::filesystem::canonical(root() / "opt" / "share" / "typeit"));
        }
#endif

        TEST(CurrentExecutableTest, ResolvesToTheRunningTestBinary) {
            const Result<std::filesystem::path> self = current_executable();

            ASSERT_TRUE(self) << (self ? "" : self.error().context);
            EXPECT_TRUE(self->is_absolute()) << self->string();
            EXPECT_TRUE(std::filesystem::exists(*self)) << self->string();
            EXPECT_NE(self->filename().string().find("infra_tests"), std::string::npos) << self->string();
        }

        TEST(CurrentExecutableTest, IsAlreadyCanonical) {
            const Result<std::filesystem::path> self = current_executable();

            ASSERT_TRUE(self);
            EXPECT_EQ(*self, std::filesystem::canonical(*self));
        }

        TEST(ConfiguredInstallPrefixTest, IsWhateverThisBuildWasConfiguredWith) {
            // Baked in at configure time, so it is a real path rather than a
            // placeholder — the entry that serves a package installed to a
            // prefix the binary is not run from.
            EXPECT_FALSE(configured_install_prefix().empty());
            EXPECT_TRUE(configured_install_prefix().is_absolute()) << configured_install_prefix().string();
        }

    }  // namespace
}  // namespace typeit::infra
