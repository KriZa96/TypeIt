#include "typeit/core/race/SpeedWall.h"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "typeit/core/race/Pacer.h"
#include "typeit/core/race/RaceParams.h"
#include "typeit/core/session/Keystroke.h"
#include "typeit/core/session/KeystrokeLog.h"
#include "typeit/core/text/Grapheme.h"
#include "typeit/core/text/TextBuffer.h"
#include "typeit/core/util/Units.h"

namespace typeit::core {
    namespace {

        /// One attempt at one position: when, whether it was right first time,
        /// and what went wrong if it was not.
        struct Attempt {
            Millis at{0};
            bool correct = false;
            std::string expected;
            std::string typed;
        };

        /// The first-attempt story of a run, in order.
        ///
        /// First attempts only: a position typed wrongly and then corrected was
        /// got wrong, and retyping the same mistake at the same place is the
        /// same mistake rather than a second one. That is the same rule the
        /// accuracy gate reads during the race, so the analysis afterwards
        /// agrees with what the ramp was reacting to at the time.
        [[nodiscard]] std::vector<Attempt> attempts_of(const KeystrokeLog& log, const TextBuffer& target) {
            std::vector<Attempt> attempts;
            std::vector<bool> seen(target.size(), false);
            for (const Keystroke& event: log.events()) {
                if (event.kind != KeystrokeKind::Character || event.target >= target.size()) {
                    continue;
                }
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounds checked
                if (seen[event.target]) {
                    continue;
                }
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access) -- bounds checked
                seen[event.target] = true;
                const Grapheme wanted = target.at(GraphemeIndex{event.target});
                attempts.push_back(Attempt{.at = event.at,
                                           .correct = event.typed == wanted,
                                           .expected = std::string{wanted.view()},
                                           .typed = std::string{event.typed.view()}});
            }
            return attempts;
        }

        /// Where rolling accuracy fell through the gate and never came back.
        ///
        /// Found from the end rather than from the start, which is what makes
        /// "and did not recover" one comparison instead of a search: the wall
        /// begins just after the *last* moment accuracy was acceptable. A
        /// transient dip in the middle of a run has an acceptable moment after
        /// it, so it is not the wall — which is the case this is really for.
        [[nodiscard]] std::optional<std::size_t> collapse_from(const std::vector<Attempt>& attempts,
                                                               const RaceParams& params) {
            if (attempts.empty()) {
                return std::nullopt;
            }

            std::deque<bool> window;
            std::size_t correct = 0;
            // One accuracy reading per attempt, in step with it.
            std::vector<bool> acceptable(attempts.size(), true);
            for (std::size_t at = 0; at < attempts.size(); ++at) {
                window.push_back(attempts.at(at).correct);
                correct += attempts.at(at).correct ? 1U : 0U;
                while (window.size() > params.accuracy_window) {
                    correct -= window.front() ? 1U : 0U;
                    window.pop_front();
                }
                const double accuracy = static_cast<double>(correct) / static_cast<double>(window.size());
                acceptable.at(at) = accuracy >= params.min_accuracy.value;
            }

            if (acceptable.back()) {
                // It ended acceptably, so whatever happened earlier recovered.
                return std::nullopt;
            }
            for (std::size_t at = attempts.size(); at > 0; --at) {
                if (acceptable.at(at - 1)) {
                    return at;
                }
            }
            // Never acceptable at all: the whole run is the collapse.
            return std::size_t{0};
        }

        /// The ghost's speed at a moment, from the once-a-second curve.
        [[nodiscard]] std::optional<Wpm> speed_at(std::span<const PacerSample> curve, Millis at) {
            std::optional<Wpm> found;
            for (const PacerSample& sample: curve) {
                if (sample.at > at) {
                    break;
                }
                found = sample.wpm;
            }
            return found;
        }

    }  // namespace

    SpeedWall speed_wall(const KeystrokeLog& log, const TextBuffer& target, std::span<const PacerSample> curve,
                         const RaceParams& params) {
        SpeedWall wall;
        const std::vector<Attempt> attempts = attempts_of(log, target);
        const std::optional<std::size_t> from = collapse_from(attempts, params);
        if (!from.has_value()) {
            return wall;
        }

        // The log's clock is the run's, and so is the curve's, but the log
        // starts at whatever the session was handed. Both are offsets from the
        // first event for the purposes of looking one up in the other.
        const Millis origin = log.events().empty() ? Millis{0} : log.events().front().at;
        const Millis began{attempts.at(*from).at.value - origin.value};
        wall.began = began;

        double low = 0.0;
        double high = 0.0;
        bool any = false;
        for (const PacerSample& sample: curve) {
            if (sample.at < began) {
                continue;
            }
            low = any ? std::min(low, sample.wpm.value) : sample.wpm.value;
            high = any ? std::max(high, sample.wpm.value) : sample.wpm.value;
            any = true;
        }
        if (!any) {
            // A collapse the curve says nothing about — a race that ended
            // inside one second, or an endless run with no ghost at all. The
            // band stays absent rather than being invented from the one
            // reading either side of it.
            if (const std::optional<Wpm> at_start = speed_at(curve, began); at_start.has_value()) {
                low = at_start->value;
                high = at_start->value;
                any = true;
            }
        }
        if (any) {
            wall.low = Wpm{low};
            wall.high = Wpm{high};
        }

        // Only what failed *inside* the band. The whole run's worst pairs are
        // the mistakes somebody makes anyway; these are the ones standing
        // between them and going faster, which is a different list and the
        // reason this function exists.
        std::map<std::pair<std::string, std::string>, std::size_t> counts;
        for (std::size_t at = *from; at < attempts.size(); ++at) {
            const Attempt& attempt = attempts.at(at);
            if (!attempt.correct) {
                ++counts[std::pair{attempt.expected, attempt.typed}];
            }
        }

        wall.pairs.reserve(counts.size());
        for (const auto& [pair, count]: counts) {
            wall.pairs.push_back(WallPair{.expected = pair.first, .typed = pair.second, .count = count});
        }
        // Most frequent first, then by the pair itself — so two pairs that
        // failed equally often come back in the same order on every machine.
        std::ranges::sort(wall.pairs, [](const WallPair& left, const WallPair& right) {
            if (left.count != right.count) {
                return left.count > right.count;
            }
            return std::pair{left.expected, left.typed} < std::pair{right.expected, right.typed};
        });
        if (wall.pairs.size() > kWallPairs) {
            wall.pairs.resize(kWallPairs);
        }
        return wall;
    }

}  // namespace typeit::core
