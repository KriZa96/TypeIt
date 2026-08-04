# The rule that is only worth stating if something enforces it: SQLite and
# toml++ end at infra's edge (ADR-001, ADR-013).
#
# Not a compile probe, unlike tests/core's FTXUI one. A probe proves the header
# is *unreachable*, which holds only while the library comes from FetchContent
# with private include directories; a distro SQLite lives in /usr/include,
# where the compiler finds it from anywhere on the machine whatever any
# CMakeLists says. A probe expecting the include to fail would itself fail on
# most Linux boxes, and a test that is red on a normal developer setup is a test
# nobody keeps.
#
# So the check reads the sources instead: not "could this header be found" but
# "did anyone reach for it", which is the rule ADR-001 actually states. It
# covers FTXUI in core and app too, where it is machine-independent in a way the
# probe is not.
add_test(NAME infra.isolation.layers_include_only_what_they_may
         COMMAND "${CMAKE_COMMAND}" "-DTYPEIT_SOURCE_DIR=${CMAKE_SOURCE_DIR}" -P
                 "${CMAKE_SOURCE_DIR}/cmake/CheckLayerIsolation.cmake")

add_executable(infra_tests DependenciesTest.cpp)

target_link_libraries(infra_tests PRIVATE typeit::infra typeit::test_support GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(infra_tests DISCOVERY_TIMEOUT 60)

typeit_set_warnings(infra_tests)
typeit_enable_coverage(infra_tests)
typeit_enable_sanitizers(infra_tests)
typeit_enable_static_analysis(infra_tests)
