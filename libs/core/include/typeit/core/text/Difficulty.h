// How hard a text is to type, on a scale of one to ten.
//
// Advisory, and deliberately so: it is shown in the library and usable as a
// filter, and it never gates anything. A number that decided what you were
// allowed to practise would be a number that has to be right, and no six-term
// weighted sum of surface features is right about prose.
//
// The formula is TECHNICAL §8.3. What makes it checkable rather than
// decorative is the project's own corpora: assets/texts/simple.txt, medium.txt
// and hard.txt have to come out in that order, and they are the test.
#ifndef TYPEIT_CORE_TEXT_DIFFICULTY_H
#define TYPEIT_CORE_TEXT_DIFFICULTY_H

#include <string_view>

namespace typeit::core {

    /// The six things measured, each already normalised to 0–1 against the
    /// empirical bounds in TECHNICAL §8.3. Exposed because "this text scored
    /// 8.2" is much less useful than "because a third of its words are rare".
    struct DifficultyFeatures {
        double mean_word_length = 0.0;
        double rare_word_ratio = 0.0;
        double punctuation_density = 0.0;
        double capital_density = 0.0;
        double digit_density = 0.0;
        double non_ascii_density = 0.0;
    };

    /// The lowest and highest a score can be. Nothing outside this ever comes
    /// back, including for text that is empty or entirely punctuation.
    inline constexpr double kMinDifficulty = 1.0;
    inline constexpr double kMaxDifficulty = 10.0;

    [[nodiscard]] DifficultyFeatures difficulty_features(std::string_view text);

    /// Weighted sum of the features, scaled to [1, 10].
    ///
    /// Empty text scores `kMinDifficulty` rather than NaN: nothing to type is
    /// not hard to type, and a NaN would propagate into the library sort.
    [[nodiscard]] double difficulty_score(std::string_view text);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_DIFFICULTY_H
