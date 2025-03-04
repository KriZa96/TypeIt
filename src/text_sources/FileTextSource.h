//
// Created by kil3 on 3/4/25.
//

#ifndef FILETEXTSOURCE_H
#define FILETEXTSOURCE_H
#include <string>
#include <fstream>
#include "ITextSource.h"

class FileTextSource: public ITextSource {
    public:
        explicit FileTextSource(const std::string& file_path) : file_path_(file_path) {}

        std::string get_text() const override {
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
    private:
        std::string file_path_;
};



#endif //FILETEXTSOURCE_H
