# One suite per port, run against every implementation (TESTING section 4).
#
# The target links infra and the fakes together, which no other test target
# does — that is the point of it. A fake is only useful while it behaves like
# the thing it stands in for, and the only way to know is to put both through
# the same expectations.
add_executable(contract_tests FakesTest.cpp HistoryExportTest.cpp FileSystemAndConfigContract.cpp HistoryRepositoryContract.cpp
                              TextLibraryRepositoryContract.cpp)

target_link_libraries(contract_tests PRIVATE typeit::infra typeit::app typeit::test_support
                                             GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(contract_tests DISCOVERY_TIMEOUT 60)

typeit_set_warnings(contract_tests)
typeit_enable_coverage(contract_tests)
typeit_enable_sanitizers(contract_tests)
typeit_enable_static_analysis(contract_tests)
