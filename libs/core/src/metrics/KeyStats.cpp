#include "typeit/core/metrics/KeyStats.h"

#include <cassert>
#include <cstddef>
#include <optional>
#include <string>

#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        void record(KeyStat& stat, bool correct, std::optional<Millis> latency, Millis outlier_threshold) {
            ++stat.attempts;
            stat.errors += correct ? 0U : 1U;
            if (latency.has_value() && latency->value >= 0 && *latency <= outlier_threshold) {
                stat.total_latency += *latency;
                ++stat.latency_samples;
            }
        }

    }  // namespace

    KeyStats key_stats(const KeystrokeLog& log, const TextBuffer& target, Millis outlier_threshold) {
        assert(outlier_threshold.value > 0 && "with no threshold every pause is typing speed");

        KeyStats stats;
        std::optional<Millis> previous_event;
        // The last position a grapheme was actually typed at, which is what
        // makes a bigram a transition between neighbouring positions rather
        // than between neighbouring keystrokes.
        std::optional<std::size_t> previous_position;

        for (const Keystroke& event: log.events()) {
            const std::optional<Millis> latency =
                    previous_event.has_value() ? std::optional{event.at - *previous_event} : std::nullopt;
            previous_event = event.at;

            if (event.kind != KeystrokeKind::Character || event.target >= target.size()) {
                // A backspace is not an attempt at a key, and it breaks the
                // chain: what follows it is a retype, not a transition.
                previous_position.reset();
                continue;
            }

            const Grapheme& expected = target.at(GraphemeIndex{event.target});
            const bool correct = event.typed == expected;
            record(stats.per_grapheme[std::string{expected.view()}], correct, latency, outlier_threshold);

            if (previous_position.has_value() && *previous_position + 1 == event.target) {
                const Grapheme& before = target.at(GraphemeIndex{*previous_position});
                const std::string bigram = std::string{before.view()} + std::string{expected.view()};
                record(stats.per_bigram[bigram], correct, latency, outlier_threshold);
            }
            previous_position = event.target;
        }

        return stats;
    }

}  // namespace typeit::core
