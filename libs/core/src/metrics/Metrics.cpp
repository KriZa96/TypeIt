#include "typeit/core/metrics/Metrics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        constexpr double kGraphemesPerWord = 5.0;
        constexpr double kMillisPerMinute = 60'000.0;

        struct Tally {
            /// Every grapheme entered, whether it survived or not. Backspaces
            /// are not graphemes and are not counted.
            std::size_t entered = 0;
            /// Positions whose final content matches the target.
            std::size_t correct = 0;
            /// Positions whose final content is there and is wrong. A position
            /// that was typed and then deleted is neither: nothing stands at
            /// it, so there is nothing to be wrong.
            std::size_t uncorrected = 0;
        };

        Tally tally(const KeystrokeLog& log, const TextBuffer& target) {
            // What each position ended up holding. Replaying is what makes a
            // deleted mistake stop counting — the whole point of ADR-002.
            std::vector<std::optional<Grapheme>> final_state(target.size());

            Tally counts;
            for (const Keystroke& event: log.events()) {
                if (event.kind == KeystrokeKind::Character) {
                    ++counts.entered;
                }
                if (event.target >= target.size()) {
                    // A space that ran past the last word resolves against no
                    // position. It was still typed, so it counts as entered.
                    continue;
                }
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounds checked
                final_state[event.target] =
                        event.kind == KeystrokeKind::Character ? std::optional{event.typed} : std::nullopt;
            }

            for (std::size_t position = 0; position < target.size(); ++position) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- position < size
                const std::optional<Grapheme>& typed = final_state[position];
                if (!typed.has_value()) {
                    continue;
                }
                if (*typed == target.at(GraphemeIndex{position})) {
                    ++counts.correct;
                } else {
                    ++counts.uncorrected;
                }
            }
            return counts;
        }

        constexpr double kMillisPerBucket = 1'000.0;
        /// One second of graphemes is a twelfth of a minute's worth of words:
        /// `(graphemes / 5) / (1 / 60)`.
        constexpr double kWpmPerGraphemePerSecond = kMillisPerMinute / kMillisPerBucket / kGraphemesPerWord;

        /// Gross WPM in each one-second window of the run, from the first
        /// keystroke to the last. A window with nothing in it is a zero, which
        /// is what lets the metric see a pause.
        std::vector<double> per_second_gross(const KeystrokeLog& log, const TextBuffer& target) {
            if (log.empty()) {
                return {};
            }

            const std::int64_t start = log.events().front().at.value;
            const auto span = static_cast<double>(log.duration().value);
            const auto buckets = static_cast<std::size_t>(std::max(1.0, std::ceil(span / kMillisPerBucket)));

            std::vector<double> samples(buckets, 0.0);
            for (const Keystroke& event: log.events()) {
                if (event.kind != KeystrokeKind::Character || event.target >= target.size()) {
                    continue;
                }
                if (!(event.typed == target.at(GraphemeIndex{event.target}))) {
                    continue;
                }
                const auto offset = static_cast<double>(event.at.value - start);
                // The last event of a run lands exactly on a boundary; it
                // belongs to the window it ends, not to one past the end.
                const auto bucket = std::min(buckets - 1, static_cast<std::size_t>(offset / kMillisPerBucket));
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- clamped above
                samples[bucket] += kWpmPerGraphemePerSecond;
            }
            return samples;
        }

    }  // namespace

    SpeedMetrics speed(const KeystrokeLog& log, const TextBuffer& target) {
        const double minutes = static_cast<double>(log.duration().value) / kMillisPerMinute;
        if (minutes <= 0.0) {
            return {};
        }

        const Tally counts = tally(log, target);
        const double raw = static_cast<double>(counts.entered) / kGraphemesPerWord / minutes;
        const double gross = static_cast<double>(counts.correct) / kGraphemesPerWord / minutes;
        const double penalty = static_cast<double>(counts.uncorrected) / minutes;

        return SpeedMetrics{.raw = Wpm{raw}, .gross = Wpm{gross}, .net = Wpm{std::max(0.0, gross - penalty)}};
    }

    AccuracyMetrics accuracy(const KeystrokeLog& log, const TextBuffer& target) {
        // Two passes over one replay. `first` is what the position was given
        // the first time it was tried and never changes afterwards — deleting
        // and retyping does not open a second attempt, which is the whole of
        // defect C4. `final_state` is what stands there at the end.
        std::vector<std::optional<Grapheme>> first_attempt(target.size());
        std::vector<std::optional<Grapheme>> final_state(target.size());
        std::vector<bool> attempted(target.size(), false);

        for (const Keystroke& event: log.events()) {
            if (event.target >= target.size()) {
                continue;
            }
            if (event.kind != KeystrokeKind::Character) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounds checked
                final_state[event.target] = std::nullopt;
                continue;
            }
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounds checked
            final_state[event.target] = event.typed;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounds checked
            if (!attempted[event.target]) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounds checked
                attempted[event.target] = true;
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounds checked
                first_attempt[event.target] = event.typed;
            }
        }

        AccuracyMetrics metrics;
        std::size_t resolved = 0;
        std::size_t correct = 0;
        for (std::size_t position = 0; position < target.size(); ++position) {
            const Grapheme& expected = target.at(GraphemeIndex{position});
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- position < size
            if (const std::optional<Grapheme>& first = first_attempt[position]; first.has_value()) {
                ++metrics.attempted;
                if (!(*first == expected)) {
                    ++metrics.first_attempt_errors;
                }
            }
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- position < size
            if (const std::optional<Grapheme>& last = final_state[position]; last.has_value()) {
                ++resolved;
                correct += *last == expected ? 1U : 0U;
            }
        }

        if (metrics.attempted > 0) {
            const auto right = static_cast<double>(metrics.attempted - metrics.first_attempt_errors);
            metrics.accuracy = Accuracy{right / static_cast<double>(metrics.attempted)};
        }
        if (resolved > 0) {
            metrics.final_correctness = Accuracy{static_cast<double>(correct) / static_cast<double>(resolved)};
        }
        return metrics;
    }

    double consistency(const KeystrokeLog& log, const TextBuffer& target) {
        const std::vector<double> samples = per_second_gross(log, target);
        if (samples.empty()) {
            return 0.0;
        }

        double sum = 0.0;
        for (const double sample: samples) {
            sum += sample;
        }
        const double mean = sum / static_cast<double>(samples.size());
        if (mean <= 0.0) {
            // Nothing was typed correctly, so there is no rate to be steady at.
            return 0.0;
        }

        double squared = 0.0;
        for (const double sample: samples) {
            squared += (sample - mean) * (sample - mean);
        }
        const double deviation = std::sqrt(squared / static_cast<double>(samples.size()));

        return std::max(0.0, 100.0 * (1.0 - (deviation / mean)));
    }

}  // namespace typeit::core
