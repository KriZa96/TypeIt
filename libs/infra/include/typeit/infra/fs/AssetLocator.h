// Where the bundled texts are (TECHNICAL section 4.1).
//
// **This is defect C2.** 1.0 finds its corpora with
//
//     inline static std::filesystem::path current_file_path = __FILE__;
//
// evaluated at runtime, so the binary only finds its assets on the machine
// that compiled it, with the source tree still at the same absolute path. The
// `ln -s … /usr/games/TypeIt` line in the README works by accident, and an
// installed copy on anyone else's machine does not work at all.
//
// The replacement asks the questions an installed program can actually answer:
// where am I, where was I installed, and what did the user tell me.
#ifndef TYPEIT_INFRA_FS_ASSETLOCATOR_H
#define TYPEIT_INFRA_FS_ASSETLOCATOR_H

#include <filesystem>
#include <vector>

#include "typeit/core/util/Result.h"
#include "typeit/infra/fs/PlatformPaths.h"

namespace typeit::infra {

    /// Everything the search depends on, passed in rather than looked up, so a
    /// test can lay out an install tree in a temp directory and ask the real
    /// question about it.
    struct AssetSearch {
        Environment environment;
        /// The running binary, symlinks already resolved. `current_executable()`
        /// is how a program gets its own.
        std::filesystem::path executable;
        /// For the development-only `./assets` entry.
        std::filesystem::path working_directory;
        /// `CMAKE_INSTALL_PREFIX`, baked in at configure time.
        std::filesystem::path install_prefix;
    };

    /// The candidates, in the order TECHNICAL section 4.1 documents:
    ///
    ///   1. `$TYPEIT_ASSETS_DIR`
    ///   2. the executable's directory, `../share/typeit`
    ///   3. the configured install prefix
    ///   4. each entry of `$XDG_DATA_DIRS`
    ///   5. `./assets`, for running out of a build tree
    ///
    /// Returned whole rather than searched internally, so that "tried in this
    /// order" is a fact a test can read rather than infer, and so a failure can
    /// list every place it looked.
    [[nodiscard]] std::vector<std::filesystem::path> asset_search_path(const AssetSearch& search);

    /// The first candidate that exists as a directory.
    ///
    /// Failing is not fatal to the application: the bundled corpora are a
    /// convenience, and a user with their own imported texts loses nothing. The
    /// error names every directory that was tried, because "assets not found"
    /// without that list is a message nobody can act on.
    [[nodiscard]] core::Result<std::filesystem::path> locate_assets(const AssetSearch& search);

    /// The absolute, symlink-resolved path of the running binary — the
    /// `/usr/games/TypeIt -> /home/kim/src/TypeIt/build/TypeIt` case has to
    /// resolve to the real file, or step 2 looks beside the symlink.
    [[nodiscard]] core::Result<std::filesystem::path> current_executable();

    /// `TYPEIT_INSTALL_PREFIX` as configured, for callers assembling an
    /// `AssetSearch` of their own.
    [[nodiscard]] std::filesystem::path configured_install_prefix();

}  // namespace typeit::infra

#endif  // TYPEIT_INFRA_FS_ASSETLOCATOR_H
