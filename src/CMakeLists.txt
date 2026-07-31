# Sources are listed, not globbed: a glob does not trigger a reconfigure, so a
# new file silently is not built until someone works out why.
#
# typeit_legacy exists only so the executable and the test binary stop
# compiling the same fifteen files twice. It is deleted whole at the Phase 4
# cutover (TI-097); nothing new should be added to it.
add_library(
    typeit_legacy STATIC
    core/FileTextSource.cpp
    core/Input.cpp
    core/Menu.cpp
    core/Screen.cpp
    core/Text.cpp
    core/Timer.cpp
    core/WordCalculator.cpp
    engines/InputAccuracyEngine.cpp
    engines/InputLineEngine.cpp
    engines/InputWordCountEngine.cpp
    engines/WordCalculatorEngine.cpp
    view/Main.cpp
    view/PerformanceArea.cpp
    view/SpeedTypingSession.cpp
    view/TextInputArea.cpp)

target_link_libraries(typeit_legacy PUBLIC ftxui::dom ftxui::screen ftxui::component)

typeit_set_warnings(typeit_legacy)
typeit_enable_sanitizers(typeit_legacy)
typeit_enable_static_analysis(typeit_legacy)

add_executable(TypeIt main.cpp)

target_link_libraries(TypeIt PRIVATE typeit_legacy)

typeit_set_warnings(TypeIt)
typeit_enable_sanitizers(TypeIt)
typeit_enable_static_analysis(TypeIt)
