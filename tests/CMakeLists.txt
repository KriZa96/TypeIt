add_subdirectory(support)
add_subdirectory(core)
add_subdirectory(app)
add_subdirectory(cli)
add_subdirectory(contract)
add_subdirectory(infra)
add_subdirectory(tui)

# Below: the 1.0.0 suite, which keeps passing unchanged throughout the rebuild
# and goes at the Phase 4 cutover (TI-097).
#
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
    test_version.cpp
    test_word_count.cpp)

target_link_libraries(tests PRIVATE typeit_legacy GTest::gtest GTest::gtest_main)

include(GoogleTest)
# The default discovery timeout is five seconds, which is not enough for a
# freshly linked executable on a Windows runner: the first run pays for
# the antivirus scan and CMake reports it as a hang.
gtest_discover_tests(tests DISCOVERY_TIMEOUT 60)

# No `add_test(all_tests tests)` beside that discovery. It registered the whole
# binary a second time, so `ctest -j` ran the legacy suite concurrently with its
# own individually discovered cases -- two processes creating and deleting the
# same fixture files, which failed roughly one release run in four and passed on
# a retry, the shape of a flake nobody chases. Discovery already covers every
# case in the binary; the aggregate added a race and nothing else.

typeit_set_warnings(tests)
typeit_enable_sanitizers(tests)
typeit_enable_static_analysis(tests)
