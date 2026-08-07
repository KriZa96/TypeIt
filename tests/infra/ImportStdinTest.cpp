// Reading a text from a pipe, and getting the keyboard back (TI-111).
//
// The reading half is tested over a `std::istringstream`, which is a pipe as
// far as `read_stream` can tell and is a pipe a test can arrange. The
// reattaching half is tested against devices that do and do not exist, because
// a test that could only use the real terminal would pass or fail on how CI
// happens to run it — which is the same as not testing it.

#include <cstdio>
#include <filesystem>
#include <gtest/gtest.h>
#include <sstream>
#include <string>

#include "typeit/core/util/Result.h"
#include "typeit/infra/term/StandardInput.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;

        TEST(ImportStdinTest, PipedTextIsReadWhole) {
            std::istringstream input{"the quick brown fox"};

            const Result<std::string> text = read_stream(input);

            ASSERT_TRUE(text) << (text ? "" : text.error().context);
            EXPECT_EQ(*text, "the quick brown fox");
        }

        TEST(ImportStdinTest, AnEmptyStreamIsEmptyRatherThanAnError) {
            // `rdbuf()` sets `failbit` when there was nothing to insert, which
            // is not a failure — and "the pipe was empty" is a case the
            // importer reports far better than this can, with a message about
            // there being nothing to type.
            std::istringstream input{""};

            const Result<std::string> text = read_stream(input);

            ASSERT_TRUE(text) << (text ? "" : text.error().context);
            EXPECT_TRUE(text->empty());
        }

        TEST(ImportStdinTest, BinaryDataComesThroughWholeRatherThanPartiallyConsumed) {
            // Whether bytes are text is the importer's question, and it already
            // answers it with an offset. A reader that validated UTF-8 would be
            // a second opinion that could disagree — and stopping at the first
            // bad byte would hand over a truncated file that looks fine.
            const std::string binary{"\x00\x01\xFF\xFE ok", 7};
            std::istringstream input{binary};

            const Result<std::string> text = read_stream(input);

            ASSERT_TRUE(text);
            EXPECT_EQ(text->size(), binary.size()) << "every byte, including the NUL";
            EXPECT_EQ(*text, binary);
        }

        TEST(ImportStdinTest, ALargeStreamIsReadWithoutTruncation) {
            // Two megabytes, which is inside the import limit and well past any
            // buffer this would be tempted to read in one go.
            const std::string line = "the quick brown fox jumps over the lazy dog\n";
            std::string big;
            while (big.size() < 2U * 1024U * 1024U) {
                big += line;
            }
            std::istringstream input{big};

            const Result<std::string> text = read_stream(input);

            ASSERT_TRUE(text);
            EXPECT_EQ(text->size(), big.size());
            EXPECT_TRUE(text->ends_with("lazy dog\n"));
        }

        TEST(ImportStdinTest, ABrokenReadIsAnErrorRatherThanAShortOne) {
            // Half an article silently imported as a whole one is the failure
            // nobody notices, so `badbit` is reported.
            std::istringstream input{"some text"};
            input.setstate(std::ios::badbit);

            const Result<std::string> text = read_stream(input);

            ASSERT_FALSE(text);
            EXPECT_EQ(text.error().code, ErrorCode::FileUnreadable);
        }

        TEST(ImportStdinTest, TheControllingTerminalIsNamedPerPlatform) {
            const std::filesystem::path device = controlling_terminal();

#ifdef _WIN32
            EXPECT_EQ(device, "CONIN$");
#else
            EXPECT_EQ(device, "/dev/tty");
#endif
        }

        TEST(ImportStdinTest, ReattachingToADeviceThatIsNotThereIsReportedRatherThanFatal) {
            // A machine with no controlling terminal — a service, a build step,
            // a container without a tty — is a normal thing to be, and the
            // answer there is a message rather than a crash.
            const core::Status reattached = reattach_input("/definitely/not/a/terminal");

            ASSERT_FALSE(reattached);
            EXPECT_EQ(reattached.error().code, ErrorCode::UnsupportedTerminal);
            EXPECT_NE(reattached.error().context.find("terminal"), std::string::npos)
                    << reattached.error().context;
        }

        TEST(ImportStdinTest, ReattachingPointsStdinAtTheDeviceItWasGiven) {
            // The whole point, asserted without needing a terminal: after the
            // call, reading `stdin` reads the *new* device rather than whatever
            // it held before. The test writes a file, reattaches to it, and
            // reads a byte back.
            const std::filesystem::path file =
                    std::filesystem::temp_directory_path() / "typeit-stdin-reattach.txt";
            {
                std::FILE* written = std::fopen(file.string().c_str(), "w");
                ASSERT_NE(written, nullptr);
                ASSERT_GT(std::fputs("k", written), 0);
                ASSERT_EQ(std::fclose(written), 0);
            }

            const core::Status reattached = reattach_input(file);

            ASSERT_TRUE(reattached) << reattached.error().context;
            EXPECT_EQ(std::fgetc(stdin), 'k') << "stdin now reads the device it was pointed at";

            // Put stdin back on something harmless before leaving. The
            // aggregate `all_tests` target runs every case in one process, so a
            // stdin left pointing at a file this test then deletes would be a
            // failure attributed to whichever case ran next.
#ifdef _WIN32
            static_cast<void>(reattach_input(std::filesystem::path{"NUL"}));
#else
            static_cast<void>(reattach_input(std::filesystem::path{"/dev/null"}));
#endif
            std::filesystem::remove(file);
        }

    }  // namespace
}  // namespace typeit::infra
