//
// Created by kil3 on 3/4/25.
//

#include "../../include/core/FileTextSource.h"

#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>
#include <ranges>
#include <algorithm>

FileTextSource::FileTextSource(std::string  file_path) : file_path_(std::move(file_path)) {}


bool FileTextSource::has_text_stream(const std::string& path) {
    return std::filesystem::exists(path);
}


bool FileTextSource::is_text_file(const std::string& path) {
    static const std::vector<std::string> text_extensions = {".txt",  ".csv", ".md", ".json", ".xml",
                                                             ".html", ".cpp", ".h",  ".py",   ".java"};

    const std::string ext = std::filesystem::path(path).extension().string();

    return std::ranges::any_of(text_extensions, [&](const auto& valid_ext) {
        return ext == valid_ext;
    });
}


std::string FileTextSource::get_text() const {

    if (!is_text_file(file_path_)) {
        return "";
    }

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
