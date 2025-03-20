//
// Created by kil3 on 3/12/25.
//

#include "../../include/core/Menu.h"

#include "../../include/core/FileTextSource.h"
#include "../../include/core/Screen.h"
#include "../../include/data/GameOptions.h"
#include "../../include/data/GameState.h"
#include "../../include/data/Style.h"


Menu::Menu(ftxui::ScreenInteractive& screen):
screen_(screen),
text_radiobox_choice_({"simple", "medium", "hard", "custom"}),
time_radiobox_choice_({"15s", "30s", "60s"}),
path_input_(ftxui::Input(&GameOptions::text_radiobox_values_[3], "Path to file", input_option_)),
time_radiobox_(ftxui::Radiobox(&time_radiobox_choice_, &GameOptions::selected_radiobox_time_, radiobox_option_)),
text_radiobox_(ftxui::Radiobox(&text_radiobox_choice_, &GameOptions::selected_radiobox_text_, radiobox_option_)),
start_button_(ftxui::Button("Start", [this] {
    std::string path = GameOptions::text_radiobox_values_[GameOptions::selected_radiobox_text_];
    if (FileTextSource::is_text_file(path) && FileTextSource::has_text_stream(path)) {
        GameState::refresh_session_ = not GameState::refresh_session_;
    }}, ftxui::ButtonOption::Ascii())),
exit_button_(ftxui::Button("Exit", [this] {screen_.ExitLoopClosure(); screen_.Exit();}, ftxui::ButtonOption::Ascii())),
info_button_(ftxui::Button("Info", [this] {GameState::show_info_ = not GameState::show_info_;}, ftxui::ButtonOption::Ascii())),
menu_container_(get_menu_container_()),
radiobox_option_({
    .transform = [&](const ftxui::EntryState& state) {
        #if defined(FTXUI_MICROSOFT_TERMINAL_FALLBACK)
            auto prefix = ftxui::text(state.state ? "(*) " : "( ) ") | ftxui::color(ftxui::Color::Grey85);
        #else
            auto prefix = ftxui::text(state.state ? "◉ " : "○ ") | ftxui::color(ftxui::Color::Grey85);
        #endif
            auto t = ftxui::text(state.label) | ftxui::color(ftxui::Color::Grey85);
            if (state.focused) {
                t |= ftxui::inverted;
            }
            return ftxui::hbox({prefix, t});
    }
}),
input_option_({.transform = [&](ftxui::InputState state) {
    state.element |= ftxui::border;

    if (state.hovered || state.focused) {
        std::string path = GameOptions::text_radiobox_values_[GameOptions::selected_radiobox_text_];
        if (FileTextSource::is_text_file(path) && FileTextSource::has_text_stream(path)) {
            state.element |= Style::input_text_color_good;
        }
        else {
            state.element |= Style::input_text_color_bad;
        }
    }
    else {
        state.element |= Style::underlying_text_color;
    }

    return state.element;
  }})
{}



ftxui::Component Menu::get_menu_component() const {
    return ftxui::Renderer(
            menu_container_,
            [this] {
                return menu_container_->Render();
            }
    );
}

ftxui::Component Menu::get_time_radiobox_component_() const {
    return ftxui::Renderer(
        time_radiobox_,
        [this] {
            return ftxui::window(ftxui::text("Time"), time_radiobox_->Render());
        }
    );
}

ftxui::Component Menu::get_text_radiobox_component_() const {
    return ftxui::Renderer(
        text_radiobox_,
        [this] {
            return ftxui::window(ftxui::text("Text"), text_radiobox_->Render());
        }
    );
}

ftxui::Component Menu::get_info_component_() const {
    return ftxui::Maybe(ftxui::Renderer(
        [&] {
            return ftxui::window(
                ftxui::text("Info"),
                ftxui::paragraph(
                "If you want to use custom file, when inputting the file path"
                    ", on valid input color will change from salmon to white/gray."
                )
            );
        }
    ), &GameState::show_info_);
}

ftxui::Component Menu::get_menu_container_() const {
    return ftxui::Container::Vertical(
            {
                get_info_component_(),
                ftxui::Container::Horizontal({info_button_}) | ftxui::center,
                get_text_radiobox_component_(),
                ftxui::Maybe(path_input_, [&] {return GameOptions::selected_radiobox_text_ == 3;}),
                get_time_radiobox_component_(),
                ftxui::Container::Horizontal(
                    {
                        start_button_,
                        exit_button_
                    }
                ) | ftxui::center
            }
        ) | Style::menu_style;
}