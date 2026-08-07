// `--version` and `--help`, which are the two outputs a user sees before they
// see anything else.
//
// The version half is small on purpose: the whole rule is that the number is
// read from the generated header and never spelled anywhere, and the only way
// to test that is to compare against the header. That a *persisted run* also
// carries it is `SessionServiceTest`'s, where the record is built.

#include <cstddef>
#include <gtest/gtest.h>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/cli/Cli.h"
#include "typeit/core/Version.h"

namespace typeit::cli {
    namespace {

        TEST(VersionWiringTest, TheVersionIsReadRatherThanSpelled) {
            const std::string text = version_text();

            EXPECT_NE(text.find(kVersionString), std::string::npos) << text;
            EXPECT_TRUE(text.starts_with("typeit ")) << text;
            EXPECT_TRUE(text.ends_with("\n")) << "it is a line, and a shell expects one";
        }

        TEST(VersionWiringTest, TheVersionNamesTheBuild) {
            // Which build this is matters in a bug report: a debug build is ten
            // times slower and a WPM figure from one means nothing.
            const std::string text = version_text();

            EXPECT_TRUE(text.contains("debug") || text.contains("release")) << text;
        }

        TEST(VersionWiringTest, TheVersionCarriesWhatGitCalledThisCommit) {
            // Empty in a tarball with no history, which is why this asserts the
            // shape rather than a value: a describe, if there is one, is in
            // parentheses after the version.
            const std::string text = version_text();

            EXPECT_EQ(text.find('(') == std::string::npos, std::string_view{kGitDescribe}.empty()) << text;
        }

        TEST(VersionWiringTest, HelpListsEveryDocumentedFlag) {
            // The list is TECHNICAL section 7's, written out here rather than
            // read from the parser's own table — a test that asked the table
            // whether it agreed with itself would pass on an empty one.
            const std::string text = usage();

            for (const std::string_view flag:
                 {"--mode",   "--time",       "--words",  "--race-preset", "--text",        "--text-id", "--section",
                  "--import", "--import-dir", "--url",    "--list-texts",  "--remove-text", "--stats",   "--export",
                  "--last",   "--simulate",   "--doctor", "--config",      "--data-dir",    "--help",    "--version"}) {
                EXPECT_NE(text.find(flag), std::string::npos) << flag << " is missing from the help";
            }
        }

        TEST(VersionWiringTest, HelpShowsTheShortFormsThatExist) {
            const std::string text = usage();

            for (const std::string_view flag: {"-m, --mode", "-t, --time", "-w, --words", "-h, --help"}) {
                EXPECT_NE(text.find(flag), std::string::npos) << flag;
            }
            EXPECT_NE(text.find("-V, --version"), std::string::npos);
        }

        TEST(VersionWiringTest, HelpGroupsFlagsTheWayTheManualDoes) {
            const std::string text = usage();

            EXPECT_TRUE(text.starts_with("typeit [OPTIONS] [FILE|-]\n")) << text;
            for (const std::string_view group:
                 {"\nModes\n", "\nText\n", "\nLibrary\n", "\nHistory\n", "\nDiagnostics\n"}) {
                EXPECT_NE(text.find(group), std::string::npos) << group;
            }
            EXPECT_NE(text.find("read the text from stdin"), std::string::npos)
                    << "the positional, which is not a flag";
        }

        TEST(VersionWiringTest, EveryHelpLineHasADescription) {
            // A flag listed with nothing beside it is a flag nobody can use,
            // and generating the text is what makes this checkable at all.
            //
            // Read as words rather than columns: the padding is one space when
            // a flag runs long, so a parse that keyed on the gap would call the
            // description a second flag.
            const std::string text = usage();
            std::size_t described = 0;

            for (std::size_t at = 0; at < text.size();) {
                const std::size_t end = text.find('\n', at);
                const std::string line = text.substr(at, end - at);
                at = end == std::string::npos ? text.size() : end + 1;
                if (!line.starts_with("  ")) {
                    continue;  // A group heading, or the title.
                }

                std::istringstream words{line};
                const std::vector<std::string> tokens{std::istream_iterator<std::string>{words}, {}};
                ASSERT_FALSE(tokens.empty()) << line;

                std::size_t token = 0;
                if (tokens.at(token).starts_with('-') && tokens.at(token).ends_with(',')) {
                    ++token;  // the short form, `-m,`
                }
                ++token;  // the long form, or the bare dash
                if (token < tokens.size() && tokens.at(token).starts_with('<')) {
                    ++token;  // the value's name
                }

                EXPECT_LT(token, tokens.size()) << "no description: " << line;
                ++described;
            }

            EXPECT_EQ(described, 23U) << "every flag in the table, plus the bare dash";
        }

    }  // namespace
}  // namespace typeit::cli
