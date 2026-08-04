#include "typeit/infra/fs/StdFileSystem.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "typeit/core/util/Result.h"

namespace typeit::infra {
    namespace {

        using core::ErrorCode;
        using core::Result;

    }  // namespace

    Result<std::string> StdFileSystem::read_text(const std::filesystem::path& path) const {
        std::error_code failure;
        const std::filesystem::file_status status = std::filesystem::status(path, failure);

        if (failure || !std::filesystem::exists(status)) {
            return core::fail(ErrorCode::FileNotFound, path.string());
        }
        if (std::filesystem::is_directory(status)) {
            return core::fail(ErrorCode::FileUnreadable, path.string() + ": is a directory");
        }

        std::ifstream file{path, std::ios::binary};
        if (!file) {
            return core::fail(ErrorCode::FileUnreadable, path.string() + ": cannot be opened for reading");
        }

        // Read whole through the stream buffer rather than through a pair of
        // istreambuf_iterators: the iterator form makes gcc 14 at -O2 report a
        // potential null dereference inside <streambuf> itself, and a warning
        // nobody can fix in their own code is a warning that gets suppressed
        // wholesale sooner or later.
        //
        // The empty-file check is not an optimisation: extracting from an empty
        // buffer sets failbit, and an empty file is a valid file (defect C1).
        std::ostringstream contents;
        if (file.peek() != std::char_traits<char>::eof()) {
            contents << file.rdbuf();
        }
        if (file.bad()) {
            return core::fail(ErrorCode::FileUnreadable, path.string() + ": read failed");
        }
        return contents.str();
    }

    bool StdFileSystem::exists(const std::filesystem::path& path) const {
        std::error_code ignored;
        return std::filesystem::exists(path, ignored);
    }

    bool StdFileSystem::is_directory(const std::filesystem::path& path) const {
        std::error_code ignored;
        return std::filesystem::is_directory(path, ignored);
    }

    Result<std::vector<std::filesystem::path>> StdFileSystem::list(const std::filesystem::path& directory) const {
        std::error_code failure;
        if (!std::filesystem::is_directory(directory, failure)) {
            return core::fail(ErrorCode::FileNotFound, directory.string() + ": not a directory");
        }

        std::vector<std::filesystem::path> entries;
        for (const std::filesystem::directory_entry& entry: std::filesystem::directory_iterator{directory, failure}) {
            entries.push_back(entry.path());
        }
        if (failure) {
            return core::fail(ErrorCode::FileUnreadable, directory.string() + ": " + failure.message());
        }

        std::ranges::sort(entries);
        return entries;
    }

}  // namespace typeit::infra
