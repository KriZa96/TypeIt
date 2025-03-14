//
// Created by kil3 on 3/12/25.
//

#include "../../include/Menu.h"

ftxui::Component Menu::get_menu_component() {
    return ftxui::Renderer(
            menu_container_,
            [this] {
                return menu_container_->Render();
            }
    );
}