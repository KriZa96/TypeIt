# The dependency rule from ADR-001, checked at configure time rather than
# trusted. Both directions are checked: a source that includes FTXUI must fail
# with core's include directories, and one that includes only the standard
# library must succeed with the same setup — otherwise a broken harness would
# read as a passing test forever.
#
# STATIC_LIBRARY so try_compile stops after archiving: these sources have no
# main(), and a link error would make the negative case pass for the wrong
# reason.
# Real targets in the real build graph, not try_compile. A dependency arrives
# by being linked, and its include directories come with it transitively;
# try_compile reaches neither the transitive usage requirements of an
# in-project target nor the include directories read off it, so both spellings
# report a clean bill of health for a core that links the whole of FTXUI. Both
# were tried, and both passed the deliberate violation.
#
# EXCLUDE_FROM_ALL: the probe is expected not to compile, so it must never be
# part of a normal build. Warnings are deliberately not applied — it has to
# fail on the include, not on something -Werror noticed.
add_library(core_isolation_probe STATIC EXCLUDE_FROM_ALL probes/includes_ftxui.cpp)
target_link_libraries(core_isolation_probe PRIVATE typeit::core)

add_library(core_isolation_control STATIC EXCLUDE_FROM_ALL probes/includes_stdlib.cpp)
target_link_libraries(core_isolation_control PRIVATE typeit::core)

# A discarded Result is an ignored failure. std::expected is [[nodiscard]] in
# both standard libraries this project builds against, so the probe only has to
# make the warning fatal -- typeit_set_warnings does not, because TYPEIT_WERROR
# is off in a local build by design.
add_library(core_nodiscard_probe STATIC EXCLUDE_FROM_ALL probes/discards_result.cpp)
target_link_libraries(core_nodiscard_probe PRIVATE typeit::core)
target_compile_options(core_nodiscard_probe PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/WX;/W4,-Werror;-Wunused-result>)

# Keep the probes out of compile_commands.json. Every analyser that walks the
# compilation database -- clang-tidy, MSVC /analyze -- treats a file that
# cannot compile as its own failure, and these two are supposed not to.
set_target_properties(core_isolation_probe core_nodiscard_probe PROPERTIES EXPORT_COMPILE_COMMANDS OFF)

# The control comes first in name so a broken harness is obvious: a negative
# test that cannot tell "unreachable" from "the compiler never ran" proves
# nothing.
add_test(NAME core.isolation.control
         COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target core_isolation_control)

add_test(NAME core.isolation.ftxui_is_unreachable
         COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target core_isolation_probe)

add_test(NAME core.nodiscard.result_cannot_be_ignored
         COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target core_nodiscard_probe)

set_tests_properties(core.isolation.ftxui_is_unreachable core.nodiscard.result_cannot_be_ignored
                     PROPERTIES WILL_FAIL TRUE)

add_executable(core_tests FakeClockTest.cpp ResultTest.cpp UnitsTest.cpp)

target_link_libraries(core_tests PRIVATE typeit::core typeit::test_support GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(core_tests)

typeit_set_warnings(core_tests)
typeit_enable_sanitizers(core_tests)
typeit_enable_static_analysis(core_tests)
