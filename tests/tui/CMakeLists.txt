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

add_executable(tui_tests FrameTickerTest.cpp TerminalAppTest.cpp)

target_link_libraries(tui_tests PRIVATE typeit::tui GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(tui_tests DISCOVERY_TIMEOUT 60)

typeit_set_warnings(tui_tests)
typeit_enable_coverage(tui_tests)
typeit_enable_sanitizers(tui_tests)
typeit_enable_static_analysis(tui_tests)
