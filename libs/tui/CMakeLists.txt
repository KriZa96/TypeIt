# typeit::tui — the driving adapter (ADR-001, ARCHITECTURE section 4.4).
#
# FTXUI is PRIVATE, and the public header names none of it. That is two
# guarantees, not one: the link is private so a consumer inherits no include
# directories, and the header is clean so a consumer could not use them if it
# had them. `apps/typeit` is the only place FTXUI and SQLite are both visible,
# and it sees neither through a header of ours.
add_library(typeit_tui STATIC src/FrameTicker.cpp src/TerminalApp.cpp)
add_library(typeit::tui ALIAS typeit_tui)

target_include_directories(typeit_tui PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")

target_link_libraries(typeit_tui PUBLIC typeit::core typeit::app)
target_link_libraries(typeit_tui PRIVATE ftxui::dom ftxui::screen ftxui::component)

target_compile_features(typeit_tui PUBLIC cxx_std_23)

typeit_set_warnings(typeit_tui)
typeit_enable_coverage(typeit_tui)
typeit_enable_sanitizers(typeit_tui)
typeit_enable_static_analysis(typeit_tui)
