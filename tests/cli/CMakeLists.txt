# The command line's tests. The parser is pure, so these are values in and
# values out — no subprocess, no temporary files, no terminal.
add_executable(cli_tests CliParserTest.cpp ScriptParserTest.cpp SimulateTest.cpp)

target_link_libraries(cli_tests PRIVATE typeit::cli typeit::test_support GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(cli_tests DISCOVERY_TIMEOUT 60)

typeit_set_warnings(cli_tests)
typeit_enable_coverage(cli_tests)
typeit_enable_sanitizers(cli_tests)
typeit_enable_static_analysis(cli_tests)
