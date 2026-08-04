// Reading files, injectable (TECHNICAL section 2.1).
//
// Exists so that a service can be tested without a filesystem, and so the
// error paths — a missing file, a directory where a file was expected, a
// permission failure — can be produced on demand rather than arranged on disk.
#ifndef TYPEIT_APP_PORTS_IFILESYSTEM_H
#define TYPEIT_APP_PORTS_IFILESYSTEM_H

#include <filesystem>
#include <string>
#include <vector>

#include "typeit/core/util/Result.h"

namespace typeit::app {

    class IFileSystem {
    public:
        IFileSystem() = default;
        virtual ~IFileSystem();
        IFileSystem(const IFileSystem&) = delete;
        IFileSystem& operator=(const IFileSystem&) = delete;
        IFileSystem(IFileSystem&&) = delete;
        IFileSystem& operator=(IFileSystem&&) = delete;

        /// The whole file as bytes. A zero-byte file is an empty string and not
        /// an error — the same case 1.0 reads past the end of (defect C1).
        [[nodiscard]] virtual core::Result<std::string> read_text(const std::filesystem::path& path) const = 0;

        [[nodiscard]] virtual bool exists(const std::filesystem::path& path) const = 0;
        [[nodiscard]] virtual bool is_directory(const std::filesystem::path& path) const = 0;

        /// The immediate children, in a deterministic order. Directory order is
        /// whatever the filesystem feels like, and an import that processes
        /// files in a different order on every machine is not reproducible.
        [[nodiscard]] virtual core::Result<std::vector<std::filesystem::path>> list(
                const std::filesystem::path& directory) const = 0;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_PORTS_IFILESYSTEM_H
