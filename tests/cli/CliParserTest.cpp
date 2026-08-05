// Every case in TECHNICAL §7, as a table row.
//
// The parser is pure, so a test is a vector of strings and an expectation —
// no subprocess, no temporary files, no terminal. That is the point of keeping
// it pure, and it is why the whole documented surface can be covered rather
// than the three flags somebody remembered.

#include <cstdint>
#include <gtest/gtest.h>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/cli/Cli.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::cli {
    namespace {

        core::Result<CliOptions> parse_of(std::initializer_list<std::string_view> arguments) {
            const std::vector<std::string_view> owned{arguments};
            return parse(owned);
        }

        /// The parsed options, or a failed assertion. Every happy-path test
        /// starts with this, so none of them repeat the unwrapping.
        CliOptions parsed(std::initializer_list<std::string_view> arguments) {
            const core::Result<CliOptions> options = parse_of(arguments);
            EXPECT_TRUE(options) << (options ? "" : options.error().context);
            return options.value_or(CliOptions{});
        }

        /// The context of the error, for a test that is about the message.
        std::string refusal(std::initializer_list<std::string_view> arguments) {
            const core::Result<CliOptions> options = parse_of(arguments);
            EXPECT_FALSE(options) << "expected a refusal";
            if (options) {
                return {};
            }
            EXPECT_EQ(options.error().code, core::ErrorCode::InvalidArgument);
            return options.error().context;
        }

        TEST(CliParserTest, NoArgumentsIsARunWithEverythingLeftToTheConfiguration) {
            const CliOptions options = parsed({});

            EXPECT_EQ(options.action, Action::Run);
            // Absent, not defaulted: a flag nobody gave must not override a
            // setting the user did give.
            EXPECT_FALSE(options.mode.has_value());
            EXPECT_FALSE(options.seconds.has_value());
            EXPECT_FALSE(options.words.has_value());
            EXPECT_FALSE(options.text_path.has_value());
            EXPECT_FALSE(options.text_from_stdin);
        }

        // --- Every flag, long and short ---------------------------------

        TEST(CliParserTest, EveryModeFlagParsesLongAndShort) {
            EXPECT_EQ(parsed({"--mode", "timed"}).mode, "timed");
            EXPECT_EQ(parsed({"-m", "zen"}).mode, "zen");
            EXPECT_EQ(parsed({"--time", "45"}).seconds, 45);
            EXPECT_EQ(parsed({"-t", "45"}).seconds, 45);
            EXPECT_EQ(parsed({"--words", "50"}).words, 50);
            EXPECT_EQ(parsed({"-w", "50"}).words, 50);
            EXPECT_EQ(parsed({"--race-preset", "brutal"}).race_preset, "brutal");
        }

        TEST(CliParserTest, EveryTextFlagParses) {
            EXPECT_EQ(parsed({"--text", "/tmp/a.txt"}).text_path, "/tmp/a.txt");
            EXPECT_EQ(parsed({"--text-id", "7"}).text_id, core::TextId{7});
            EXPECT_EQ(parsed({"--section", "3"}).section, 3);
        }

        TEST(CliParserTest, EveryLibraryFlagParsesAndChoosesItsAction) {
            const CliOptions import = parsed({"--import", "/tmp/book.txt"});
            EXPECT_EQ(import.action, Action::Import);
            EXPECT_EQ(import.operand, "/tmp/book.txt");

            const CliOptions directory = parsed({"--import-dir", "/tmp/books"});
            EXPECT_EQ(directory.action, Action::ImportDirectory);
            EXPECT_EQ(directory.operand, "/tmp/books");

            const CliOptions url = parsed({"--url", "https://example.com/a"});
            EXPECT_EQ(url.action, Action::ImportUrl);
            EXPECT_EQ(url.operand, "https://example.com/a");

            EXPECT_EQ(parsed({"--list-texts"}).action, Action::ListTexts);

            const CliOptions removal = parsed({"--remove-text", "12"});
            EXPECT_EQ(removal.action, Action::RemoveText);
            EXPECT_EQ(removal.remove_id, core::TextId{12});
        }

        TEST(CliParserTest, EveryHistoryFlagParses) {
            EXPECT_EQ(parsed({"--stats"}).action, Action::Stats);

            const CliOptions csv = parsed({"--export", "csv"});
            EXPECT_EQ(csv.action, Action::Export);
            EXPECT_EQ(csv.operand, "csv");
            EXPECT_EQ(parsed({"--export", "json"}).operand, "json");

            EXPECT_EQ(parsed({"--last", "20"}).last, 20);
        }

        TEST(CliParserTest, EveryDiagnosticFlagParses) {
            const CliOptions simulate = parsed({"--simulate", "keys.tks"});
            EXPECT_EQ(simulate.action, Action::Simulate);
            EXPECT_EQ(simulate.operand, "keys.tks");

            EXPECT_EQ(parsed({"--doctor"}).action, Action::Doctor);
            EXPECT_EQ(parsed({"--config", "/tmp/c.toml"}).config_path, "/tmp/c.toml");
            EXPECT_EQ(parsed({"--data-dir", "/tmp/data"}).data_dir, "/tmp/data");
        }

        TEST(CliParserTest, AValueMayBeAttachedWithAnEquals) {
            EXPECT_EQ(parsed({"--mode=quote"}).mode, "quote");
            EXPECT_EQ(parsed({"--time=15"}).seconds, 15);
        }

        TEST(CliParserTest, AFlagThatTakesNoValueRefusesOne) {
            EXPECT_EQ(refusal({"--doctor=yes"}), "--doctor takes no value");
        }

        // --- Positionals, `-` and `--` ----------------------------------

        TEST(CliParserTest, APositionalIsTheTextToType) {
            const CliOptions options = parsed({"/tmp/a.txt"});

            EXPECT_EQ(options.action, Action::Run);
            EXPECT_EQ(options.text_path, "/tmp/a.txt");
        }

        TEST(CliParserTest, ABareDashMeansStandardInput) {
            const CliOptions options = parsed({"-"});

            EXPECT_TRUE(options.text_from_stdin);
            EXPECT_FALSE(options.text_path.has_value());
        }

        TEST(CliParserTest, DoubleDashEndsOptionParsing) {
            // The escape hatch for a file whose name starts with a dash, which
            // is otherwise unnameable.
            const CliOptions options = parsed({"--mode", "quote", "--", "--weird-name.txt"});

            EXPECT_EQ(options.mode, "quote");
            EXPECT_EQ(options.text_path, "--weird-name.txt");
        }

        TEST(CliParserTest, AfterDoubleDashADashIsAFileNotStandardInput) {
            const CliOptions options = parsed({"--", "-"});

            EXPECT_EQ(options.text_path, "-");
            EXPECT_FALSE(options.text_from_stdin);
        }

        TEST(CliParserTest, TwoTextsAreRefusedHoweverTheyWereSpelled) {
            EXPECT_EQ(refusal({"--text", "a.txt", "--text-id", "3"}), "--text and --text-id cannot be combined");
            // A filename is quoted back as itself: dressing it up as
            // `--a.txt` would be worse than saying nothing.
            EXPECT_EQ(refusal({"a.txt", "b.txt"}), "a.txt and b.txt cannot be combined");
            EXPECT_EQ(refusal({"--text", "a.txt", "-"}), "--text and - cannot be combined");
        }

        // --- Argument order ---------------------------------------------

        TEST(CliParserTest, ArgumentOrderDoesNotMatter) {
            const CliOptions first = parsed({"--mode", "timed", "--time", "30", "a.txt"});
            const CliOptions second = parsed({"a.txt", "--time", "30", "--mode", "timed"});

            EXPECT_EQ(first.mode, second.mode);
            EXPECT_EQ(first.seconds, second.seconds);
            EXPECT_EQ(first.text_path, second.text_path);
        }

        // --- Unknown flags ----------------------------------------------

        TEST(CliParserTest, AnUnknownFlagSuggestsTheNearestRealOne) {
            EXPECT_EQ(refusal({"--tim", "30"}), "unknown option --tim (did you mean --time?)");
            EXPECT_EQ(refusal({"--wrods", "50"}), "unknown option --wrods (did you mean --words?)");
            EXPECT_EQ(refusal({"--sections", "2"}), "unknown option --sections (did you mean --section?)");
        }

        TEST(CliParserTest, AFlagNothingResemblesIsRefusedWithoutAGuess) {
            // Offering the alphabetically luckiest flag would send the reader
            // off to read about something they never asked for.
            EXPECT_EQ(refusal({"--xyzzy"}), "unknown option --xyzzy");
        }

        TEST(CliParserTest, AnUnknownShortFlagIsRefused) { EXPECT_EQ(refusal({"-q"}), "unknown option -q"); }

        TEST(CliParserTest, ClusteredShortFlagsAreRefusedRatherThanHalfUnderstood) {
            EXPECT_EQ(refusal({"-hV"}), "unknown option -hV");
        }

        // --- Missing and malformed values -------------------------------

        TEST(CliParserTest, AMissingValueNamesTheFlag) {
            EXPECT_EQ(refusal({"--time"}), "--time expects a value");
            EXPECT_EQ(refusal({"-m"}), "--mode expects a value");
            EXPECT_EQ(refusal({"--mode", "timed", "--export"}), "--export expects a value");
        }

        TEST(CliParserTest, ANumberThatIsNotANumberIsRefused) {
            EXPECT_EQ(refusal({"--time", "soon"}), "--time = \"soon\" (expected a whole number)");
            // from_chars is happy to read the front of "30s" and leave the
            // rest, which would silently accept a typo as 30.
            EXPECT_EQ(refusal({"--time", "30s"}), "--time = \"30s\" (expected a whole number)");
        }

        TEST(CliParserTest, OutOfRangeNumbersNameTheirRange) {
            EXPECT_EQ(refusal({"--time", "0"}), "--time = 0 (expected 1..3600)");
            EXPECT_EQ(refusal({"--time", "3601"}), "--time = 3601 (expected 1..3600)");
            EXPECT_EQ(refusal({"--words", "0"}), "--words = 0 (expected 1..10000)");
            EXPECT_EQ(refusal({"--words", "10001"}), "--words = 10001 (expected 1..10000)");
        }

        TEST(CliParserTest, TheRangesAreTheOnesTheConfigurationFileEnforces) {
            // The command line overrides general.default_duration_s and
            // general.default_word_count. A command line that accepted what
            // the file refuses would be a second, quieter set of rules.
            EXPECT_TRUE(parse_of({"--time", "1"}));
            EXPECT_TRUE(parse_of({"--time", "3600"}));
            EXPECT_TRUE(parse_of({"--words", "1"}));
            EXPECT_TRUE(parse_of({"--words", "10000"}));
        }

        TEST(CliParserTest, AnIdentifierIsPositive) {
            EXPECT_EQ(refusal({"--text-id", "0"}).substr(0, 20), "--text-id = 0 (expec");
            EXPECT_EQ(refusal({"--remove-text", "-3"}).substr(0, 21), "--remove-text = -3 (e");
            EXPECT_EQ(refusal({"--section", "0"}).substr(0, 18), "--section = 0 (exp");
            EXPECT_EQ(refusal({"--last", "0"}).substr(0, 15), "--last = 0 (exp");
        }

        // --- Vocabularies -----------------------------------------------

        TEST(CliParserTest, AModeOutsideTheVocabularyIsRefusedWithTheListNamed) {
            EXPECT_EQ(refusal({"--mode", "fast"}),
                      "--mode = \"fast\" (expected one of: timed, words, quote, zen, endless, race)");
        }

        TEST(CliParserTest, EveryDocumentedModeIsAccepted) {
            for (const std::string_view mode: {"timed", "words", "quote", "zen", "endless", "race"}) {
                EXPECT_TRUE(parse_of({"--mode", mode})) << mode;
            }
        }

        TEST(CliParserTest, AnExportFormatOutsideTheVocabularyIsRefused) {
            EXPECT_EQ(refusal({"--export", "xml"}), "--export = \"xml\" (expected one of: csv, json)");
        }

        TEST(CliParserTest, ARacePresetOutsideTheVocabularyIsRefused) {
            // `custom` is what a configuration file becomes once it overrides a
            // preset, not something to ask for by name.
            EXPECT_EQ(refusal({"--race-preset", "custom"}),
                      "--race-preset = \"custom\" (expected one of: gentle, standard, brutal)");
        }

        // --- One action at a time ---------------------------------------

        TEST(CliParserTest, TwoActionsAreRefusedNamingBoth) {
            EXPECT_EQ(refusal({"--stats", "--doctor"}), "--stats and --doctor cannot be combined");
            EXPECT_EQ(refusal({"--import", "a.txt", "--export", "csv"}), "--import and --export cannot be combined");
            EXPECT_EQ(refusal({"--list-texts", "--remove-text", "3"}),
                      "--list-texts and --remove-text cannot be combined");
        }

        TEST(CliParserTest, AnActionMayStillCarryTheModifiersItUses) {
            const CliOptions options = parsed({"--export", "json", "--last", "10", "--data-dir", "/tmp/d"});

            EXPECT_EQ(options.action, Action::Export);
            EXPECT_EQ(options.operand, "json");
            EXPECT_EQ(options.last, 10);
            EXPECT_EQ(options.data_dir, "/tmp/d");
        }

        // --- Help and version -------------------------------------------

        TEST(CliParserTest, HelpAndVersionShortCircuit) {
            EXPECT_EQ(parsed({"--help"}).action, Action::Help);
            EXPECT_EQ(parsed({"-h"}).action, Action::Help);
            EXPECT_EQ(parsed({"--version"}).action, Action::Version);
            EXPECT_EQ(parsed({"-V"}).action, Action::Version);
        }

        TEST(CliParserTest, HelpBeatsWhateverFollowsIt) {
            // Somebody who has just mistyped a flag and asked for help should
            // get help, not another complaint about the typo.
            EXPECT_EQ(parsed({"--help", "--nonsense"}).action, Action::Help);
            EXPECT_EQ(parsed({"--version", "--time", "0"}).action, Action::Version);
        }

        TEST(CliParserTest, WhatComesBeforeHelpIsDiscardedWithIt) {
            // The invocation prints help and exits; carrying half-parsed
            // options past that point would only invite somebody to act on
            // them.
            const CliOptions options = parsed({"--mode", "zen", "--help"});

            EXPECT_EQ(options.action, Action::Help);
            EXPECT_FALSE(options.mode.has_value());
        }

        TEST(CliParserTest, AnErrorBeforeHelpIsStillAnError) {
            // Short-circuiting is about what follows `--help`, not about
            // pardoning what was already refused.
            EXPECT_EQ(refusal({"--time", "0", "--help"}), "--time = 0 (expected 1..3600)");
        }

    }  // namespace
}  // namespace typeit::cli
