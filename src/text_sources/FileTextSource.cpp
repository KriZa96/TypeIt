//
// Created by kil3 on 3/4/25.
//

#include "../../include/FileTextSource.h"

#include <fstream>
#include <utility>


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