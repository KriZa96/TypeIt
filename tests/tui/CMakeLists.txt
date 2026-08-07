# The driving adapter's tests, and the probes that prove what it cannot reach.
#
# Same arrangement as tests/core's isolation probes: real targets in the real
# build graph, because try_compile reaches neither an in-project target's
# transitive usage requirements nor the include directories read off it, and so
# reports a clean bill of health for a library that links everything.
if(TYPEIT_BUILD_PROBES)
    add_library(tui_isolation_probe STATIC EXCLUDE_FROM_ALL probes/includes_infra.cpp)
    target_link_libraries(tui_isolation_probe PRIVATE typeit::tui)

    # FTXUI is linked PRIVATE, so a consumer inherits none of its include
    # directories. This is the check that the private link is real rather than
    # a comment.
    add_library(tui_ftxui_probe STATIC EXCLUDE_FROM_ALL probes/includes_ftxui.cpp)
    target_link_libraries(tui_ftxui_probe PRIVATE typeit::tui)

    add_library(tui_isolation_control STATIC EXCLUDE_FROM_ALL probes/includes_stdlib.cpp)
    target_link_libraries(tui_isolation_control PRIVATE typeit::tui)

    # And out of compile_commands.json, for the analysers that read that
    # instead of the build system.
    set_target_properties(tui_isolation_probe tui_ftxui_probe PROPERTIES EXPORT_COMPILE_COMMANDS OFF)

    # The control comes first in name so a broken harness is obvious.
    add_test(NAME tui.isolation.control
             COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target tui_isolation_control)

    add_test(NAME tui.isolation.infra_is_unreachable
             COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target tui_isolation_probe)

    add_test(NAME tui.isolation.ftxui_does_not_leak
             COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target tui_ftxui_probe)

    set_tests_properties(tui.isolation.infra_is_unreachable tui.isolation.ftxui_does_not_leak
                         PROPERTIES WILL_FAIL TRUE)
endif()

add_executable(tui_tests ColorQuantizerTest.cpp FrameTickerTest.cpp GlyphSetTest.cpp
                         HeatmapTest.cpp HistogramTest.cpp KeymapTest.cpp
                         LineChartTest.cpp
                         LayoutTest.cpp SparklineTest.cpp StatsBarTest.cpp
                         ScreensTest.cpp ScreenStackTest.cpp Snapshot.cpp TerminalAppTest.cpp TypingAreaTest.cpp)

# Reaching into `libs/tui/src` on purpose: `ScreenStack` and `IScreen` are
# internal because they name FTXUI, and testing them means speaking FTXUI too.
# The isolation probes above still hold — they link `typeit::tui` alone.
target_include_directories(tui_tests PRIVATE "${CMAKE_SOURCE_DIR}/libs/tui/src")
target_link_libraries(tui_tests PRIVATE typeit::tui typeit::infra typeit::test_support
                                        ftxui::dom ftxui::screen ftxui::component
                                        GTest::gtest GTest::gtest_main)

# The quantiser's distinguishability property is asserted over the theme files
# this project actually ships, not over a copy of their values that could drift
# from them — so the test links `infra` to read them. The *library* still
# cannot: `tui.isolation.infra_is_unreachable` proves it, and a test binary is
# not the library.
target_compile_definitions(tui_tests PRIVATE TYPEIT_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets"
                                             TYPEIT_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/goldens")

include(GoogleTest)
gtest_discover_tests(tui_tests DISCOVERY_TIMEOUT 60)

typeit_set_warnings(tui_tests)
typeit_enable_coverage(tui_tests)
typeit_enable_sanitizers(tui_tests)
typeit_enable_static_analysis(tui_tests)
