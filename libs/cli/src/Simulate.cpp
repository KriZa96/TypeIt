#include "typeit/cli/Simulate.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>

#include "typeit/app/Json.h"
#include "typeit/app/records/History.h"
#include "typeit/app/services/SessionService.h"
#include "typeit/cli/Script.h"
#include "typeit/core/metrics/Timeline.h"
#include "typeit/core/modes/IMode.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::cli {
    namespace {

        void append_field(std::string& out, std::string_view name, const std::string& written, bool first = false) {
            if (!first) {
                out += ',';
            }
            app::json::append_string(out, name);
            out += ':';
            out += written;
        }

        std::string quoted(std::string_view value) {
            std::string out;
            app::json::append_string(out, value);
            return out;
        }

        std::string count(std::size_t value) { return std::to_string(value); }

        /// The record, as JSON.
        ///
        /// Deliberately without `started_at` and `ended_at`: they come from the
        /// wall clock, and a harness whose output changed every time it ran
        /// would be no use as a fixture. Everything here is derived from the
        /// script's own relative times.
        std::string to_json(const app::SessionRecord& record) {
            std::string out = "{";
            append_field(out, "schema", std::to_string(kSimulationSchema), true);
            append_field(out, "mode", quoted(record.mode));
            append_field(out, "mode_param", quoted(record.mode_param));
            append_field(out, "provider", quoted(record.provider));
            append_field(out, "provider_seed", std::to_string(record.provider_seed));
            append_field(out, "completed", record.completed ? "true" : "false");
            append_field(out, "duration_ms", std::to_string(record.duration.value));
            append_field(out, "graphemes_typed", count(record.graphemes_typed));
            append_field(out, "graphemes_correct", count(record.graphemes_correct));
            append_field(out, "errors_total", count(record.errors_total));
            append_field(out, "errors_uncorrected", count(record.errors_uncorrected));
            append_field(out, "backspaces", count(record.backspaces));
            append_field(out, "raw_wpm", app::json::number(record.raw_wpm.value));
            append_field(out, "gross_wpm", app::json::number(record.gross_wpm.value));
            append_field(out, "net_wpm", app::json::number(record.net_wpm.value));
            append_field(out, "accuracy", app::json::number(record.accuracy.value));
            append_field(out, "final_correctness", app::json::number(record.final_correctness.value));
            append_field(out, "consistency", app::json::number(record.consistency));

            // Samples are rebased to zero. `core::timeline` stamps them on the
            // log's own clock, whose epoch is whenever the process happened to
            // start — absolute here would put a different number in the output
            // every time the same script ran, which is the one thing this
            // format promises not to do. The database keeps the log's own
            // stamps; a chart subtracts, and so does this.
            const core::Millis base = record.timeline.empty() ? core::Millis{0} : record.timeline.front().at;

            out += R"(,"timeline":[)";
            bool first = true;
            for (const core::TimelineSample& sample: record.timeline) {
                if (!first) {
                    out += ',';
                }
                first = false;
                out += R"({"at_ms":)" + std::to_string((sample.at - base).value);
                out += R"(,"wpm":)" + app::json::number(sample.wpm.value);
                out += R"(,"keystrokes":)" + count(sample.keystrokes);
                out += R"(,"errors":)" + count(sample.errors);
                out += '}';
            }
            out += "]}";
            return out;
        }

    }  // namespace

    core::Result<std::string> simulate(const app::SessionService& service, const app::SessionRequest& request,
                                       const Script& script) {
        core::Result<app::ActiveRun> run = service.start(request);
        if (!run) {
            return std::unexpected{run.error()};
        }

        // The script's timestamps are milliseconds from the start of the run.
        // The session was started on whatever clock the service holds, so every
        // event is offset from that rather than from zero — which is what keeps
        // a scripted run and a played one on the same clock.
        const core::Millis origin = run->session->started_at();

        for (const ScriptEvent& event: script.events) {
            const core::Millis at = origin + event.at;

            // Time first, exactly as a screen offers it: a timed run has to be
            // able to end between two keystrokes, and if it has, this one never
            // happened. A typist does not get to keep typing past the end, and
            // neither does a file.
            run->session->on_tick(at);
            if (run->session->is_finished()) {
                break;
            }

            if (event.is_backspace()) {
                run->session->on_backspace(at);
            } else {
                run->session->on_key(event.typed, at);
            }
        }

        // A timed run ends on the clock, not on a keystroke: if the script
        // stopped typing before the deadline, the deadline still arrives. The
        // mode is asked how much longer it has rather than being told — nothing
        // here knows what a duration is — and only a timed run has an answer.
        // A word count and a fixed text end when they are typed, and a zen run
        // ends when the user says so, so for those there is nothing to wait
        // for.
        if (!run->session->is_finished()) {
            const core::ModeProgress progress = run->session->progress();
            const auto* const timed = std::get_if<core::TimedProgress>(&progress);
            if (timed != nullptr && timed->remaining.value > 0) {
                const core::Millis last = script.events.empty() ? origin : origin + script.events.back().at;
                run->session->on_tick(last + timed->remaining);
            }
        }

        // A script that ran out before the mode finished is an abandoned run,
        // and is saved as one. That is what happens when somebody stops typing.
        const app::Outcome outcome = run->session->is_finished() ? app::Outcome::Completed : app::Outcome::Abandoned;

        const core::Result<app::SessionResult> result = service.finish(*run, outcome);
        if (!result) {
            return std::unexpected{result.error()};
        }
        return to_json(result->record);
    }

}  // namespace typeit::cli
