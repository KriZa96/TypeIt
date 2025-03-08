find_package(GTest CONFIG REQUIRED)
find_package(ftxui CONFIG REQUIRED)

enable_testing()

file(
        GLOB
        SRC_FILES
        ../src/components/*.cpp
        ../src/text_sources/*.cpp
)

file(
        GLOB
        HEADER_FILES
        ../include/*.h
)

file(
        GLOB
        TEST_FILES
        ./*.cpp
)

add_executable(
        tests
        ${HEADER_FILES}
        ${SRC_FILES}
        ${TEST_FILES}
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
