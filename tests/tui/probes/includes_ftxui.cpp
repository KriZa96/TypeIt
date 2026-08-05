// FTXUI is linked PRIVATE, so a consumer of typeit::tui inherits none of its
// include directories. This must not compile: the point of the private link is
// that the terminal library stops at this library's edge.
#include <ftxui/dom/elements.hpp>

int uses_ftxui() { return ftxui::text("") ? 1 : 0; }
