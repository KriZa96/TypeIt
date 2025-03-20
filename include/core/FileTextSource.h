//
// Created by kil3 on 3/4/25.
//

#ifndef FILETEXTSOURCE_H
#define FILETEXTSOURCE_H

#include <string>

#include "../interface/ITextSource.h"


class FileTextSource final : public ITextSource {
    public:
        explicit FileTextSource(std::string file_path);
        [[nodiscard]] std::string get_text() const override;
        [[nodiscard]] static bool has_text_stream(const std::string& path);
        [[nodiscard]] static bool is_text_file(const std::string& path);
    private:
        std::string file_path_;
};


#endif //FILETEXTSOURCE_H
