// What a file has to survive on its way into the library (TI-110).
//
// `TextLibraryServiceTest` covers what importing *stores*. This covers what it
// has to cope with first: the encodings, the marks and the line endings that a
// text picks up from whatever wrote it, and which turn into a typing test
// somebody cannot pass.

#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string>

#include "typeit/app/records/TextLibrary.h"
#include "typeit/app/services/TextLibraryService.h"
#include "typeit/core/util/Result.h"
#include "typeit/core/util/Units.h"
#include "typeit/testing/FakeClock.h"
#include "typeit/testing/Fakes.h"

namespace typeit::app {
    namespace {

        using core::ErrorCode;

        constexpr core::Millis kNoon{1'767'225'600'000};

        /// The UTF-8 byte-order mark, spelled once.
        constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF", 3};

        class ImportFileTest : public ::testing::Test {
        protected:
            /// A file with these exact bytes, and the import of it.
            core::Result<ImportOutcome> import_bytes(const std::string& path, const std::string& bytes) {
                files_.add_file(path, bytes);
                return service_.import_file(path);
            }

            /// What was stored for an import, so a test can look at the text
            /// the typist would actually be given.
            [[nodiscard]] TextItem stored(core::TextId id) {
                const core::Result<std::optional<TextItem>> found = library_.get(id);
                EXPECT_TRUE(found);
                EXPECT_TRUE(found && found->has_value());
                return (found && found->has_value()) ? **found : TextItem{};
            }

            testing::FakeTextLibraryRepository library_;
            testing::FakeFileSystem files_;
            testing::FakeClock clock_{kNoon};
            TextLibraryService service_{library_, files_, clock_};
        };

        // ---- encodings and marks ---------------------------------------------

        TEST_F(ImportFileTest, PlainUtf8Imports) {
            const core::Result<ImportOutcome> imported = import_bytes("plain.txt", "the quick brown fox");

            ASSERT_TRUE(imported) << (imported ? "" : imported.error().context);
            EXPECT_FALSE(imported->already_present);
        }

        TEST_F(ImportFileTest, AUtf8ByteOrderMarkIsStrippedRatherThanTyped) {
            // The mark is valid UTF-8, which is exactly the problem: it decodes
            // to U+FEFF and becomes a grapheme at the head of the text that the
            // typist has to type and cannot see. Every file Notepad saves has
            // one.
            const core::Result<ImportOutcome> imported =
                    import_bytes("windows.txt", std::string{kUtf8Bom} + "the quick brown fox");

            ASSERT_TRUE(imported) << (imported ? "" : imported.error().context);
            const TextItem& item = stored(imported->id);
            EXPECT_EQ(item.content, "the quick brown fox");
            EXPECT_FALSE(item.content.starts_with(kUtf8Bom));
        }

        TEST_F(ImportFileTest, TheSameTextWithAndWithoutAMarkIsOneText) {
            // The reason the mark goes before the hash rather than after: two
            // copies of one article, one saved by an editor that writes a mark
            // and one by an editor that does not, are the same article.
            const core::Result<ImportOutcome> bare = import_bytes("bare.txt", "the quick brown fox");
            const core::Result<ImportOutcome> marked =
                    import_bytes("marked.txt", std::string{kUtf8Bom} + "the quick brown fox");

            ASSERT_TRUE(bare);
            ASSERT_TRUE(marked);
            EXPECT_EQ(marked->id, bare->id);
            EXPECT_TRUE(marked->already_present);
        }

        TEST_F(ImportFileTest, CrlfBecomesLf) {
            const core::Result<ImportOutcome> imported = import_bytes("dos.txt", "one\r\ntwo\r\nthree");

            ASSERT_TRUE(imported) << (imported ? "" : imported.error().context);
            EXPECT_EQ(stored(imported->id).content, "one\ntwo\nthree");
        }

        TEST_F(ImportFileTest, ADosFileAndAUnixFileAreOneText) {
            const core::Result<ImportOutcome> unix_file = import_bytes("unix.txt", "one\ntwo");
            const core::Result<ImportOutcome> dos_file = import_bytes("dos.txt", "one\r\ntwo");

            ASSERT_TRUE(unix_file);
            ASSERT_TRUE(dos_file);
            EXPECT_EQ(dos_file->id, unix_file->id) << "the same words, saved on two machines";
        }

        TEST_F(ImportFileTest, AUtf16FileIsRejectedByNameRatherThanAsCorruptUtf8) {
            // "Invalid UTF-8 at byte 0" is true and useless: it sends somebody
            // hunting for one bad character when the whole file is in another
            // encoding, which is a different fix entirely.
            const std::string utf16le = std::string{"\xFF\xFE", 2} + std::string{"h\0i\0", 4};

            const core::Result<ImportOutcome> imported = import_bytes("utf16.txt", utf16le);

            ASSERT_FALSE(imported);
            EXPECT_EQ(imported.error().code, ErrorCode::InvalidUtf8);
            EXPECT_NE(imported.error().context.find("UTF-16LE"), std::string::npos) << imported.error().context;
        }

        TEST_F(ImportFileTest, EveryForeignByteOrderMarkIsNamed) {
            const std::string utf16be = std::string{"\xFE\xFF", 2} + std::string{"\0h\0i", 4};
            const std::string utf32le = std::string{"\xFF\xFE\x00\x00", 4} + std::string{"h\0\0\0", 4};
            const std::string utf32be = std::string{"\x00\x00\xFE\xFF", 4} + std::string{"\0\0\0h", 4};

            for (const auto& [bytes, name]: {std::pair{utf16be, "UTF-16BE"}, std::pair{utf32le, "UTF-32LE"},
                                             std::pair{utf32be, "UTF-32BE"}}) {
                library_.texts.clear();
                const core::Result<ImportOutcome> imported = import_bytes("foreign.txt", bytes);

                ASSERT_FALSE(imported) << name;
                EXPECT_NE(imported.error().context.find(name), std::string::npos) << imported.error().context;
            }
        }

        TEST_F(ImportFileTest, InvalidUtf8ThatIsNotAnEncodingMarkStillReportsTheByte) {
            // A genuinely corrupt character keeps the offset it always had —
            // naming encodings must not have swallowed the useful case.
            const core::Result<ImportOutcome> imported = import_bytes("broken.txt", "good then \xFF bad");

            ASSERT_FALSE(imported);
            EXPECT_EQ(imported.error().code, ErrorCode::InvalidUtf8);
            EXPECT_NE(imported.error().context.find("10"), std::string::npos)
                    << "the byte offset: " << imported.error().context;
        }

        // ---- what a path can be ----------------------------------------------

        TEST_F(ImportFileTest, TheExtensionIsNotConsulted) {
            // A typing test over source code is the point of the feature, and a
            // whitelist of extensions is a list somebody's file is missing from.
            for (const std::string name: {"notes", "README.md", "main.rs", "article.txt", "a.tar.gz"}) {
                library_.texts.clear();
                const core::Result<ImportOutcome> imported = import_bytes(name, "fn main() { println!(); }");

                EXPECT_TRUE(imported) << name << ": " << (imported ? "" : imported.error().context);
            }
        }

        TEST_F(ImportFileTest, AFileWithNoExtensionIsTitledAfterItsWholeName) {
            const core::Result<ImportOutcome> imported = import_bytes("notes", "the quick brown fox");

            ASSERT_TRUE(imported);
            EXPECT_EQ(stored(imported->id).title, "notes");
        }

        // ---- the ways a read can fail ----------------------------------------

        TEST_F(ImportFileTest, AMissingFileADirectoryAndAnUnreadableFileAreDistinct) {
            // Three different fixes, so three different errors: the person who
            // typed the path needs to know which of them they are looking at.
            files_.add_directory("some/dir");

            const core::Result<ImportOutcome> missing = service_.import_file("nowhere.txt");
            const core::Result<ImportOutcome> directory = service_.import_file("some/dir");

            ASSERT_FALSE(missing);
            ASSERT_FALSE(directory);
            EXPECT_EQ(missing.error().code, ErrorCode::FileNotFound);
            EXPECT_EQ(directory.error().code, ErrorCode::FileUnreadable);
            EXPECT_NE(directory.error().context.find("directory"), std::string::npos) << directory.error().context;
        }

        TEST_F(ImportFileTest, APermissionFailureIsReportedRatherThanLookingLikeAMissingFile) {
            // What the real file system gives back when a file is there and
            // cannot be opened. Armed on the port, because a test that chmods a
            // file only passes for a user who is not root.
            files_.add_file("secret.txt", "the quick brown fox");
            files_.failure.fail_next(core::make_error(ErrorCode::FileUnreadable, "secret.txt: permission denied"));

            const core::Result<ImportOutcome> imported = service_.import_file("secret.txt");

            ASSERT_FALSE(imported);
            EXPECT_EQ(imported.error().code, ErrorCode::FileUnreadable);
            EXPECT_NE(imported.error().context.find("permission"), std::string::npos) << imported.error().context;
        }

        TEST_F(ImportFileTest, AFileOfOnlyAMarkIsEmpty) {
            // Stripping the mark leaves nothing, and nothing is the defect C1
            // case rather than a text of length zero somebody gets to type.
            const core::Result<ImportOutcome> imported = import_bytes("marked-empty.txt", std::string{kUtf8Bom});

            ASSERT_FALSE(imported);
            EXPECT_EQ(imported.error().code, ErrorCode::EmptyText);
        }

    }  // namespace
}  // namespace typeit::app
