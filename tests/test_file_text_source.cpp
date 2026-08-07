//
// Created by Kristijan Zalac on 3/4/25.
//

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

#include "../include/core/FileTextSource.h"


void create_temp_file(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    file << content;
    file.close();
}


TEST(FileTextSourceTest, ReadsFileCorrectlyWithoutNewLine) {
    // A filename per test. All three shared one, and ctest runs each case in
    // its own process but the same directory — so two of them raced and the
    // suite failed at random, roughly once a run. Found while sweeping TI-119;
    // one line is cheaper than one more red pipeline before TI-097 deletes
    // this file outright.
    std::string temp_filename = "temp_test_without_newline.txt";
    std::string expected_content = "Hello world. This is a test file.";
    create_temp_file(temp_filename, expected_content);

    FileTextSource fileSource(temp_filename);
    std::string result = fileSource.get_text();

    EXPECT_EQ(result, expected_content);
}


TEST(FileTextSourceTest, ReadsFileCorrectlyWithNewLine) {
    std::string temp_filename = "temp_test_with_newline.txt";
    std::string expected_content = "Hello world.\nThis is a test file.";
    create_temp_file(temp_filename, expected_content);

    FileTextSource fileSource(temp_filename);
    std::string result = fileSource.get_text();

    EXPECT_EQ(result, "Hello world. This is a test file.");
}


TEST(FileTextSourceTest, ReadsEmptyFileReturnsEmptyString) {
    std::string temp_filename = "temp_empty.txt";
    create_temp_file(temp_filename, "");

    FileTextSource fileSource(temp_filename);

    EXPECT_EQ(fileSource.get_text(), "");
}


TEST(FileTextSourceTest, ReadsWhitespaceOnlyFile) {
    std::string temp_filename = "temp_whitespace.txt";
    create_temp_file(temp_filename, "   ");

    FileTextSource fileSource(temp_filename);

    EXPECT_EQ(fileSource.get_text(), "   ");
}


TEST(FileTextSourceTest, ReadsSingleCharacterFile) {
    std::string temp_filename = "temp_single.txt";
    create_temp_file(temp_filename, "a");

    FileTextSource fileSource(temp_filename);

    EXPECT_EQ(fileSource.get_text(), "a");
}


TEST(FileTextSourceTest, IsFileValidTrue) {
    std::string temp_filename = "temp_valid_true.txt";
    std::string expected_content = "Hello world.\nThis is a test file.";
    create_temp_file(temp_filename, expected_content);

    EXPECT_TRUE(FileTextSource::is_file_valid(temp_filename));
}

TEST(FileTextSourceTest, IsFileValidFalse) {
    // A name nothing else writes. This asserts the file is *absent*, and
    // `IsFileValidEmptyFile` used to create the very file it names.
    std::string temp_filename = "temp_valid_false_never_created.txt";

    EXPECT_FALSE(FileTextSource::is_file_valid(temp_filename));
}


TEST(FileTextSourceTest, IsFileValidEmptyFile) {
    std::string temp_filename = "temp_valid_empty.txt";
    std::string expected_content = "    ";
    create_temp_file(temp_filename, expected_content);

    EXPECT_FALSE(FileTextSource::is_file_valid(temp_filename));
}
