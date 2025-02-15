find_package(GTest CONFIG REQUIRED)

enable_testing()

add_executable(
        tests
        test_setup.cpp)

target_link_libraries(
        tests PRIVATE
        GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(tests)

add_test(all_tests tests)

