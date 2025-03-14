//
// Created by kil3 on 3/12/25.
//

#ifndef MENU_H
#define MENU_H

#include "ftxui/component/component.hpp"

#include "./GameOptions.h"


class Menu {
    public:
        ftxui::Component get_menu_component();
    private:
        std::vector<std::string> text_radiobox_choice_{"simple", "medium", "hard", "custom"};
        std::vector<std::string> time_radiobox_choice_{"15s", "30s", "60s"};

        ftxui::RadioboxOption radiobox_option_{
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
        };

        ftxui::Component time_radiobox_ = ftxui::Radiobox(&time_radiobox_choice_, &GameOptions::selected_radiobox_time_, radiobox_option_);
        ftxui::Component text_radiobox_ = ftxui::Radiobox(&text_radiobox_choice_, &GameOptions::selected_radiobox_text_, radiobox_option_);
        ftxui::Component start_button_ = ftxui::Button("Start", [this] {GameOptions::start_game_ = not GameOptions::start_game_;}, ftxui::ButtonOption::Ascii()) ;
        ftxui::Component exit_button_ = ftxui::Button("Exit", [this] {GameOptions::start_game_ = not GameOptions::start_game_;}, ftxui::ButtonOption::Ascii()) ;

        ftxui::Component get_time_radiobox_component_() {
            return ftxui::Renderer(
                time_radiobox_,
                [this] {
                    return ftxui::window(ftxui::text("Time"), time_radiobox_->Render());
                }
            );
        }
        ftxui::Component get_text_radiobox_component_() {
            return ftxui::Renderer(
                text_radiobox_,
                [this] {
                    return ftxui::window(ftxui::text("Text"), text_radiobox_->Render());
                }
            );
        }
        ftxui::Component get_start_button_component_() {
            return ftxui::Renderer(
                start_button_,
                [this] {
                    return ftxui::hbox(start_button_->Render());
                }
            );
        }
        ftxui::Component get_exit_button_component_() {
            return ftxui::Renderer(
                exit_button_,
                [this] {
                    return ftxui::hbox(exit_button_->Render());
                }
            );
        }

            // TODO moyda netrbaju sve metode za komponente, treba probati bez
        ftxui::Component menu_container_ = ftxui::Container::Vertical(
            {
                get_text_radiobox_component_(),
                get_time_radiobox_component_(),
                ftxui::Container::Horizontal(
                    {
                        start_button_,
                        exit_button_
                    }
                ) | ftxui::center
            }
        ) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 15) | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 20) | ftxui::center;


};



#endif //MENU_H
