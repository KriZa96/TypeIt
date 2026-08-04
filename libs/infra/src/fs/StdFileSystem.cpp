#include "typeit/infra/fs/StdFileSystem.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
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

        // Read whole, through iterators: a zero-byte file yields an empty
        // string rather than an underflow, and a file whose size changes
        // between the stat and the read is not truncated to a stale length.
        std::string contents{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
        if (file.bad()) {
            return core::fail(ErrorCode::FileUnreadable, path.string() + ": read failed");
        }
        return contents;
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
