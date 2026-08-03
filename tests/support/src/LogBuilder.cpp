#include "typeit/testing/LogBuilder.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/text/Whitespace.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/TypingRun.h"

namespace typeit::testing {
    namespace {

        using core::Grapheme;
        using core::Keystroke;
        using core::KeystrokeKind;
        using core::KeystrokeLog;
        using core::Millis;
        using core::TextBuffer;
        using core::Wpm;

        constexpr double kGraphemesPerWord = 5.0;
        constexpr double kMillisPerMinute = 60'000.0;

        /// How long a run of `graphemes` has to last to come out at `speed`, on
        /// the GAMEPLAY section 4.1 definition.
        double duration_for(std::size_t graphemes, Wpm speed) {
            assert(speed.value > 0.0 && "a run at zero words per minute never ends");
            return (static_cast<double>(graphemes) / kGraphemesPerWord) / speed.value * kMillisPerMinute;
        }

        Millis at(double milliseconds) { return Millis{static_cast<std::int64_t>(std::llround(milliseconds))}; }

        /// The evenly spaced timestamps of a `perfect` run: first keystroke at
        /// zero, last at the full duration, so elapsed-from-first-to-last is
        /// exactly what the requested speed implies.
        std::vector<Millis> even_times(std::size_t count, Wpm speed) {
            std::vector<Millis> times;
            times.reserve(count);
            if (count == 0) {
                return times;
            }
            if (count == 1) {
                times.push_back(Millis{0});
                return times;
            }
            const double step = duration_for(count, speed) / static_cast<double>(count - 1);
            for (std::size_t i = 0; i < count; ++i) {
                times.push_back(at(static_cast<double>(i) * step));
            }
            return times;
        }

        KeystrokeLog log_of(std::span<const Grapheme> graphemes, std::span<const Millis> times) {
            assert(graphemes.size() == times.size());
            KeystrokeLog log;
            log.reserve(graphemes.size());
            for (std::size_t i = 0; i < graphemes.size(); ++i) {
                log.append(Keystroke{.at = times[i],
                                     .target = static_cast<std::uint32_t>(i),
                                     .kind = KeystrokeKind::Character,
                                     .typed = graphemes[i]});
            }
            return log;
        }

        /// A grapheme that is not the one expected, so the error is an error
        /// whatever the text happens to say.
        Grapheme something_else(const Grapheme& expected) {
            const TextBuffer candidates = text_of("xq");
            const Grapheme& first = candidates.at(core::GraphemeIndex{0});
            return first == expected ? candidates.at(core::GraphemeIndex{1}) : first;
        }

    }  // namespace

    LogBuilder& LogBuilder::type(std::string_view grapheme, Millis when) {
        const TextBuffer typed = text_of(grapheme);
        assert(typed.size() == 1 && "LogBuilder::type takes one grapheme at a time");
        log_.append(Keystroke{.at = when,
                              .target = static_cast<std::uint32_t>(cursor_),
                              .kind = KeystrokeKind::Character,
                              .typed = typed.at(core::GraphemeIndex{0})});
        ++cursor_;
        return *this;
    }

    LogBuilder& LogBuilder::backspace(Millis when) {
        if (cursor_ > 0) {
            --cursor_;
        }
        log_.append(Keystroke{.at = when,
                              .target = static_cast<std::uint32_t>(cursor_),
                              .kind = KeystrokeKind::Backspace,
                              .typed = {}});
        return *this;
    }

    KeystrokeLog perfect(std::string_view text, Wpm speed) {
        const TextBuffer target = text_of(text);
        return log_of(target.graphemes(), even_times(target.size(), speed));
    }

    KeystrokeLog with_errors(std::string_view text, std::size_t errors, Wpm speed) {
        const TextBuffer target = text_of(text);
        std::vector<Grapheme> typed{target.graphemes().begin(), target.graphemes().end()};

        // Only the graphemes a mistake can sensibly land on, spread across the
        // whole text so a metric that looks at the first half sees some.
        std::vector<std::size_t> candidates;
        for (std::size_t i = 0; i < typed.size(); ++i) {
            if (!core::is_word_separator(typed[i])) {
                candidates.push_back(i);
            }
        }
        assert(errors <= candidates.size() && "the text does not have room for that many errors");

        for (std::size_t error = 0; error < errors; ++error) {
            const std::size_t position = candidates[error * candidates.size() / errors];
            typed[position] = something_else(typed[position]);
        }
        return log_of(typed, even_times(typed.size(), speed));
    }

    KeystrokeLog bursty(std::string_view text, Wpm speed) {
        const TextBuffer target = text_of(text);
        const std::size_t count = target.size();

        // Six bursts, whatever the length. A fixed small burst — five
        // keystrokes, say — lands one burst in every one-second window of a
        // 60 WPM run, which a per-second metric cannot tell from an even run at
        // all. The unevenness has to be coarser than the bucket it is measured
        // in to be unevenness.
        constexpr std::size_t kBursts = 6;
        const std::size_t kBurst = std::max<std::size_t>(2, (count + kBursts - 1) / kBursts);
        const std::size_t bursts = (count + kBurst - 1) / kBurst;
        if (bursts < 2) {
            return perfect(text, speed);
        }

        // Each burst is typed in a quarter of the time it is given, and the
        // bursts are spaced so the last keystroke still lands on the same
        // millisecond a `perfect` run would have ended on.
        const double total = duration_for(count, speed);
        const double span = total / (4.0 * static_cast<double>(bursts));
        const double between = (total - span) / static_cast<double>(bursts - 1);

        std::vector<Millis> times;
        times.reserve(count);
        for (std::size_t burst = 0; burst < bursts; ++burst) {
            const std::size_t first = burst * kBurst;
            const std::size_t last = std::min(first + kBurst, count);
            const double start = static_cast<double>(burst) * between;
            const double step = last - first > 1 ? span / static_cast<double>(last - first - 1) : 0.0;
            for (std::size_t i = first; i < last; ++i) {
                times.push_back(at(start + static_cast<double>(i - first) * step));
            }
        }
        // A final burst of exactly one keystroke would otherwise stop a burst
        // short of the total.
        times.back() = at(total);

        return log_of(target.graphemes(), times);
    }

    KeystrokeLog with_pauses(std::string_view text, std::span<const Millis> gaps, Wpm speed) {
        const TextBuffer target = text_of(text);
        const std::size_t count = target.size();
        std::vector<Millis> times = even_times(count, speed);
        if (count == 0 || gaps.empty()) {
            return log_of(target.graphemes(), times);
        }

        // Gap `g` opens after the keystroke at `after`, and everything from
        // there on is pushed back by it.
        Millis offset{0};
        std::size_t gap = 0;
        for (std::size_t i = 0; i < count; ++i) {
            while (gap < gaps.size() && i > (gap + 1) * count / (gaps.size() + 1)) {
                offset += gaps[gap];
                ++gap;
            }
            times[i] += offset;
        }
        return log_of(target.graphemes(), times);
    }

}  // namespace typeit::testing
