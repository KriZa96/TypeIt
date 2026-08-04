#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/infra/fs/StdFileSystem.h"
#include "typeit/infra/time/SystemClock.h"
#include "typeit/testing/TempEnv.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;

        class FileSystemTest : public ::testing::Test {
        protected:
            [[nodiscard]] std::filesystem::path root() const { return env_.data(); }

            std::filesystem::path write(const std::filesystem::path& name, std::string_view contents) const {
                const std::filesystem::path path = root() / name;
                std::filesystem::create_directories(path.parent_path());
                std::ofstream{path, std::ios::binary} << contents;
                return path;
            }

            testing::TempEnv env_;
            StdFileSystem files_;
        };

        // ---- reading -------------------------------------------------------

        TEST_F(FileSystemTest, ReadsAFileWhole) {
            const std::filesystem::path path = write("prose.txt", "the quick brown fox\nand a second line\n");

            const Result<std::string> contents = files_.read_text(path);

            ASSERT_TRUE(contents) << (contents ? "" : contents.error().context);
            EXPECT_EQ(*contents, "the quick brown fox\nand a second line\n");
        }

        TEST_F(FileSystemTest, AZeroByteFileIsAnEmptyStringNotAnError) {
            // Defect C1's shape: 1.0 reads past the end of an empty file.
            const std::filesystem::path path = write("empty.txt", "");

            const Result<std::string> contents = files_.read_text(path);

            ASSERT_TRUE(contents) << (contents ? "" : contents.error().context);
            EXPECT_TRUE(contents->empty());
        }

        TEST_F(FileSystemTest, AFileOfOnlyWhitespaceIsReadVerbatim) {
            const std::filesystem::path path = write("blank.txt", "   \n\t\n");

            const Result<std::string> contents = files_.read_text(path);

            ASSERT_TRUE(contents);
            EXPECT_EQ(*contents, "   \n\t\n") << "trimming is a decision for a layer that knows what the text is for";
        }

        TEST_F(FileSystemTest, AMissingFileAndADirectoryAreDifferentErrors) {
            // "Could not read the file" tells a user nothing they can act on.
            const Result<std::string> missing = files_.read_text(root() / "not-here.txt");
            const Result<std::string> directory = files_.read_text(root());

            ASSERT_FALSE(missing);
            EXPECT_EQ(missing.error().code, ErrorCode::FileNotFound);
            EXPECT_NE(missing.error().context.find("not-here.txt"), std::string::npos);

            ASSERT_FALSE(directory);
            EXPECT_EQ(directory.error().code, ErrorCode::FileUnreadable);
            EXPECT_NE(directory.error().context.find("is a directory"), std::string::npos) << directory.error().context;
        }

#ifndef _WIN32
        TEST_F(FileSystemTest, APermissionDeniedFileIsItsOwnError) {
            const std::filesystem::path path = write("secret.txt", "not for you");
            std::filesystem::permissions(path, std::filesystem::perms::none);

            const Result<std::string> contents = files_.read_text(path);

            std::filesystem::permissions(path, std::filesystem::perms::owner_all);

            ASSERT_FALSE(contents);
            EXPECT_EQ(contents.error().code, ErrorCode::FileUnreadable);
            EXPECT_NE(contents.error().context.find("cannot be opened"), std::string::npos) << contents.error().context;
        }
#endif

        TEST_F(FileSystemTest, BinaryContentSurvivesIncludingNulBytes) {
            // A text file is not always text. Truncating at the first NUL would
            // silently import half a file.
            const std::string bytes{"before\0after", 12};
            const std::filesystem::path path = write("binary.dat", bytes);

            const Result<std::string> contents = files_.read_text(path);

            ASSERT_TRUE(contents);
            EXPECT_EQ(contents->size(), 12U);
            EXPECT_EQ(*contents, bytes);
        }

        TEST_F(FileSystemTest, ANonAsciiPathWorks) {
            const std::filesystem::path path = write("čšž 漢字 😀.txt", "found me");

            const Result<std::string> contents = files_.read_text(path);

            ASSERT_TRUE(contents) << (contents ? "" : contents.error().context);
            EXPECT_EQ(*contents, "found me");
        }

        // ---- asking --------------------------------------------------------

        TEST_F(FileSystemTest, ExistsAndIsDirectoryAnswerWithoutThrowing) {
            const std::filesystem::path file = write("prose.txt", "x");

            EXPECT_TRUE(files_.exists(file));
            EXPECT_FALSE(files_.is_directory(file));
            EXPECT_TRUE(files_.exists(root()));
            EXPECT_TRUE(files_.is_directory(root()));
            EXPECT_FALSE(files_.exists(root() / "nothing"));
            EXPECT_FALSE(files_.is_directory(root() / "nothing"));
        }

        // ---- listing -------------------------------------------------------

        TEST_F(FileSystemTest, ListReturnsADeterministicOrder) {
            // Directory order is whatever the filesystem feels like. An import
            // that processes files differently on every machine is not
            // reproducible.
            for (const char* name: {"zebra.txt", "apple.txt", "middle.txt"}) {
                write(std::filesystem::path{"corpus"} / name, "x");
            }

            const Result<std::vector<std::filesystem::path>> entries = files_.list(root() / "corpus");

            ASSERT_TRUE(entries) << (entries ? "" : entries.error().context);
            ASSERT_EQ(entries->size(), 3U);
            EXPECT_EQ(entries->at(0).filename(), "apple.txt");
            EXPECT_EQ(entries->at(1).filename(), "middle.txt");
            EXPECT_EQ(entries->at(2).filename(), "zebra.txt");
        }

        TEST_F(FileSystemTest, ListIncludesSubdirectoriesAndStopsThere) {
            write(std::filesystem::path{"corpus"} / "one.txt", "x");
            std::filesystem::create_directories(root() / "corpus" / "nested" / "deeper");

            const Result<std::vector<std::filesystem::path>> entries = files_.list(root() / "corpus");

            ASSERT_TRUE(entries);
            EXPECT_EQ(entries->size(), 2U) << "the immediate children, not the tree";
        }

        TEST_F(FileSystemTest, ListingAnEmptyDirectoryIsAnEmptyList) {
            std::filesystem::create_directories(root() / "empty");

            const Result<std::vector<std::filesystem::path>> entries = files_.list(root() / "empty");

            ASSERT_TRUE(entries) << (entries ? "" : entries.error().context);
            EXPECT_TRUE(entries->empty());
        }

        TEST_F(FileSystemTest, ListingSomethingThatIsNotADirectoryIsAnError) {
            const std::filesystem::path file = write("prose.txt", "x");

            const Result<std::vector<std::filesystem::path>> entries = files_.list(file);

            ASSERT_FALSE(entries);
            EXPECT_EQ(entries.error().code, ErrorCode::FileNotFound);
        }

        // ---- the clock -----------------------------------------------------

        TEST(SystemClockTest, NeverGoesBackwards) {
            // Monotonic, not wall-clock: a run measured against the system
            // clock changes length when NTP corrects the machine, and a typist
            // who gains 40 WPM at 02:00 on the last Sunday of October has not
            // improved.
            const SystemClock clock;
            core::Millis previous = clock.now();

            for (int i = 0; i < 1'000; ++i) {
                const core::Millis current = clock.now();
                ASSERT_GE(current, previous) << "reading " << i;
                previous = current;
            }
        }

        TEST(SystemClockTest, MeasuresElapsedTimeAcrossRealWork) {
            const SystemClock clock;
            const core::Millis start = clock.now();

            // Enough work to take a measurable moment without sleeping: this
            // suite has no sleeps in it and is not starting now.
            std::string busy;
            for (int i = 0; i < 200'000; ++i) {
                busy += 'x';
            }

            EXPECT_GE(clock.now(), start);
            EXPECT_EQ(busy.size(), 200'000U);
        }

        TEST(SystemClockTest, UnixNowIsARealDate) {
            // The other clock, for the things that genuinely are dates. Kept
            // apart so nobody reaches for a wall clock to measure a duration.
            constexpr core::Millis kFirstOfJanuary2020{1'577'836'800'000};

            EXPECT_GT(unix_now(), kFirstOfJanuary2020) << "the machine's date is at least plausible";
        }

    }  // namespace
}  // namespace typeit::infra
