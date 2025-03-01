find_package(GTest CONFIG REQUIRED)
find_package(ftxui CONFIG REQUIRED)

enable_testing()

add_executable(
        tests
        test_setup.cpp
        test_timer.cpp
        ../src/components/Timer.h
        ../src/components/Timer.cpp
)

target_link_libraries(
        tests PRIVATE
        GTest::gtest
        GTest::gtest_main
        ftxui::dom
        ftxui::screen
        ftxui::component
)

include(GoogleTest)
gtest_discover_tests(tests)

add_test(all_tests tests)
