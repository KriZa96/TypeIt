# Sources are listed, not globbed: a glob does not trigger a reconfigure, so a
# new file silently is not built until someone works out why.
set(TYPEIT_LEGACY_SOURCES
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

list(TRANSFORM TYPEIT_LEGACY_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
set(TYPEIT_LEGACY_SOURCES
    ${TYPEIT_LEGACY_SOURCES}
    PARENT_SCOPE)

add_executable(TypeIt main.cpp ${TYPEIT_LEGACY_SOURCES})

target_link_libraries(TypeIt PRIVATE ftxui::dom ftxui::screen ftxui::component)

typeit_set_warnings(TypeIt)
