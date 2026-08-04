#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <optional>
#include <string>

#include "typeit/core/util/Result.h"
#include "typeit/infra/fs/PlatformPaths.h"
#include "typeit/testing/TempEnv.h"

namespace typeit::testing {
    namespace {

        TEST(TempEnvTest, CreatesItsDirectoriesAndPointsTheEnvironmentAtThem) {
            const TempEnv env;

            EXPECT_TRUE(std::filesystem::is_directory(env.config()));
            EXPECT_TRUE(std::filesystem::is_directory(env.data()));
            EXPECT_EQ(read_environment("TYPEIT_CONFIG_DIR"), env.config().string());
            EXPECT_EQ(read_environment("TYPEIT_DATA_DIR"), env.data().string());
        }

        TEST(TempEnvTest, TheApplicationResolvesToTheTemporaryDirectories) {
            // The whole point, end to end: code that asks the real environment
            // where to put things is answered with the fixture's directory.
            const TempEnv env;

            const core::Result<infra::Paths> paths = infra::resolve_paths(infra::system_environment());

            ASSERT_TRUE(paths) << (paths ? "" : paths.error().context);
            EXPECT_EQ(paths->config, env.config());
            EXPECT_EQ(paths->data, env.data());
            EXPECT_EQ(paths->cache, env.data() / "cache");
        }

        TEST(TempEnvTest, RemovesItsDirectoryAndRestoresTheEnvironment) {
            const std::optional<std::string> before = read_environment("TYPEIT_CONFIG_DIR");
            std::filesystem::path root;
            {
                const TempEnv env;
                root = env.root();
                std::ofstream{env.data() / "typeit.db"} << "pretend history";
                ASSERT_TRUE(std::filesystem::exists(root));
            }

            EXPECT_FALSE(std::filesystem::exists(root)) << "including the files a test left in it";
            EXPECT_EQ(read_environment("TYPEIT_CONFIG_DIR"), before);
        }

        TEST(TempEnvTest, RestoringUnsetIsNotTheSameAsRestoringEmpty) {
            // A variable that was never set must end up unset, not set to "".
            // An empty TYPEIT_CONFIG_DIR would be ignored by resolve_paths, so
            // the difference is invisible until something else reads it.
            ASSERT_FALSE(read_environment("TYPEIT_CONFIG_DIR").has_value())
                    << "this test must run outside any other fixture";

            { const TempEnv env; }

            EXPECT_FALSE(read_environment("TYPEIT_CONFIG_DIR").has_value());
        }

        TEST(TempEnvTest, TwoFixturesInOneProcessDoNotCollide) {
            const TempEnv first;
            const TempEnv second;

            EXPECT_NE(first.root(), second.root());
            EXPECT_TRUE(std::filesystem::is_directory(first.data()));
            EXPECT_TRUE(std::filesystem::is_directory(second.data()));
            EXPECT_EQ(read_environment("TYPEIT_DATA_DIR"), second.data().string()) << "the inner one wins while alive";
        }

        TEST(TempEnvTest, TheOuterFixtureIsRestoredWhenTheInnerOneGoes) {
            const TempEnv outer;
            {
                const TempEnv inner;
                ASSERT_EQ(read_environment("TYPEIT_DATA_DIR"), inner.data().string());
            }

            EXPECT_EQ(read_environment("TYPEIT_DATA_DIR"), outer.data().string());
        }

        TEST(TempEnvTest, ItSurvivesADirectoryThatWasAlreadyRemoved) {
            // Destructing over a directory somebody else deleted must not throw
            // out of a destructor and take a passing test with it.
            const TempEnv env;
            std::filesystem::remove_all(env.root());

            SUCCEED() << "the destructor runs at the end of this test";
        }

    }  // namespace
}  // namespace typeit::testing
