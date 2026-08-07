#include "Keymap.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <ftxui/component/event.hpp>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/core/util/Result.h"

namespace typeit::tui {
    namespace {

        using core::ErrorCode;

        constexpr std::array<std::pair<std::string_view, Action>, 10> kActionNames{{
                {"quit_or_back", Action::QuitOrBack},
                {"force_quit", Action::ForceQuit},
                {"restart", Action::Restart},
                {"new_text", Action::NewText},
                {"menu", Action::Menu},
                {"history", Action::History},
                {"text_library", Action::TextLibrary},
                {"settings", Action::Settings},
                {"help", Action::Help},
                {"export", Action::Export},
        }};

        /// The named keys a binding may use, beside a single character. `f1`
        /// through `f12` are handled separately, being a family rather than a
        /// list.
        constexpr std::array<std::string_view, 10> kNamedKeys{"escape", "enter", "tab", "space",   "backspace",
                                                              "delete", "home",  "end", "page_up", "page_down"};

        std::string lowered(std::string_view text) {
            std::string out;
            out.reserve(text.size());
            for (const char letter: text) {
                out += static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
            }
            return out;
        }

        std::string_view trimmed(std::string_view text) {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
                text.remove_prefix(1);
            }
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
                text.remove_suffix(1);
            }
            return text;
        }

        bool is_function_key(std::string_view key) {
            if (key.size() < 2 || key.front() != 'f') {
                return false;
            }
            const std::string_view digits = key.substr(1);
            if (!std::ranges::all_of(digits, [](char digit) { return digit >= '0' && digit <= '9'; })) {
                return false;
            }
            const int number = std::stoi(std::string{digits});
            return number >= 1 && number <= 12;
        }

        bool is_known_key(std::string_view key) {
            if (key.size() == 1) {
                // A single printable character. Control characters are spelled
                // with a modifier, never written literally.
                const auto byte = static_cast<unsigned char>(key.front());
                return byte > 0x20 && byte < 0x7F;
            }
            return std::ranges::find(kNamedKeys, key) != kNamedKeys.end() || is_function_key(key);
        }

        /// The FTXUI event a named key arrives as.
        std::optional<ftxui::Event> named_event(std::string_view key) {
            if (key == "escape") {
                return ftxui::Event::Escape;
            }
            if (key == "enter") {
                return ftxui::Event::Return;
            }
            if (key == "tab") {
                return ftxui::Event::Tab;
            }
            if (key == "space") {
                return ftxui::Event::Character(' ');
            }
            if (key == "backspace") {
                return ftxui::Event::Backspace;
            }
            if (key == "delete") {
                return ftxui::Event::Delete;
            }
            if (key == "page_up") {
                return ftxui::Event::PageUp;
            }
            if (key == "page_down") {
                return ftxui::Event::PageDown;
            }
            if (key == "home") {
                return ftxui::Event::Home;
            }
            if (key == "end") {
                return ftxui::Event::End;
            }
            return std::nullopt;
        }

        std::optional<ftxui::Event> function_event(std::string_view key) {
            static const std::array<ftxui::Event, 12> kFunctionKeys{
                    ftxui::Event::F1, ftxui::Event::F2,  ftxui::Event::F3,  ftxui::Event::F4,
                    ftxui::Event::F5, ftxui::Event::F6,  ftxui::Event::F7,  ftxui::Event::F8,
                    ftxui::Event::F9, ftxui::Event::F10, ftxui::Event::F11, ftxui::Event::F12};
            if (!is_function_key(key)) {
                return std::nullopt;
            }
            const int number = std::stoi(std::string{key.substr(1)});
            return kFunctionKeys.at(static_cast<std::size_t>(number - 1));
        }

    }  // namespace

    std::string_view to_string(Action action) {
        for (const auto& [name, value]: kActionNames) {
            if (value == action) {
                return name;
            }
        }
        return "unknown";
    }

    bool action_from(std::string_view name, Action& out) {
        for (const auto& [known, value]: kActionNames) {
            if (known == name) {
                out = value;
                return true;
            }
        }
        return false;
    }

    core::Result<Binding> parse_binding(std::string_view text) {
        const std::string spelled = lowered(trimmed(text));
        if (spelled.empty()) {
            return core::fail(ErrorCode::InvalidKeyBinding, std::string{text});
        }

        Binding binding;
        std::string_view rest{spelled};

        // Modifiers, in any order and any number, each ending in a dash. The
        // key itself is what is left, which is why `ctrl-,` works without the
        // comma needing an escape.
        for (;;) {
            if (rest.starts_with("ctrl-")) {
                binding.ctrl = true;
                rest.remove_prefix(5);
                continue;
            }
            if (rest.starts_with("alt-")) {
                binding.alt = true;
                rest.remove_prefix(4);
                continue;
            }
            break;
        }

        if (!is_known_key(rest)) {
            return core::fail(ErrorCode::InvalidKeyBinding, std::string{text});
        }
        binding.key = rest;
        return binding;
    }

    std::string to_string(const Binding& binding) {
        std::string out;
        // A fixed order, so two spellings of one binding write back the same.
        if (binding.ctrl) {
            out += "ctrl-";
        }
        if (binding.alt) {
            out += "alt-";
        }
        out += binding.key;
        return out;
    }

    bool is_deliverable(const Binding& binding) {
        if (!binding.ctrl) {
            return true;
        }
        // A terminal encodes Ctrl by clearing the top bits of a letter, which
        // works for letters and for a handful of symbols and for nothing else.
        // `ctrl-,` is the documented example: GAMEPLAY §9 gives Settings an F2
        // fallback precisely because of it.
        if (binding.key.size() != 1) {
            return true;
        }
        const char key = binding.key.front();
        return (key >= 'a' && key <= 'z') || key == '[' || key == ']' || key == '\\' || key == '@' || key == '_';
    }

    bool matches(const Binding& binding, const ftxui::Event& event) {
        if (binding.ctrl) {
            if (binding.key.size() != 1) {
                return false;
            }
            const char key = binding.key.front();
            if (key < 'a' || key > 'z') {
                // Not something a terminal can encode; `is_deliverable` has
                // already said so, and matching it would be pretending.
                return false;
            }
            // Ctrl+A is 0x01 through Ctrl+Z at 0x1A.
            const auto control = static_cast<char>(key - 'a' + 1);
            return event == ftxui::Event::Special(std::string(1, control));
        }

        if (binding.alt) {
            // Escape then the key, which is how a terminal sends Alt.
            return event == ftxui::Event::Special("\x1b" + binding.key);
        }

        if (const std::optional<ftxui::Event> named = named_event(binding.key); named.has_value()) {
            return event == *named;
        }
        if (const std::optional<ftxui::Event> function = function_event(binding.key); function.has_value()) {
            return event == *function;
        }
        return event == ftxui::Event::Character(binding.key);
    }

    Binding default_binding(Action action) {
        // GAMEPLAY §9. Ctrl+T appears nowhere, deliberately: it is SIGINFO on
        // BSD, a tab key in several emulators, and the common tmux prefix.
        switch (action) {
            case Action::QuitOrBack:
                return Binding{.key = "escape", .ctrl = false, .alt = false};
            case Action::ForceQuit:
                return Binding{.key = "q", .ctrl = true, .alt = false};
            case Action::Restart:
                return Binding{.key = "r", .ctrl = true, .alt = false};
            case Action::NewText:
                return Binding{.key = "n", .ctrl = true, .alt = false};
            case Action::Menu:
                return Binding{.key = "escape", .ctrl = false, .alt = false};
            case Action::History:
                return Binding{.key = "h", .ctrl = true, .alt = false};
            case Action::TextLibrary:
                return Binding{.key = "l", .ctrl = true, .alt = false};
            case Action::Settings:
                // `ctrl-,` is what UX asks for and what most terminals cannot
                // send, so the shipped default is the documented fallback and
                // the unreachable one is left for somebody to opt into.
                return Binding{.key = "f2", .ctrl = false, .alt = false};
            case Action::Help:
                return Binding{.key = "f1", .ctrl = false, .alt = false};
            case Action::Export:
                return Binding{.key = "e", .ctrl = true, .alt = false};
        }
        return Binding{.key = "escape", .ctrl = false, .alt = false};
    }

    Keymap::Keymap() {
        for (const Action action: kAllActions) {
            bindings_[action] = default_binding(action);
        }
    }

    Keymap Keymap::from_config(const std::map<std::string, std::string>& keys) {
        Keymap keymap;

        for (const auto& [name, spelled]: keys) {
            Action action{};
            if (!action_from(name, action)) {
                std::string warning = "keys.";
                warning += name;
                warning += ": no such action; ignored";
                keymap.warnings_.push_back(std::move(warning));
                continue;
            }

            const core::Result<Binding> parsed = parse_binding(spelled);
            if (!parsed) {
                std::string warning = "keys.";
                warning += name;
                warning += " = \"";
                warning += spelled;
                warning += "\": could not be understood; using the default";
                keymap.warnings_.push_back(std::move(warning));
                continue;
            }

            // A duplicate names both actions, because "that key is taken" is
            // no use without saying by what. Only bindings the file itself set
            // count: colliding with a *default* is how somebody swaps two keys
            // over, and complaining about it would make that impossible.
            const auto clashing = std::ranges::find_if(keymap.bindings_, [&](const auto& entry) {
                return entry.first != action && entry.second == *parsed &&
                       keys.contains(std::string{to_string(entry.first)});
            });
            if (clashing != keymap.bindings_.end()) {
                std::string warning = "keys.";
                warning += name;
                warning += " = \"";
                warning += spelled;
                warning += "\": already bound to ";
                warning += to_string(clashing->first);
                warning += "; using the default";
                keymap.warnings_.push_back(std::move(warning));
                continue;
            }

            keymap.bindings_[action] = *parsed;
            if (!is_deliverable(*parsed)) {
                // Kept, not dropped: a terminal that *can* send it should still
                // work, and `--doctor` says which ones probably will not.
                std::string note = "keys.";
                note += name;
                note += " = \"";
                note += to_string(*parsed);
                note += "\": most terminals cannot send this";
                keymap.unreachable_.push_back(std::move(note));
            }
        }

        return keymap;
    }

    Binding Keymap::binding(Action action) const {
        const auto found = bindings_.find(action);
        return found == bindings_.end() ? default_binding(action) : found->second;
    }

    std::optional<Action> Keymap::action_for(const ftxui::Event& event) const {
        for (const auto& [action, binding]: bindings_) {
            if (matches(binding, event)) {
                return action;
            }
        }
        return std::nullopt;
    }

}  // namespace typeit::tui
