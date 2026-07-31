file(
        GLOB
        SRC_FILES
        ../src/*/*.cpp
)

file(
        GLOB
        HEADER_FILES
        ../include/*/*.h
)

file(
        GLOB
        TEST_FILES
        ./test_*.cpp
)

add_executable(
        tests
        ${HEADER_FILES}
        ${SRC_FILES}
        ${TEST_FILES}
        test_input_line_engine.cpp
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

typeit_set_warnings(tests)
