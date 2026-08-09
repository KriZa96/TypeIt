#include "typeit/infra/config/TomlConfigStore.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <toml++/toml.h>
#include <utility>
#include <vector>

#include "typeit/app/ports/IConfigStore.h"
#include "typeit/core/config/Config.h"
#include "typeit/core/config/Validation.h"
#include "typeit/core/session/TypingRules.h"
#include "typeit/core/util/Result.h"

namespace typeit::infra {
    namespace {

        using core::Config;
        using core::ErrorCode;
        using core::Result;
        using core::Status;

        /// A field of the configuration, as the file names it and as the value
        /// type it is. `read` moves the value from the file into the config;
        /// `write` renders it back with the comment that explains it.
        struct Loader {
            std::vector<std::string>* warnings;
            toml::table* file;
            Config* config;

            /// The keys this loader has looked for. Anything in the file that
            /// is not here gets a warning — forward compatibility means loading
            /// a file from a newer TypeIt, not pretending its extra keys are
            /// ours.
            std::set<std::string>* known;

            template<typename T, typename Accessor>
            void field(std::string_view key, Accessor access) {
                known->insert(std::string{key});

                const auto node = file->at_path(key);
                if (!node) {
                    return;  // Absent is the default, and the default is fine.
                }

                const std::optional<T> value = node.template value<T>();
                if (!value.has_value()) {
                    warnings->emplace_back(std::string{key} + ": wrong type for this setting; using the default");
                    return;
                }

                // Applied, then validated by core's own rules rather than by a
                // second copy of the ranges here. A range that lived in two
                // places would eventually disagree with itself.
                auto& target = access(*config);
                const auto previous = target;
                target = *value;
                if (const Status valid = core::validate(*config); !valid) {
                    warnings->emplace_back(std::string{key} + ": " + valid.error().context + "; using the default");
                    target = previous;
                }
            }

            /// A value the file spells as a word and the domain holds as an
            /// enum. Not `field`, because the two sides are different types.
            template<typename Enum, typename Parse, typename Accessor>
            void named(std::string_view key, Parse parse, Accessor access, std::string_view allowed) {
                known->insert(std::string{key});

                const auto node = file->at_path(key);
                if (!node) {
                    return;
                }
                if (const std::optional<Enum> value = parse(node.value_or(std::string{})); value.has_value()) {
                    access(*config) = *value;
                } else {
                    warnings->emplace_back(std::string{key} + ": expected one of: " + std::string{allowed} +
                                           "; using the default");
                }
            }

            /// A whole table of user-chosen keys — the key bindings and the
            /// converters. There is no fixed set to check them against, so
            /// every key in the table counts as known.
            void table(std::string_view key, std::map<std::string, std::string>& into) const {
                const auto node = file->at_path(key);
                if (!node) {
                    return;
                }
                const toml::table* entries = node.as_table();
                if (entries == nullptr) {
                    warnings->emplace_back(std::string{key} + ": expected a table; ignored");
                    return;
                }
                for (const auto& [name, value]: *entries) {
                    known->insert(std::string{key} + "." + std::string{name.str()});
                    into[std::string{name.str()}] = value.value_or(std::string{});
                }
            }
        };

        /// Every key the file contains, dotted, so unknown ones can be named.
        void collect_keys(const toml::table& table, const std::string& prefix, std::set<std::string>& into) {
            for (const auto& [key, value]: table) {
                const std::string path =
                        prefix.empty() ? std::string{key.str()} : prefix + "." + std::string{key.str()};
                if (const toml::table* child = value.as_table(); child != nullptr) {
                    collect_keys(*child, path, into);
                } else {
                    into.insert(path);
                }
            }
        }

        std::string escape(std::string_view value) {
            std::string escaped;
            for (const char character: value) {
                if (character == '"' || character == '\\') {
                    escaped += '\\';
                }
                escaped += character;
            }
            return escaped;
        }

        std::string quoted(std::string_view value) { return "\"" + escape(value) + "\""; }

        std::string boolean(bool value) { return value ? "true" : "false"; }

        void load_every_setting(Loader& loader) {
            loader.field<std::int64_t>("config_version", [](Config& c) -> auto& { return c.config_version; });

            loader.field<std::string>("general.default_mode",
                                      [](Config& c) -> auto& { return c.general.default_mode; });
            loader.field<std::int64_t>("general.default_duration_s",
                                       [](Config& c) -> auto& { return c.general.default_duration_s; });
            loader.field<std::int64_t>("general.default_word_count",
                                       [](Config& c) -> auto& { return c.general.default_word_count; });
            loader.field<std::int64_t>("general.countdown_s", [](Config& c) -> auto& { return c.general.countdown_s; });
            loader.field<bool>("general.confirm_quit", [](Config& c) -> auto& { return c.general.confirm_quit; });
            loader.field<std::string>("general.log_level", [](Config& c) -> auto& { return c.general.log_level; });

            loader.field<std::string>("appearance.theme", [](Config& c) -> auto& { return c.appearance.theme; });
            loader.field<std::string>("appearance.color_depth",
                                      [](Config& c) -> auto& { return c.appearance.color_depth; });
            loader.field<std::string>("appearance.glyphs", [](Config& c) -> auto& { return c.appearance.glyphs; });
            loader.field<std::string>("appearance.caret", [](Config& c) -> auto& { return c.appearance.caret; });
            loader.field<bool>("appearance.caret_blink", [](Config& c) -> auto& { return c.appearance.caret_blink; });
            loader.field<std::string>("appearance.layout", [](Config& c) -> auto& { return c.appearance.layout; });
            loader.field<std::int64_t>("appearance.line_width",
                                       [](Config& c) -> auto& { return c.appearance.line_width; });
            loader.field<std::int64_t>("appearance.lines_visible",
                                       [](Config& c) -> auto& { return c.appearance.lines_visible; });
            loader.field<bool>("appearance.show_live_wpm",
                               [](Config& c) -> auto& { return c.appearance.show_live_wpm; });
            loader.field<bool>("appearance.show_live_acc",
                               [](Config& c) -> auto& { return c.appearance.show_live_acc; });
            loader.field<bool>("appearance.show_progress",
                               [](Config& c) -> auto& { return c.appearance.show_progress; });
            loader.field<bool>("appearance.smooth_scroll",
                               [](Config& c) -> auto& { return c.appearance.smooth_scroll; });

            loader.field<bool>("typing.allow_backspace", [](Config& c) -> auto& { return c.typing.allow_backspace; });
            loader.field<bool>("typing.strict_spaces", [](Config& c) -> auto& { return c.typing.strict_spaces; });
            loader.field<bool>("typing.space_advances_word",
                               [](Config& c) -> auto& { return c.typing.space_advances_word; });
            loader.field<bool>("typing.blind_mode", [](Config& c) -> auto& { return c.typing.blind_mode; });

            loader.field<bool>("text.flatten_typography", [](Config& c) -> auto& { return c.text.flatten_typography; });
            loader.field<bool>("text.collapse_whitespace",
                               [](Config& c) -> auto& { return c.text.collapse_whitespace; });
            loader.field<bool>("text.strip_punctuation", [](Config& c) -> auto& { return c.text.strip_punctuation; });
            loader.field<bool>("text.lowercase", [](Config& c) -> auto& { return c.text.lowercase; });
            loader.field<std::int64_t>("text.tab_width", [](Config& c) -> auto& { return c.text.tab_width; });
            loader.field<std::int64_t>("text.chunk_graphemes",
                                       [](Config& c) -> auto& { return c.text.chunk_graphemes; });

            loader.field<std::string>("race.preset", [](Config& c) -> auto& { return c.race.preset; });
            loader.field<std::string>("race.start_policy", [](Config& c) -> auto& { return c.race.start_policy; });
            loader.field<std::int64_t>("race.start_wpm", [](Config& c) -> auto& { return c.race.start_wpm; });
            loader.field<double>("race.ramp_up", [](Config& c) -> auto& { return c.race.ramp_up; });
            loader.field<double>("race.ramp_down", [](Config& c) -> auto& { return c.race.ramp_down; });
            loader.field<double>("race.min_accuracy", [](Config& c) -> auto& { return c.race.min_accuracy; });
            loader.field<std::int64_t>("race.lead_comfort", [](Config& c) -> auto& { return c.race.lead_comfort; });
            loader.field<std::int64_t>("race.lead_danger", [](Config& c) -> auto& { return c.race.lead_danger; });
            loader.field<std::int64_t>("race.lead_scale", [](Config& c) -> auto& { return c.race.lead_scale; });
            loader.field<std::int64_t>("race.grace_ms", [](Config& c) -> auto& { return c.race.grace_ms; });
            loader.field<std::int64_t>("race.lives", [](Config& c) -> auto& { return c.race.lives; });
            loader.field<std::int64_t>("race.sustain_window_s",
                                       [](Config& c) -> auto& { return c.race.sustain_window_s; });
            loader.field<double>("race.catch_penalty", [](Config& c) -> auto& { return c.race.catch_penalty; });
            loader.field<double>("race.start_factor", [](Config& c) -> auto& { return c.race.start_factor; });

            loader.field<bool>("history.keep_keystroke_logs",
                               [](Config& c) -> auto& { return c.history.keep_keystroke_logs; });
            loader.field<std::int64_t>("history.retention_days",
                                       [](Config& c) -> auto& { return c.history.retention_days; });

            loader.field<std::int64_t>("goals.daily_minutes", [](Config& c) -> auto& { return c.goals.daily_minutes; });
            loader.field<std::int64_t>("goals.daily_runs", [](Config& c) -> auto& { return c.goals.daily_runs; });

            loader.field<bool>("network.enabled", [](Config& c) -> auto& { return c.network.enabled; });
            loader.field<bool>("network.allow_http", [](Config& c) -> auto& { return c.network.allow_http; });
            loader.field<bool>("network.allow_private_addresses",
                               [](Config& c) -> auto& { return c.network.allow_private_addresses; });
            loader.field<std::int64_t>("network.timeout_s", [](Config& c) -> auto& { return c.network.timeout_s; });
            loader.field<std::int64_t>("network.max_size_mb", [](Config& c) -> auto& { return c.network.max_size_mb; });
            loader.field<bool>("network.respect_robots_txt",
                               [](Config& c) -> auto& { return c.network.respect_robots_txt; });

            loader.field<bool>("import.dehyphenate", [](Config& c) -> auto& { return c.import_.dehyphenate; });
            loader.field<bool>("import.drop_running_heads",
                               [](Config& c) -> auto& { return c.import_.drop_running_heads; });
            loader.field<bool>("import.strip_footnotes", [](Config& c) -> auto& { return c.import_.strip_footnotes; });
            loader.field<bool>("import.rejoin_paragraphs",
                               [](Config& c) -> auto& { return c.import_.rejoin_paragraphs; });
            loader.field<bool>("import.report_unreachable_characters",
                               [](Config& c) -> auto& { return c.import_.report_unreachable_characters; });

            // The enum-like fields and the two open tables need their own
            // helpers: the first are words in the file and enums in the domain,
            // and the second have no fixed set of keys to check against.
            loader.named<core::StopOnError>(
                    "typing.stop_on_error", core::stop_on_error_from,
                    [](Config& c) -> auto& { return c.typing.stop_on_error; }, "off, letter, word");
            loader.named<core::ConfidenceMode>(
                    "typing.confidence_mode", core::confidence_mode_from,
                    [](Config& c) -> auto& { return c.typing.confidence_mode; }, "off, on, max");

            loader.table("keys", loader.config->keys);
            loader.table("import.converters", loader.config->import_.converters);
        }

    }  // namespace

    Result<app::LoadedConfig> TomlConfigStore::load() {
        app::LoadedConfig loaded;

        std::error_code ignored;
        if (!std::filesystem::exists(file_, ignored)) {
            // A first run. The defaults are the answer, and the template is
            // written so the file can be found and edited.
            if (const Status written = save(loaded.config); !written) {
                loaded.warnings.push_back("could not write the default configuration: " + written.error().context);
            }
            return loaded;
        }

        toml::table file;
        try {
            file = toml::parse_file(file_.string());
        } catch (const toml::parse_error& failure) {
            // Reported with the line, and the file is left exactly as it is.
            // Somebody hand-wrote it; losing it because we could not read it
            // would be the worst thing this class could do.
            const auto& where = failure.source();
            return core::fail(ErrorCode::ConfigParse, file_.string() + ":" + std::to_string(where.begin.line) + ":" +
                                                              std::to_string(where.begin.column) + ": " +
                                                              std::string{failure.description()});
        }

        std::set<std::string> known;
        Loader loader{.warnings = &loaded.warnings, .file = &file, .config = &loaded.config, .known = &known};

        load_every_setting(loader);

        // Anything left is from a newer TypeIt, or a typo. Either way the file
        // loads: a warning is a message, not a refusal.
        std::set<std::string> present;
        collect_keys(file, "", present);
        for (const std::string& key: present) {
            if (!known.contains(key)) {
                loaded.warnings.emplace_back(key + ": unknown setting; ignored");
            }
        }

        return loaded;
    }

    Status TomlConfigStore::save(const Config& config) {
        std::error_code failure;
        std::filesystem::create_directories(file_.parent_path(), failure);
        if (failure) {
            return core::fail(ErrorCode::FileUnreadable, file_.parent_path().string() + ": " + failure.message());
        }

        // Written beside the target and renamed over it. A rename within one
        // directory is atomic on every filesystem this runs on, so a crash
        // mid-write leaves the old file whole rather than a truncated new one.
        const std::filesystem::path temporary = file_.string() + ".tmp";
        {
            std::ofstream out{temporary, std::ios::binary | std::ios::trunc};
            if (!out) {
                return core::fail(ErrorCode::FileUnreadable, temporary.string() + ": cannot write");
            }
            out << render(config);
            out.flush();
            if (!out) {
                return core::fail(ErrorCode::FileUnreadable, temporary.string() + ": write failed");
            }
        }

        std::filesystem::rename(temporary, file_, failure);
        if (failure) {
            std::filesystem::remove(temporary, failure);
            return core::fail(ErrorCode::FileUnreadable, file_.string() + ": " + failure.message());
        }
        return {};
    }

    std::string TomlConfigStore::render(const Config& config) {
        std::ostringstream out;
        out << "# TypeIt configuration. Delete any key to fall back to its default.\n";
        out << "config_version = " << config.config_version << "\n\n";

        out << "[general]\n";
        out << "default_mode       = " << quoted(config.general.default_mode)
            << "       # timed | words | quote | zen | endless | race\n";
        out << "default_duration_s = " << config.general.default_duration_s << "\n";
        out << "default_word_count = " << config.general.default_word_count << "\n";
        out << "countdown_s        = " << config.general.countdown_s << "             # 0 disables\n";
        out << "confirm_quit       = " << boolean(config.general.confirm_quit) << "\n";
        out << "log_level          = " << quoted(config.general.log_level)
            << "         # off | error | warn | info | debug\n\n";

        out << "[appearance]\n";
        out << "theme          = " << quoted(config.appearance.theme)
            << "     # builtin name, or a path to a .toml theme\n";
        out << "color_depth    = " << quoted(config.appearance.color_depth)
            << "            # auto | truecolor | 256 | 16 | mono\n";
        out << "glyphs         = " << quoted(config.appearance.glyphs) << "            # auto | unicode | ascii\n";
        out << "caret          = " << quoted(config.appearance.caret)
            << "           # block | underline | outline | none\n";
        out << "caret_blink    = " << boolean(config.appearance.caret_blink) << "\n";
        out << "layout         = " << quoted(config.appearance.layout) << "     # compact | comfortable\n";
        out << "line_width     = " << config.appearance.line_width
            << "                 # graphemes per line; 0 = fit the terminal\n";
        out << "lines_visible  = " << config.appearance.lines_visible << "\n";
        out << "show_live_wpm  = " << boolean(config.appearance.show_live_wpm) << "\n";
        out << "show_live_acc  = " << boolean(config.appearance.show_live_acc) << "\n";
        out << "show_progress  = " << boolean(config.appearance.show_progress) << "\n";
        out << "smooth_scroll  = " << boolean(config.appearance.smooth_scroll) << "\n\n";

        out << "[typing]\n";
        out << "stop_on_error       = " << quoted(core::to_string(config.typing.stop_on_error))
            << "        # off | letter | word\n";
        out << "allow_backspace     = " << boolean(config.typing.allow_backspace) << "\n";
        out << "strict_spaces       = " << boolean(config.typing.strict_spaces) << "\n";
        out << "space_advances_word = " << boolean(config.typing.space_advances_word) << "\n";
        out << "blind_mode          = " << boolean(config.typing.blind_mode) << "\n";
        out << "confidence_mode     = " << quoted(core::to_string(config.typing.confidence_mode))
            << "        # off | on | max\n\n";

        out << "[text]\n";
        out << "flatten_typography  = " << boolean(config.text.flatten_typography)
            << "          # smart quotes/dashes to ASCII\n";
        out << "collapse_whitespace = " << boolean(config.text.collapse_whitespace) << "\n";
        out << "strip_punctuation   = " << boolean(config.text.strip_punctuation) << "\n";
        out << "lowercase           = " << boolean(config.text.lowercase) << "\n";
        out << "tab_width           = " << config.text.tab_width << "\n";
        out << "chunk_graphemes     = " << config.text.chunk_graphemes << "          # for long documents\n\n";

        out << "[race]\n";
        out << "preset             = " << quoted(config.race.preset) << "    # gentle | standard | brutal | custom\n";
        out << "start_policy       = " << quoted(config.race.start_policy) << "  # from_history | fixed\n";
        out << "start_wpm          = " << config.race.start_wpm << "\n";
        out << "ramp_up            = " << config.race.ramp_up << "          # WPM gained per second at full lead\n";
        out << "ramp_down          = " << config.race.ramp_down << "          # WPM lost per second when struggling\n";
        out << "min_accuracy       = " << config.race.min_accuracy << "\n";
        out << "lead_comfort       = " << config.race.lead_comfort << "            # graphemes\n";
        out << "lead_danger        = " << config.race.lead_danger << "\n";
        out << "lead_scale         = " << config.race.lead_scale << "\n";
        out << "grace_ms           = " << config.race.grace_ms << "\n";
        out << "lives              = " << config.race.lives << "\n";
        out << "sustain_window_s   = " << config.race.sustain_window_s << "\n";
        out << "catch_penalty      = " << config.race.catch_penalty << "           # speed lost on being caught\n";
        out << "start_factor       = " << config.race.start_factor
            << "          # fraction of your sustained best the next race starts at\n\n";

        out << "[history]\n";
        out << "keep_keystroke_logs = " << boolean(config.history.keep_keystroke_logs)
            << "        # enables exact replay; grows the database\n";
        out << "retention_days      = " << config.history.retention_days << "            # 0 = keep everything\n\n";

        out << "[goals]\n";
        out << "daily_minutes = " << config.goals.daily_minutes << "            # either bar clears the day\n";
        out << "daily_runs    = " << config.goals.daily_runs << "             # 0 on both = any run counts\n\n";

        out << "[network]\n";
        out << "enabled      = " << boolean(config.network.enabled) << "\n";
        out << "allow_http   = " << boolean(config.network.allow_http) << "            # HTTPS only by default\n";
        out << "allow_private_addresses = " << boolean(config.network.allow_private_addresses) << "\n";
        out << "timeout_s    = " << config.network.timeout_s << "\n";
        out << "max_size_mb  = " << config.network.max_size_mb << "\n";
        out << "respect_robots_txt = " << boolean(config.network.respect_robots_txt) << "\n\n";

        out << "[import]\n";
        out << "dehyphenate        = " << boolean(config.import_.dehyphenate) << "\n";
        out << "drop_running_heads = " << boolean(config.import_.drop_running_heads) << "\n";
        out << "strip_footnotes    = " << boolean(config.import_.strip_footnotes) << "\n";
        out << "rejoin_paragraphs  = " << boolean(config.import_.rejoin_paragraphs) << "\n";
        out << "report_unreachable_characters = " << boolean(config.import_.report_unreachable_characters) << "\n\n";

        out << "[import.converters]\n";
        if (config.import_.converters.empty()) {
            out << "# \"application/pdf\" = \"pdftotext -layout -nopgbrk {input} -\"\n";
            out << "# \"*\"               = \"pandoc --to=plain --wrap=none {input}\"\n";
        }
        for (const auto& [mime, command]: config.import_.converters) {
            out << quoted(mime) << " = " << quoted(command) << "\n";
        }
        out << "\n[keys]\n";
        for (const auto& [action, binding]: config.keys) {
            out << action << " = " << quoted(binding) << "\n";
        }

        return out.str();
    }

}  // namespace typeit::infra
