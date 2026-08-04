# The app layer's tests. The layer rule — app sees core and never infra — is
# checked by infra.lint.source_rules, which reads the sources rather than
# relying on an include path that a system package would defeat anyway.
add_executable(app_tests ConfigServiceTest.cpp HistoryServiceTest.cpp PortsTest.cpp ProfileServiceTest.cpp TextLibraryServiceTest.cpp)

target_link_libraries(app_tests PRIVATE typeit::app typeit::test_support GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(app_tests DISCOVERY_TIMEOUT 60)

typeit_set_warnings(app_tests)
typeit_enable_coverage(app_tests)
typeit_enable_sanitizers(app_tests)
typeit_enable_static_analysis(app_tests)
