// Difficulty scoring (TI-072).
//
// The test that matters is the first one: the project's own three corpora have
// to come out in ascending order. Everything else here is a guard rail around
// a number that is advisory and must never be NaN, out of range, or different
// on the second call.

#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <string_view>

#include "typeit/core/text/Difficulty.h"

namespace typeit::core {
    namespace {

        /// The corpora, found through the path CMake compiled in rather than
        /// through `__FILE__` — that is defect C2, and it makes the tests pass
        /// only on the machine that built them.
        std::string corpus(std::string_view name) {
            const std::string path = std::string{TYPEIT_TEST_CORPUS_DIR} + "/" + std::string{name};
            std::ifstream file{path, std::ios::binary};
            EXPECT_TRUE(file) << path;
            std::ostringstream contents;
            contents << file.rdbuf();
            return contents.str();
        }

        TEST(DifficultyScoreTest, TheBundledCorporaScoreInAscendingOrder) {
            const double simple = difficulty_score(corpus("simple.txt"));
            const double medium = difficulty_score(corpus("medium.txt"));
            const double hard = difficulty_score(corpus("hard.txt"));

            EXPECT_LT(simple, medium) << simple << " vs " << medium;
            EXPECT_LT(medium, hard) << medium << " vs " << hard;
        }

        TEST(DifficultyScoreTest, EveryScoreIsWithinTheScale) {
            const std::string_view texts[] = {
                    "",
                    " ",
                    "a",
                    ".....",
                    "1234567890",
                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                    "«Ἀγαθὸν» — τὸ μὴ γενέσθαι, 42%!",
            };
            for (const std::string_view text: texts) {
                const double score = difficulty_score(text);
                EXPECT_GE(score, kMinDifficulty) << text;
                EXPECT_LE(score, kMaxDifficulty) << text;
                EXPECT_EQ(score, score) << "NaN would propagate into the library sort";
            }
        }

        TEST(DifficultyScoreTest, EmptyTextIsTheEasiestThingThereIs) {
            EXPECT_DOUBLE_EQ(difficulty_score(""), kMinDifficulty) << "nothing to type is not hard to type";
        }

        TEST(DifficultyScoreTest, ScoringIsDeterministic) {
            const std::string text = corpus("medium.txt");
            EXPECT_DOUBLE_EQ(difficulty_score(text), difficulty_score(text));
        }

        TEST(DifficultyScoreTest, RareWordsRaiseTheScoreAndCommonOnesDoNot) {
            const double common = difficulty_score("the cat sat on the mat and the dog saw it");
            const double rare = difficulty_score("perspicacious anfractuosity meticulously perambulating");

            EXPECT_LT(common, rare);
        }

        TEST(DifficultyScoreTest, TheFeaturesAreVisibleSoAScoreCanBeExplained) {
            const DifficultyFeatures plain = difficulty_features("the cat sat on the mat");
            const DifficultyFeatures loud = difficulty_features("THE CAT SAT ON THE MAT");
            const DifficultyFeatures punctuated = difficulty_features("the, cat, sat, on, the, mat,");
            const DifficultyFeatures numeric = difficulty_features("the cat 1234567 on the mat");
            const DifficultyFeatures accented = difficulty_features("thé çat sàt ön thé mät");

            EXPECT_GT(loud.capital_density, plain.capital_density);
            EXPECT_GT(punctuated.punctuation_density, plain.punctuation_density);
            EXPECT_GT(numeric.digit_density, plain.digit_density);
            EXPECT_GT(accented.non_ascii_density, plain.non_ascii_density);
            EXPECT_DOUBLE_EQ(plain.non_ascii_density, 0.0);
        }

        TEST(DifficultyScoreTest, PunctuationHangingOffAWordDoesNotMakeItRare) {
            // `"The cat,` and `the cat` are the same two words, or every text
            // with quotation marks in it scores as unfamiliar vocabulary.
            EXPECT_DOUBLE_EQ(difficulty_features("the cat sat").rare_word_ratio,
                             difficulty_features("\"The cat,\" sat!").rare_word_ratio);
        }

        TEST(DifficultyScoreTest, InvalidUtf8ScoresRatherThanCrashes) {
            // The normalizer is where malformed input is refused, with a byte
            // offset. An advisory number nothing depends on does not get to
            // fail as well.
            const double score = difficulty_score("fine\xE6\xBC");
            EXPECT_GE(score, kMinDifficulty);
            EXPECT_LE(score, kMaxDifficulty);
        }

    }  // namespace
}  // namespace typeit::core
