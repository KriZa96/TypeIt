// Every setting the application has, as one value (TECHNICAL section 6).
//
// A value type with the documented defaults, so a `Config{}` is the program as
// it ships and a partially written config file is the defaults with a few
// fields moved. Reading TOML is `infra`'s job (Phase 2); what counts as a valid
// setting is a domain rule and lives next door in Validation.h.
//
// The enum-like fields are strings here rather than enums, deliberately: the
// config layer has to be able to hold what the file actually said in order to
// report it back, and "colour depth" is a rendering vocabulary this layer has
// no opinion about beyond which spellings exist.
#ifndef TYPEIT_CORE_CONFIG_CONFIG_H
#define TYPEIT_CORE_CONFIG_CONFIG_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

#include "typeit/core/session/TypingRules.h"

namespace typeit::core {

    struct GeneralConfig {
        std::string default_mode = "timed";
        std::int64_t default_duration_s = 30;
        std::int64_t default_word_count = 50;
        /// Zero disables the countdown.
        std::int64_t countdown_s = 3;
        bool confirm_quit = true;
        std::string log_level = "warn";
    };

    struct AppearanceConfig {
        std::string theme = "typeit-dark";
        std::string color_depth = "auto";
        std::string glyphs = "auto";
        std::string caret = "block";
        bool caret_blink = false;
        std::string layout = "comfortable";
        /// Graphemes per line. **Zero is valid** and means "fit the terminal",
        /// which is the setting most people want and the one a naive range
        /// check would reject.
        std::int64_t line_width = 0;
        std::int64_t lines_visible = 3;
        bool show_live_wpm = true;
        bool show_live_acc = true;
        bool show_progress = true;
        bool smooth_scroll = true;
    };

    struct TextConfig {
        bool flatten_typography = true;
        bool collapse_whitespace = true;
        bool strip_punctuation = false;
        bool lowercase = false;
        std::int64_t tab_width = 4;
        std::int64_t chunk_graphemes = 1'200;
    };

    struct RaceConfig {
        std::string preset = "standard";
        std::string start_policy = "from_history";
        std::int64_t start_wpm = 20;
        double ramp_up = 0.60;
        double ramp_down = 1.50;
        double min_accuracy = 0.92;
        std::int64_t lead_comfort = 25;
        std::int64_t lead_danger = 8;
        std::int64_t lead_scale = 30;
        std::int64_t grace_ms = 300;
        std::int64_t lives = 1;
        std::int64_t sustain_window_s = 10;
    };

    struct HistoryConfig {
        bool keep_keystroke_logs = false;
        /// Zero keeps everything.
        std::int64_t retention_days = 0;
    };

    struct NetworkConfig {
        bool enabled = false;
        bool allow_http = false;
        bool allow_private_addresses = false;
        std::int64_t timeout_s = 30;
        std::int64_t max_size_mb = 10;
        bool respect_robots_txt = true;
    };

    struct ImportConfig {
        bool dehyphenate = true;
        bool drop_running_heads = true;
        bool strip_footnotes = true;
        bool rejoin_paragraphs = true;
        bool report_unreachable_characters = true;
        /// MIME type (or `*`) to an argv-style command line. Never a shell, and
        /// never taken from anywhere but the user's own file (ADR-015). What
        /// makes a command *runnable* is infra's problem; this layer only
        /// carries it.
        std::map<std::string, std::string> converters;
    };

    struct Config {
        std::int64_t config_version = 1;
        GeneralConfig general;
        AppearanceConfig appearance;
        TypingRules typing;
        TextConfig text;
        RaceConfig race;
        HistoryConfig history;
        NetworkConfig network;
        ImportConfig import_;
        /// Action name to key spelling. Whether a spelling is a key the
        /// terminal can deliver is the keymap parser's question (Phase 4).
        std::map<std::string, std::string> keys;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_CONFIG_CONFIG_H
