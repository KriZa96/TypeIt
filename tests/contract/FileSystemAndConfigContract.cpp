// One suite each for IFileSystem and IConfigStore, over every implementation.

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "typeit/app/ports/IConfigStore.h"
#include "typeit/app/ports/IFileSystem.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/util/Result.h"
#include "typeit/infra/config/TomlConfigStore.h"
#include "typeit/infra/fs/StdFileSystem.h"
#include "typeit/testing/Fakes.h"
#include "typeit/testing/TempEnv.h"

namespace typeit {
    namespace {

        // ---- IFileSystem ---------------------------------------------------

        /// The real one, over a temporary directory the fixture owns.
        class StdFileSystemFactory {
        public:
            app::IFileSystem& files() { return files_; }

            [[nodiscard]] std::filesystem::path root() const { return env_.data(); }

            void write(const std::filesystem::path& relative, std::string contents) {
                const std::filesystem::path path = root() / relative;
                std::filesystem::create_directories(path.parent_path());
                std::ofstream{path, std::ios::binary} << contents;
            }

            void make_directory(const std::filesystem::path& relative) {
                std::filesystem::create_directories(root() / relative);
            }

        private:
            testing::TempEnv env_;
            infra::StdFileSystem files_;
        };

        class FakeFileSystemFactory {
        public:
            app::IFileSystem& files() { return files_; }

            [[nodiscard]] static std::filesystem::path root() { return "/fake"; }

            void write(const std::filesystem::path& relative, std::string contents) {
                files_.add_file(root() / relative, std::move(contents));
            }

            void make_directory(const std::filesystem::path& relative) { files_.add_directory(root() / relative); }

        private:
            testing::FakeFileSystem files_;
        };

        template<typename FactoryType>
        class FileSystemContract : public ::testing::Test {
        protected:
            app::IFileSystem& files() { return factory_.files(); }

            FactoryType factory_;
        };

        using FileSystemImplementations = ::testing::Types<StdFileSystemFactory, FakeFileSystemFactory>;

        class FileSystemNames {
        public:
            template<typename Factory>
            static std::string GetName(int /*index*/) {
                return std::is_same_v<Factory, StdFileSystemFactory> ? "Std" : "Fake";
            }
        };

        TYPED_TEST_SUITE(FileSystemContract, FileSystemImplementations, FileSystemNames);

        TYPED_TEST(FileSystemContract, ReadsWhatWasWritten) {
            this->factory_.write("prose.txt", "the quick brown fox");

            const core::Result<std::string> contents = this->files().read_text(this->factory_.root() / "prose.txt");

            ASSERT_TRUE(contents) << (contents ? "" : contents.error().context);
            EXPECT_EQ(*contents, "the quick brown fox");
        }

        TYPED_TEST(FileSystemContract, AnEmptyFileIsAnEmptyString) {
            this->factory_.write("empty.txt", "");

            const core::Result<std::string> contents = this->files().read_text(this->factory_.root() / "empty.txt");

            ASSERT_TRUE(contents) << (contents ? "" : contents.error().context);
            EXPECT_TRUE(contents->empty()) << "not an error, and not an underflow";
        }

        TYPED_TEST(FileSystemContract, AMissingFileIsFileNotFound) {
            const core::Result<std::string> contents = this->files().read_text(this->factory_.root() / "absent.txt");

            ASSERT_FALSE(contents);
            EXPECT_EQ(contents.error().code, core::ErrorCode::FileNotFound);
        }

        TYPED_TEST(FileSystemContract, ADirectoryIsUnreadableRatherThanMissing) {
            this->factory_.make_directory("corpus");

            const core::Result<std::string> contents = this->files().read_text(this->factory_.root() / "corpus");

            ASSERT_FALSE(contents);
            EXPECT_EQ(contents.error().code, core::ErrorCode::FileUnreadable)
                    << "a different problem, a different code";
        }

        TYPED_TEST(FileSystemContract, ExistsAndIsDirectoryAgree) {
            this->factory_.write("prose.txt", "x");
            this->factory_.make_directory("corpus");

            EXPECT_TRUE(this->files().exists(this->factory_.root() / "prose.txt"));
            EXPECT_FALSE(this->files().is_directory(this->factory_.root() / "prose.txt"));
            EXPECT_TRUE(this->files().is_directory(this->factory_.root() / "corpus"));
            EXPECT_FALSE(this->files().exists(this->factory_.root() / "nothing"));
        }

        TYPED_TEST(FileSystemContract, ListingIsSorted) {
            this->factory_.write(std::filesystem::path{"corpus"} / "zebra.txt", "x");
            this->factory_.write(std::filesystem::path{"corpus"} / "apple.txt", "x");
            this->factory_.write(std::filesystem::path{"corpus"} / "middle.txt", "x");

            const core::Result<std::vector<std::filesystem::path>> entries =
                    this->files().list(this->factory_.root() / "corpus");

            ASSERT_TRUE(entries) << (entries ? "" : entries.error().context);
            ASSERT_EQ(entries->size(), 3U);
            EXPECT_EQ(entries->at(0).filename(), "apple.txt");
            EXPECT_EQ(entries->at(1).filename(), "middle.txt");
            EXPECT_EQ(entries->at(2).filename(), "zebra.txt");
        }

        TYPED_TEST(FileSystemContract, ListingSomethingThatIsNotADirectoryIsAnError) {
            this->factory_.write("prose.txt", "x");

            const core::Result<std::vector<std::filesystem::path>> entries =
                    this->files().list(this->factory_.root() / "prose.txt");

            EXPECT_FALSE(entries);
        }

        // ---- IConfigStore --------------------------------------------------

        class TomlConfigStoreFactory {
        public:
            TomlConfigStoreFactory() : store_{env_.config() / "config.toml"} {}

            app::IConfigStore& store() { return store_; }

        private:
            testing::TempEnv env_;
            infra::TomlConfigStore store_;
        };

        class FakeConfigStoreFactory {
        public:
            app::IConfigStore& store() { return store_; }

        private:
            testing::FakeConfigStore store_;
        };

        template<typename FactoryType>
        class ConfigStoreContract : public ::testing::Test {
        protected:
            app::IConfigStore& store() { return factory_.store(); }

            FactoryType factory_;
        };

        using ConfigStoreImplementations = ::testing::Types<TomlConfigStoreFactory, FakeConfigStoreFactory>;

        class ConfigStoreNames {
        public:
            template<typename Factory>
            static std::string GetName(int /*index*/) {
                return std::is_same_v<Factory, TomlConfigStoreFactory> ? "Toml" : "Fake";
            }
        };

        TYPED_TEST_SUITE(ConfigStoreContract, ConfigStoreImplementations, ConfigStoreNames);

        TYPED_TEST(ConfigStoreContract, LoadingWithNothingSavedGivesTheDefaults) {
            const core::Result<app::LoadedConfig> loaded = this->store().load();

            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_EQ(loaded->config.general.default_duration_s, core::Config{}.general.default_duration_s);
            EXPECT_TRUE(loaded->warnings.empty()) << "a first run is not a complaint";
        }

        TYPED_TEST(ConfigStoreContract, SaveThenLoadRoundTrips) {
            core::Config config;
            config.general.default_mode = "words";
            config.general.default_duration_s = 45;
            config.typing.stop_on_error = core::StopOnError::Letter;
            config.appearance.line_width = 72;

            ASSERT_TRUE(this->store().save(config));
            const core::Result<app::LoadedConfig> loaded = this->store().load();

            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_EQ(loaded->config.general.default_mode, "words");
            EXPECT_EQ(loaded->config.general.default_duration_s, 45);
            EXPECT_EQ(loaded->config.typing.stop_on_error, core::StopOnError::Letter);
            EXPECT_EQ(loaded->config.appearance.line_width, 72);
        }

        TYPED_TEST(ConfigStoreContract, TheLastSaveWins) {
            core::Config first;
            first.general.default_duration_s = 15;
            core::Config second;
            second.general.default_duration_s = 60;

            ASSERT_TRUE(this->store().save(first));
            ASSERT_TRUE(this->store().save(second));

            const core::Result<app::LoadedConfig> loaded = this->store().load();
            ASSERT_TRUE(loaded);
            EXPECT_EQ(loaded->config.general.default_duration_s, 60);
        }

        TYPED_TEST(ConfigStoreContract, NonAsciiSurvives) {
            core::Config config;
            config.appearance.theme = "typeit-čšž-漢字";

            ASSERT_TRUE(this->store().save(config));

            const core::Result<app::LoadedConfig> loaded = this->store().load();
            ASSERT_TRUE(loaded) << (loaded ? "" : loaded.error().context);
            EXPECT_EQ(loaded->config.appearance.theme, "typeit-čšž-漢字");
        }

    }  // namespace
}  // namespace typeit
