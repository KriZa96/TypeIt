// Every action rebindable, and every binding validated (TI-086, GAMEPLAY §9).
//
// Two halves. A **binding** is a key as a person writes it — `ctrl-q`,
// `escape`, `f1`, `?` — parsed once, at config load, so a typo is a message
// with the offending string in it rather than a key that silently does
// nothing. A **keymap** is the whole set: every action bound to something,
// with the defaults filled in and duplicates refused.
//
// `Ctrl+T` is not a default anywhere, deliberately: it is `SIGINFO` on BSD, a
// tab key in several emulators, and collides with the common tmux prefix.
// There is a test whose only job is to keep it that way.
//
// Bindings a terminal cannot deliver — `ctrl-,` on most of them — are **not**
// silently dropped. They are recorded as unreachable so `--doctor` can say so,
// and the action keeps its fallback.
#ifndef TYPEIT_TUI_KEYMAP_H
#define TYPEIT_TUI_KEYMAP_H

#include <array>
#include <cstdint>
#include <ftxui/component/event.hpp>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/core/util/Result.h"

namespace typeit::tui {

    /// Everything a key can be bound to. The enum is the vocabulary: an action
    /// nobody can name is an action nobody can rebind.
    enum class Action : std::uint8_t {
        QuitOrBack,
        ForceQuit,
        Restart,
        NewText,
        Menu,
        History,
        TextLibrary,
        Settings,
        Help,
    };

    inline constexpr std::array<Action, 9> kAllActions{
            Action::QuitOrBack, Action::ForceQuit,   Action::Restart,  Action::NewText, Action::Menu,
            Action::History,    Action::TextLibrary, Action::Settings, Action::Help,
    };

    /// The name the configuration file uses under `[keys]`.
    [[nodiscard]] std::string_view to_string(Action action);
    [[nodiscard]] bool action_from(std::string_view name, Action& out);

    /// One key, parsed. Modifiers are flags rather than part of the name, so
    /// `ctrl-q` and `Ctrl-Q` are the same binding and compare equal.
    struct Binding {
        /// The key itself, lower case and canonical: `q`, `escape`, `f1`, `,`.
        std::string key;
        bool ctrl = false;
        bool alt = false;

        friend bool operator==(const Binding&, const Binding&) = default;
    };

    /// `ctrl-q`, `escape`, `f1`, `alt-x`, `?`, `ctrl-,`.
    ///
    /// Case-insensitive and tolerant of whitespace around the parts, because a
    /// configuration file is written by a person. Fails with
    /// `ErrorCode::InvalidKeyBinding`, naming the offending string, on anything
    /// else — a binding that cannot be parsed must not become a key that does
    /// nothing.
    [[nodiscard]] core::Result<Binding> parse_binding(std::string_view text);

    /// The canonical spelling: modifiers in a fixed order, lower case. Parsing
    /// this again gives the same binding, which is what lets a settings screen
    /// write back what it read.
    [[nodiscard]] std::string to_string(const Binding& binding);

    /// Whether a terminal can be expected to deliver this. `ctrl-,` and
    /// `ctrl-;` cannot be encoded by most, and a binding nobody can press is
    /// worth reporting rather than leaving somebody to wonder.
    [[nodiscard]] bool is_deliverable(const Binding& binding);

    /// Whether an FTXUI event is this binding.
    [[nodiscard]] bool matches(const Binding& binding, const ftxui::Event& event);

    /// The bindings GAMEPLAY §9 documents.
    [[nodiscard]] Binding default_binding(Action action);

    /// A whole keymap: every action bound, with what went wrong on the way.
    class Keymap {
    public:
        /// Every action at its default.
        Keymap();

        /// The defaults, overridden by `[keys]` from the configuration.
        ///
        /// Never fails. An unparseable binding, a duplicate, or a name nobody
        /// recognises is a warning and the default stands — a keymap that
        /// refused to load would leave the program with no keys at all, which
        /// is worse than one wrong key.
        [[nodiscard]] static Keymap from_config(const std::map<std::string, std::string>& keys);

        [[nodiscard]] Binding binding(Action action) const;

        /// The action a key press means, or nothing.
        [[nodiscard]] std::optional<Action> action_for(const ftxui::Event& event) const;

        /// What was wrong with the configuration. Empty after a clean load.
        [[nodiscard]] const std::vector<std::string>& warnings() const noexcept { return warnings_; }

        /// Bindings this terminal is not expected to deliver, for `--doctor`.
        /// Not an error: the binding is kept, because a terminal that *can*
        /// send it should still work.
        [[nodiscard]] const std::vector<std::string>& unreachable() const noexcept { return unreachable_; }

    private:
        std::map<Action, Binding> bindings_;
        std::vector<std::string> warnings_;
        std::vector<std::string> unreachable_;
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_KEYMAP_H
