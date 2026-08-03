#include "typeit/core/metrics/Timeline.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        constexpr double kGraphemesPerWord = 5.0;
        constexpr double kMillisPerMinute = 60'000.0;

    }  // namespace

    std::vector<TimelineSample> timeline(const KeystrokeLog& log, const TextBuffer& target, Millis bucket) {
        assert(bucket.value > 0 && "a bucket of no time holds no keystrokes");
        if (log.empty()) {
            return {};
        }

        const Millis start = log.events().front().at;
        const auto span = static_cast<double>(log.duration().value);
        const auto width = static_cast<double>(bucket.value);
        const auto count = static_cast<std::size_t>(std::max(1.0, std::ceil(span / width)));

        std::vector<TimelineSample> samples(count);
        for (std::size_t i = 0; i < count; ++i) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- i < count
            samples[i].at = start + Millis{static_cast<std::int64_t>(i) * bucket.value};
        }

        for (const Keystroke& event: log.events()) {
            const auto offset = static_cast<double>((event.at - start).value);
            // The run's last event sits exactly on the closing boundary; it
            // belongs to the bucket it ends rather than to one past the end.
            const auto index = std::min(count - 1, static_cast<std::size_t>(offset / width));
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- clamped above
            TimelineSample& sample = samples[index];

            ++sample.keystrokes;
            if (event.kind != KeystrokeKind::Character || event.target >= target.size()) {
                continue;
            }
            if (event.typed == target.at(GraphemeIndex{event.target})) {
                sample.wpm.value += 1.0;
            } else {
                ++sample.errors;
            }
        }

        // The counts become a rate only once they are all in: a bucket of
        // `bucket` milliseconds holding n correct graphemes is (n / 5) words in
        // that fraction of a minute.
        const double minutes = width / kMillisPerMinute;
        for (TimelineSample& sample: samples) {
            sample.wpm.value = sample.wpm.value / kGraphemesPerWord / minutes;
        }

        return samples;
    }

}  // namespace typeit::core
