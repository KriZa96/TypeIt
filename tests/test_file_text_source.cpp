//
// Created by Kristijan Zalac on 3/4/25.
//

#include <fstream>
#include <filesystem>
#include <gtest/gtest.h>

#include "../include/core/FileTextSource.h"


void create_temp_file(const std::string& filename, const std::string& content) {
    std::ofstream file(filename);
    file << content;
    file.close();
}


TEST(FileTextSourceTest, ReadsFileCorrectlyWithoutNewLine) {
    std::string temp_filename = "temp_test.txt";
    std::string expected_content = "Hello world. This is a test file.";
    create_temp_file(temp_filename, expected_content);

    FileTextSource fileSource(temp_filename);
    std::string result = fileSource.get_text();

    EXPECT_EQ(result, expected_content);
}


TEST(FileTextSourceTest, ReadsFileCorrectlyWithNewLine) {
    std::string temp_filename = "temp_test.txt";
    std::string expected_content = "Hello world.\nThis is a test file.";
    create_temp_file(temp_filename, expected_content);

    FileTextSource fileSource(temp_filename);
    std::string result = fileSource.get_text();

    EXPECT_EQ(result, "Hello world. This is a test file.");
}


TEST(FileTextSourceTest, IsFileValidTrue) {
    std::string temp_filename = "temp_test.txt";
    std::string expected_content = "Hello world.\nThis is a test file.";
    create_temp_file(temp_filename, expected_content);

    EXPECT_TRUE(FileTextSource::is_file_valid(temp_filename));
}

TEST(FileTextSourceTest, IsFileValidFalse) {
    std::string temp_filename = "temp_test_.txt";

    EXPECT_FALSE(FileTextSource::is_file_valid(temp_filename));
}


TEST(FileTextSourceTest, IsFileValidEmptyFile) {
    std::string temp_filename = "temp_test_.txt";
    std::string expected_content = "    ";
    create_temp_file(temp_filename, expected_content);

    EXPECT_FALSE(FileTextSource::is_file_valid(temp_filename));
}