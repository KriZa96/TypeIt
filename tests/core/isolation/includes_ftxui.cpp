// This must NOT compile with typeit::core's include directories. If it ever
// does, the domain has grown a dependency on the terminal library and ADR-001
// is enforced by discipline alone again.
#include <ftxui/dom/elements.hpp>

int uses_ftxui() { return ftxui::text("") ? 1 : 0; }
