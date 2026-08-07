// What every screen needs, in one parameter (TI-090 – TI-094).
//
// A parameter object rather than four arguments on five constructors: the set
// is the same everywhere, and a screen added later would otherwise be a fifth
// place to forget one. Everything in it is borrowed and outlives the screens —
// the application owns all of it.
#ifndef TYPEIT_TUI_SCREENCONTEXT_H
#define TYPEIT_TUI_SCREENCONTEXT_H

#include <vector>

#include "Keymap.h"
#include "Layout.h"
#include "typeit/app/Capabilities.h"
#include "typeit/app/Theme.h"
#include "typeit/core/config/Config.h"
#include "typeit/tui/TerminalApp.h"

namespace typeit::tui {

    struct ScreenContext {
        const app::Theme* theme = nullptr;
        const Keymap* keymap = nullptr;
        const core::Config* config = nullptr;
        app::Capabilities capabilities;

        /// The texts on offer, owned by the application. Null or empty means
        /// there are none to choose between, and the menu says so rather than
        /// offering an empty list.
        const std::vector<TextChoice>* texts = nullptr;
        TextLoader load_text;
        TextWriter save_text;

        /// What the history screens read. Null throughout is a normal state,
        /// not an error: a layout test should not have to stand up a database,
        /// and every screen that reads history has an empty state anyway.
        HistorySource history;
        /// The terminal as last reported. A screen reads it rather than asking
        /// FTXUI, so every layout case is a value in a test.
        TerminalSize size;

        [[nodiscard]] Layout layout() const {
            return layout_for(size, static_cast<std::size_t>(config->appearance.line_width),
                              static_cast<std::size_t>(config->appearance.lines_visible),
                              density_from(config->appearance.layout));
        }
    };

}  // namespace typeit::tui

#endif  // TYPEIT_TUI_SCREENCONTEXT_H
