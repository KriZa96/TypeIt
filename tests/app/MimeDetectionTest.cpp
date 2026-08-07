// What these bytes are (TX-001).
//
// The ordering is the whole subject. A `.txt` file that is actually a ZIP is a
// real thing — a download named badly, an EPUB somebody renamed — and believing
// the name hands a ZIP to the plain-text extractor, which reports invalid UTF-8
// and sends the user hunting for a corrupt character in a file that is not text
// at all.

#include <cstddef>
#include <gtest/gtest.h>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/ingest/MimeDetection.h"

namespace typeit::app {
    namespace {

        /// Characters as bytes, for a test that wants to write a signature.
        std::vector<std::byte> bytes(std::string_view text) {
            std::vector<std::byte> out;
            out.reserve(text.size());
            for (const char letter: text) {
                out.push_back(static_cast<std::byte>(static_cast<unsigned char>(letter)));
            }
            return out;
        }

        // ---- magic bytes -------------------------------------------------------

        TEST(MimeDetectionTest, KnownSignaturesAreRecognised) {
            EXPECT_EQ(mime_from_magic(bytes(std::string_view{"PK\x03\x04junk", 8})), "application/zip");
            EXPECT_EQ(mime_from_magic(bytes("%PDF-1.7")), "application/pdf");
            EXPECT_EQ(mime_from_magic(bytes(std::string_view{"\x1F\x8B\x08", 3})), "application/gzip");
        }

        TEST(MimeDetectionTest, AnUnknownSignatureIsEmptyRatherThanAGuess) {
            // Only formats this project can name. A general-purpose sniffer
            // would be a second `file(1)` to maintain, and every type it knew
            // that no extractor handled would be a more confident way to fail.
            EXPECT_TRUE(mime_from_magic(bytes("just some ordinary prose")).empty());
        }

        TEST(MimeDetectionTest, DetectionOnDegenerateInputIsEmptyRatherThanACrash) {
            // Zero bytes, one byte, and a file of pure NULs: each is shorter
            // than most signatures, and a comparison that read past the end
            // would be undefined rather than merely wrong.
            EXPECT_TRUE(mime_from_magic({}).empty());
            EXPECT_TRUE(mime_from_magic(bytes("P")).empty());
            EXPECT_TRUE(mime_from_magic(bytes(std::string_view{"\0\0\0\0\0\0\0\0", 8})).empty());
        }

        // ---- extensions --------------------------------------------------------

        TEST(MimeDetectionTest, TheExtensionNamesTheType) {
            EXPECT_EQ(mime_from_extension("notes.txt"), "text/plain");
            EXPECT_EQ(mime_from_extension("README.md"), "text/markdown");
            EXPECT_EQ(mime_from_extension("book.epub"), "application/epub+zip");
            EXPECT_EQ(mime_from_extension("main.rs"), "text/x-code");
            EXPECT_EQ(mime_from_extension("film.srt"), "text/x-subrip");
        }

        TEST(MimeDetectionTest, TheExtensionIsCaseInsensitive) {
            EXPECT_EQ(mime_from_extension("NOTES.TXT"), "text/plain");
            EXPECT_EQ(mime_from_extension("Book.EPub"), "application/epub+zip");
        }

        TEST(MimeDetectionTest, OnlyTheLastExtensionCounts) {
            // `notes.txt.md` is markdown, not text. Asserted with two types the
            // table actually names: `.tar.gz` reads better in a comment and
            // tests nothing, because `gz` is recognised by its magic bytes and
            // deliberately not by its name.
            EXPECT_EQ(mime_from_extension("notes.txt.md"), "text/markdown");
            EXPECT_EQ(mime_from_extension("main.md.rs"), "text/x-code");
        }

        TEST(MimeDetectionTest, ANameWithNoExtensionHasNoType) {
            EXPECT_TRUE(mime_from_extension("notes").empty());
            EXPECT_TRUE(mime_from_extension("").empty());
        }

        TEST(MimeDetectionTest, ADotInADirectoryNameIsNotAnExtension) {
            // `/home/kim/v2.0/notes` is a file called `notes`, not one of type
            // `0/notes`.
            EXPECT_TRUE(mime_from_extension("/home/kim/v2.0/notes").empty());
        }

        TEST(MimeDetectionTest, ALeadingDotIsAHiddenFileRatherThanAnExtension) {
            EXPECT_TRUE(mime_from_extension(".bashrc").empty());
            EXPECT_TRUE(mime_from_extension("/home/kim/.vimrc").empty());
        }

        TEST(MimeDetectionTest, AnUnknownExtensionHasNoType) {
            EXPECT_TRUE(mime_from_extension("thing.xyzzy").empty());
        }

        // ---- the order they are consulted in -----------------------------------

        TEST(MimeDetectionTest, MagicBytesBeatAMisleadingExtension) {
            // The assertion this function exists for.
            const std::vector<std::byte> zip = bytes(std::string_view{"PK\x03\x04 and more", 13});

            EXPECT_EQ(detect_mime(zip, "notes.txt"), "application/zip");
        }

        TEST(MimeDetectionTest, TheExtensionIsUsedWhenTheContentSaysNothing) {
            const std::vector<std::byte> prose = bytes("# A heading\n\nSome prose.");

            EXPECT_EQ(detect_mime(prose, "README.md"), "text/markdown");
        }

        TEST(MimeDetectionTest, AUserOverrideBeatsBoth) {
            // They can see the file and this cannot.
            const std::vector<std::byte> zip = bytes(std::string_view{"PK\x03\x04 and more", 13});

            EXPECT_EQ(detect_mime(zip, "notes.txt", "text/x-code"), "text/x-code");
        }

        TEST(MimeDetectionTest, NoSignatureAndNoExtensionIsPlainText) {
            // Usually somebody's notes. Refusing it would refuse the commonest
            // thing anybody imports.
            EXPECT_EQ(detect_mime(bytes("some notes"), "notes"), "text/plain");
            EXPECT_EQ(detect_mime({}, ""), "text/plain");
        }

    }  // namespace
}  // namespace typeit::app
