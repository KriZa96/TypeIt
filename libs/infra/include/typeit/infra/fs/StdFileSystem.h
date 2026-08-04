// Reading files through std::filesystem (TECHNICAL section 2.1).
//
// The adapter behind IFileSystem. Everything above it takes the interface, so a
// service can be tested without a disk and its error paths can be produced on
// demand rather than arranged on one.
#ifndef TYPEIT_INFRA_FS_STDFILESYSTEM_H
#define TYPEIT_INFRA_FS_STDFILESYSTEM_H

#include <filesystem>
#include <string>
#include <vector>

#include "typeit/app/ports/IFileSystem.h"
#include "typeit/core/util/Result.h"

namespace typeit::infra {

    class StdFileSystem final : public app::IFileSystem {
    public:
        /// Distinct errors for the three ways this fails, because "could not
        /// read the file" tells a user nothing they can act on: absent
        /// (`FileNotFound`), a directory or unreadable (`FileUnreadable`).
        ///
        /// A zero-byte file is an empty string and not an error — the case 1.0
        /// reads past the end of (defect C1).
        [[nodiscard]] core::Result<std::string> read_text(const std::filesystem::path& path) const override;

        [[nodiscard]] bool exists(const std::filesystem::path& path) const override;
        [[nodiscard]] bool is_directory(const std::filesystem::path& path) const override;

        /// Sorted. Directory order is whatever the filesystem feels like, and
        /// an import that processes files in a different order on every machine
        /// is not reproducible.
        [[nodiscard]] core::Result<std::vector<std::filesystem::path>> list(
                const std::filesystem::path& directory) const override;
    };

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_FS_STDFILESYSTEM_H
