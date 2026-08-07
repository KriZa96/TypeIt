# typeit::tui — the driving adapter (ADR-001, ARCHITECTURE section 4.4).
#
# FTXUI is PRIVATE, and the public header names none of it. That is two
# guarantees, not one: the link is private so a consumer inherits no include
# directories, and the header is clean so a consumer could not use them if it
# had them. `apps/typeit` is the only place FTXUI and SQLite are both visible,
# and it sees neither through a header of ours.
add_library(typeit_tui STATIC src/Bars.cpp src/Charts.cpp src/ColorQuantizer.cpp src/FrameTicker.cpp src/Layout.cpp src/Glyphs.cpp src/Heatmap.cpp src/Keymap.cpp src/ScreenStack.cpp src/TerminalApp.cpp src/TypingArea.cpp
                                src/screens/HelpScreen.cpp src/screens/HistoryScreen.cpp src/screens/MenuScreen.cpp
                                src/screens/ResultsScreen.cpp src/screens/SessionDetailScreen.cpp src/screens/SessionScreen.cpp
                                src/screens/TerminalTooSmallScreen.cpp src/ConsoleMode.cpp)
add_library(typeit::tui ALIAS typeit_tui)

# The public surface is `include/`, which names nothing from FTXUI. `src/`
# holds the headers that do — `IScreen` speaks in `ftxui::Element` — and is
# private: the composition root asks for an application, not for a rendering
# vocabulary. tests/tui reaches in deliberately, which is what a white-box test
# of a library's internals is.
target_include_directories(typeit_tui PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                                      PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(typeit_tui PUBLIC typeit::core typeit::app)
# typeit_version carries the generated header, privately: the session detail
# screen compares a stored run's version against this build's major, because
# metric definitions may differ across a MAJOR version (VERSIONING section 9),
# and nothing in this library's public headers needs to see it.
target_link_libraries(typeit_tui PRIVATE ftxui::dom ftxui::screen ftxui::component typeit_version)

target_compile_features(typeit_tui PUBLIC cxx_std_23)

typeit_set_warnings(typeit_tui)
typeit_enable_coverage(typeit_tui)
typeit_enable_sanitizers(typeit_tui)
typeit_enable_static_analysis(typeit_tui)
