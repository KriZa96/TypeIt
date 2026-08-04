// Getting a text into the library (TI-070).
//
// Importing is the one place where a file becomes something that can be typed:
// read, validate, normalise, hash, score, and store — or recognise that it is
// already there. It is a service rather than a repository method because every
// one of those steps has an opinion, and the repository's job is to write rows.
//
// The successor to 1.0's `is_file_valid`, which answered "does this file
// exist" and let an empty one through to be read past the end of (defect C1).
#ifndef TYPEIT_APP_SERVICES_TEXTLIBRARYSERVICE_H
#define TYPEIT_APP_SERVICES_TEXTLIBRARYSERVICE_H

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

#include "typeit/app/ports/IFileSystem.h"
#include "typeit/app/ports/ITextLibraryRepository.h"
#include "typeit/app/records/TextLibrary.h"
#include "typeit/core/text/TextNormalizer.h"
#include "typeit/core/util/IClock.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"

namespace typeit::app {

    struct ImportOutcome {
        core::TextId id{0};
        /// True when the content was already in the library and this import
        /// found it rather than storing it again.
        bool already_present = false;
    };

    class TextLibraryService {
    public:
        /// Four megabytes, which is a longer book than anybody will type and
        /// short enough that reading it whole is not a decision. The limit is
        /// checked after reading, because `IFileSystem` deliberately has no
        /// stat: a port that could answer questions about a file without
        /// reading it would be a port with two ways to be wrong.
        static constexpr std::size_t kMaxImportBytes = 4U * 1024U * 1024U;

        /// All three references must outlive the service; the composition root
        /// owns them.
        TextLibraryService(ITextLibraryRepository& library, IFileSystem& files, core::IWallClock& clock,
                           core::NormalizeOptions normalization = {}) :
            library_{&library}, files_{&files}, clock_{&clock}, normalization_{normalization} {}

        /// Reads, imports, and titles the text after the file unless `title`
        /// says otherwise.
        [[nodiscard]] core::Result<ImportOutcome> import_file(const std::filesystem::path& path,
                                                              std::optional<std::string> title = std::nullopt);

        /// The same, for content that never was a file: a paste, or standard
        /// input. An untitled one is named after its opening words, because a
        /// library of things called "Untitled" is not a library.
        [[nodiscard]] core::Result<ImportOutcome> import_text(std::string content, TextSource source,
                                                              std::optional<std::string> origin = std::nullopt,
                                                              std::optional<std::string> title = std::nullopt);

        /// Records how far through a text somebody got, stamped with the
        /// clock's time rather than the caller's idea of it.
        [[nodiscard]] core::Status bookmark(core::TextId id, core::GraphemeIndex offset);

        /// What normalisation the next import will apply. Changing it does not
        /// re-import anything: the raw content is kept alongside the normalised
        /// form precisely so that settings can change without re-importing.
        void set_normalization(core::NormalizeOptions options) { normalization_ = options; }

        [[nodiscard]] const core::NormalizeOptions& normalization() const noexcept { return normalization_; }

    private:
        ITextLibraryRepository* library_;
        IFileSystem* files_;
        core::IWallClock* clock_;
        core::NormalizeOptions normalization_;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_SERVICES_TEXTLIBRARYSERVICE_H
