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
#include <memory>
#include <optional>
#include <string>

#include "typeit/app/ingest/Code.h"
#include "typeit/app/ingest/ExtractorRegistry.h"
#include "typeit/app/ingest/Markdown.h"
#include "typeit/app/ingest/PlainText.h"
#include "typeit/app/ingest/Subtitles.h"
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
        /// What the extractor thought worth mentioning: a source file that
        /// mixes tabs and spaces, one with minified lines nobody can type.
        /// Empty for almost everything.
        std::vector<std::string> warnings;
    };

    /// What came of importing a folder (TX-004).
    ///
    /// A batch reports rather than fails. One unreadable file in a folder of
    /// two hundred should not cost somebody the other hundred and ninety-nine,
    /// and a summary they can read beats an error naming only the first thing
    /// that went wrong.
    struct DirectoryImport {
        std::vector<ImportOutcome> imported;
        /// One line per file that did not import, each naming the file and the
        /// reason. A count alone would tell somebody that something was wrong
        /// without telling them what.
        std::vector<std::string> skipped;
    };

    /// How far through a long text somebody is (GAMEPLAY §2.3).
    ///
    /// `fraction` is exact at both ends rather than close enough: a book
    /// reported as 99.7% finished is a book somebody types one more chunk of to
    /// find nothing there, and one reported as 0.3% before they have started is
    /// a book that lies about the work already done.
    struct TextProgress {
        core::GraphemeIndex offset{0};
        std::size_t total = 0;
        double fraction = 0.0;
        bool finished = false;
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
            library_{&library}, files_{&files}, clock_{&clock}, normalization_{normalization} {
            // The built-in extractors, registered here rather than by the
            // composition root: a service that could be handed an empty
            // registry would be one that fails to import a plain text file,
            // which is not a state worth being able to construct. TX-002
            // onwards add to this list.
            static_cast<void>(extractors_.add(std::make_shared<PlainTextExtractor>()));
            static_cast<void>(extractors_.add(std::make_shared<MarkdownExtractor>()));
            static_cast<void>(extractors_.add(std::make_shared<CodeExtractor>()));
            static_cast<void>(extractors_.add(std::make_shared<SubtitleExtractor>()));
        }

        /// The extractors this service will use. Exposed so a test can say what
        /// is registered rather than infer it from what imports.
        [[nodiscard]] const ExtractorRegistry& extractors() const noexcept { return extractors_; }

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

        /// Every file in a folder, one text apiece.
        ///
        /// Immediate children only. Recursing would import a source tree's
        /// entire history of vendored dependencies from one keystroke, and a
        /// folder somebody points at is a folder they can see the contents of.
        [[nodiscard]] core::Result<DirectoryImport> import_directory(const std::filesystem::path& path);

        /// Records how far through a text somebody got, stamped with the
        /// clock's time rather than the caller's idea of it.
        [[nodiscard]] core::Status bookmark(core::TextId id, core::GraphemeIndex offset);

        /// Moves the bookmark on by what was **actually typed**, not by the
        /// chunk that was offered.
        ///
        /// A run abandoned half way through a chunk has read half a chunk, and
        /// advancing by the whole one would skip text nobody saw. Clamped to
        /// the end of the text: a caller that over-counts should not produce a
        /// bookmark pointing past the last grapheme.
        [[nodiscard]] core::Status advance(core::TextId id, std::size_t graphemes_completed);

        /// Back to the beginning, for a text somebody has finished or wants to
        /// start again.
        [[nodiscard]] core::Status reset_progress(core::TextId id);

        /// Where somebody is in a text, and how much of it is left.
        ///
        /// A text with no bookmark is at zero rather than an error: not having
        /// started is the normal state of most of a library.
        [[nodiscard]] core::Result<TextProgress> progress(core::TextId id) const;

        /// What normalisation the next import will apply. Changing it does not
        /// re-import anything: the raw content is kept alongside the normalised
        /// form precisely so that settings can change without re-importing.
        void set_normalization(core::NormalizeOptions options) { normalization_ = options; }

        [[nodiscard]] const core::NormalizeOptions& normalization() const noexcept { return normalization_; }

    private:
        ExtractorRegistry extractors_;
        ITextLibraryRepository* library_;
        /// The shared tail of both imports: normalise, deduplicate, store.
        ///
        /// Takes the normalisation rather than reading the member, because an
        /// extractor can override it (`ExtractedText::normalization`) and a
        /// source file normalised as prose is a source file with its
        /// indentation collapsed into single spaces.
        ///
        /// `boundaries` are where the extractor found chapters, in the bytes it
        /// produced; they are mapped onto the normalised text here, which is
        /// the only place that has both halves to map between (TX-005).
        [[nodiscard]] core::Result<ImportOutcome> store(std::string content, TextSource source,
                                                        std::optional<std::string> origin,
                                                        std::optional<std::string> title,
                                                        const core::NormalizeOptions& normalization,
                                                        std::vector<std::string> warnings,
                                                        std::vector<SectionBoundary> boundaries = {});

        IFileSystem* files_;
        core::IWallClock* clock_;
        core::NormalizeOptions normalization_;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_SERVICES_TEXTLIBRARYSERVICE_H
