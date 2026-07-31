# test_input_line_engine.cpp used to be listed twice -- once through the glob,
# once explicitly.
add_executable(
    tests
    test_file_text_source.cpp
    test_input.cpp
    test_input_accuracy.cpp
    test_input_line_engine.cpp
    test_screen.cpp
    test_text.cpp
    test_timer.cpp
    test_word_calculator.cpp
    test_word_count.cpp)

target_link_libraries(tests PRIVATE typeit_legacy GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(tests)

add_test(all_tests tests)

typeit_set_warnings(tests)
