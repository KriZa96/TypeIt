//
// Created by kil3 on 3/4/25.
//

#include "../../include/core/FileTextSource.h"

#include <filesystem>
#include <fstream>
#include <utility>
#include <algorithm>

FileTextSource::FileTextSource(std::string  file_path) : file_path_(std::move(file_path)) {}


std::string FileTextSource::get_text() const {
    std::ifstream fileStream(file_path_);
    if (!fileStream) {
        return "";
    }

    std::string content, line;
    while (std::getline(fileStream, line)) {
        content += line + " ";
    }

    content.pop_back();

    return content;
}

bool FileTextSource::is_file_valid(const std::string& path) {
    if (std::filesystem::is_directory(path)) {
        return false;
    }

    std::ifstream file_stream(path);
    if (!file_stream) {
        return false;
    }

    std::string line;
    while (std::getline(file_stream, line)) {
        if (!line.empty() &&
            std::any_of(
                line.begin(),
                line.end(),
                [](unsigned char c) { return !std::isspace(c); }
            )) {
            return true;
        }
    }

    return false;
}
