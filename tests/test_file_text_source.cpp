//
// Created by kil3 on 3/4/25.
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


TEST(FileTextSourceTest, HandlesMissingFile) {
    FileTextSource file_source("../../files/hard.txt");
    std::string result = file_source.get_text();

    EXPECT_NE(result, "");
}
