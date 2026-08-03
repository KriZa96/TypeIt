#include "typeit/core/metrics/RollingWpm.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <vector>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        constexpr double kGraphemesPerWord = 5.0;
        constexpr double kMillisPerMinute = 60'000.0;
        constexpr Millis kSampleInterval{1'000};

    }  // namespace

    RollingWpm::RollingWpm(const TextBuffer& target, Millis window) : target_{target.graphemes()}, window_{window} {
        assert(window.value > 0 && "a window of no time holds no keystrokes");
    }

    bool RollingWpm::is_correct(std::size_t event_index, const KeystrokeLog& log) const {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- callers hold the index
        const Keystroke& event = log.events()[event_index];
        if (event.kind != KeystrokeKind::Character || event.target >= target_.size()) {
            return false;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounds checked
        return event.typed == target_[event.target];
    }

    Wpm RollingWpm::advance(const KeystrokeLog& log, Millis now) {
        const std::span<const Keystroke> events = log.events();

        // In: everything that has happened by now and has not been counted yet.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounded by size()
        while (consumed_ < events.size() && events[consumed_].at <= now) {
            if (!started_) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounded above
                start_ = events[consumed_].at;
                started_ = true;
            }
            correct_ += is_correct(consumed_, log) ? 1U : 0U;
            ++consumed_;
            ++visited_;
        }

        // Out: everything that has fallen off the back of the window. The index
        // never rewinds, which is the whole reason this class holds state.
        const Millis oldest = now - window_;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounded by consumed_
        while (left_ < consumed_ && events[left_].at <= oldest) {
            correct_ -= is_correct(left_, log) ? 1U : 0U;
            ++left_;
            ++visited_;
        }

        if (!started_) {
            return Wpm{0.0};
        }

        // Before the window has filled, the divisor is the time that has really
        // passed. After it, the window itself.
        const Millis elapsed = now - start_;
        const auto span = static_cast<double>(std::min(elapsed, window_).value);
        if (span <= 0.0) {
            return Wpm{0.0};
        }
        return Wpm{static_cast<double>(correct_) / kGraphemesPerWord / (span / kMillisPerMinute)};
    }

    Wpm peak_sustained_wpm(const KeystrokeLog& log, const TextBuffer& target, SustainedWindow over) {
        assert(over.sustain.value > 0 && "a plateau of no length is every plateau");
        if (log.empty()) {
            return Wpm{0.0};
        }

        // One rolling reading a second, taken through the same class the HUD
        // uses rather than a second implementation of the same idea.
        RollingWpm rolling{target, over.window};
        const Millis start = log.events().front().at;
        const Millis end = log.events().back().at;

        std::vector<double> samples;
        for (Millis now = start; now <= end; now += kSampleInterval) {
            samples.push_back(rolling.advance(log, now).value);
        }

        const auto needed = static_cast<std::size_t>(over.sustain.value / kSampleInterval.value);
        if (needed == 0 || samples.size() < needed) {
            return Wpm{0.0};
        }

        // The best level that never dropped for a whole `sustain`: the largest
        // minimum over any run of that many consecutive samples.
        double best = 0.0;
        for (std::size_t first = 0; first + needed <= samples.size(); ++first) {
            double lowest = std::numeric_limits<double>::max();
            for (std::size_t i = first; i < first + needed; ++i) {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounded above
                lowest = std::min(lowest, samples[i]);
            }
            best = std::max(best, lowest);
        }
        return Wpm{best};
    }

}  // namespace typeit::core
