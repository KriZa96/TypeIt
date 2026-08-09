// MIME type → extractor, and the fetchers behind it (TX-001).

#include <algorithm>
#include <array>
#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/app/ingest/ExtractorRegistry.h"
#include "typeit/app/ingest/Ingestion.h"
#include "typeit/app/ingest/PlainText.h"
#include "typeit/core/util/Result.h"
#include "typeit/testing/Fakes.h"

namespace typeit::app {
    namespace {

        using core::ErrorCode;

        /// An extractor that claims whatever it was told to and returns its own
        /// name, so a test can see which one ran.
        class SpyExtractor final : public ITextExtractor {
        public:
            SpyExtractor(std::string name, std::vector<std::string> types) :
                name_{std::move(name)}, storage_{std::move(types)} {
                views_.reserve(storage_.size());
                for (const std::string& type: storage_) {
                    views_.emplace_back(type);
                }
            }

            [[nodiscard]] std::string_view name() const override { return name_; }

            [[nodiscard]] std::span<const std::string_view> mime_types() const override { return views_; }

            [[nodiscard]] core::Result<ExtractedText> extract(const FetchedContent& /*content*/) const override {
                ExtractedText extracted;
                extracted.text = name_;
                return extracted;
            }

        private:
            std::string name_;
            std::vector<std::string> storage_;
            std::vector<std::string_view> views_;
        };

        std::shared_ptr<SpyExtractor> spy(std::string name, std::vector<std::string> types) {
            return std::make_shared<SpyExtractor>(std::move(name), std::move(types));
        }

        // ---- the registry ------------------------------------------------------

        TEST(ExtractorRegistryTest, EachTypeResolvesToItsExtractor) {
            ExtractorRegistry registry;
            ASSERT_TRUE(registry.add(spy("plain", {"text/plain"})));
            ASSERT_TRUE(registry.add(spy("markdown", {"text/markdown"})));

            const ITextExtractor* const found = registry.find("text/markdown");

            ASSERT_NE(found, nullptr);
            EXPECT_EQ(found->extract({})->text, "markdown");
        }

        TEST(ExtractorRegistryTest, AnUnknownTypeResolvesToNothingRatherThanToAnything) {
            ExtractorRegistry registry;
            ASSERT_TRUE(registry.add(spy("plain", {"text/plain"})));

            EXPECT_EQ(registry.find("application/epub+zip"), nullptr);
        }

        TEST(ExtractorRegistryTest, TwoExtractorsClaimingOneTypeIsAnErrorRatherThanLastWins) {
            // A silent overwrite lets the build order decide which extractor
            // runs, and the symptom is an EPUB extracted as a ZIP on one
            // machine and correctly on another — a bug nobody reproduces.
            ExtractorRegistry registry;
            ASSERT_TRUE(registry.add(spy("first", {"text/plain"})));

            const core::Status second = registry.add(spy("second", {"text/plain"}));

            ASSERT_FALSE(second);
            EXPECT_EQ(second.error().code, ErrorCode::InvalidArgument);
            EXPECT_NE(second.error().context.find("text/plain"), std::string::npos) << second.error().context;
            EXPECT_EQ(registry.find("text/plain")->extract({})->text, "first") << "and the first one stands";
        }

        TEST(ExtractorRegistryTest, ARejectedRegistrationClaimsNoneOfItsTypes) {
            // Half-applied is worse than refused: the second of two types
            // claimed while the first was rejected would be a registry nobody
            // could reason about.
            ExtractorRegistry registry;
            ASSERT_TRUE(registry.add(spy("first", {"text/plain"})));

            ASSERT_FALSE(registry.add(spy("second", {"text/plain", "text/markdown"})));

            EXPECT_EQ(registry.find("text/markdown"), nullptr) << "the type it would also have claimed";
        }

        TEST(ExtractorRegistryTest, ACharsetParameterIsMatchedOnTheTypeAlone) {
            // A charset is a detail of the bytes. An extractor that handled
            // `text/plain` but not `text/plain;charset=utf-8` would fail on
            // exactly the files somebody bothered to label.
            ExtractorRegistry registry;
            ASSERT_TRUE(registry.add(spy("plain", {"text/plain"})));

            EXPECT_NE(registry.find("text/plain;charset=utf-8"), nullptr);
            EXPECT_NE(registry.find("text/plain; charset=utf-16le"), nullptr);
        }

        TEST(ExtractorRegistryTest, RegisteringNothingIsRefused) {
            ExtractorRegistry registry;

            EXPECT_FALSE(registry.add(nullptr));
            EXPECT_TRUE(registry.empty());
        }

        TEST(ExtractorRegistryTest, TheRegisteredTypesCanBeListed) {
            ExtractorRegistry registry;
            ASSERT_TRUE(registry.add(spy("one", {"text/plain", "text/x-code"})));

            EXPECT_EQ(registry.types(), (std::vector<std::string>{"text/plain", "text/x-code"}));
        }

        // ---- the fetchers ------------------------------------------------------

        TEST(FetcherTest, TheFileFetcherReadsBytesAndNamesThem) {
            testing::FakeFileSystem files;
            files.add_file("/home/kim/notes.md", "# A heading");
            const FileFetcher fetcher{files};

            const core::Result<FetchedContent> fetched = fetcher.fetch("/home/kim/notes.md");

            ASSERT_TRUE(fetched) << (fetched ? "" : fetched.error().context);
            EXPECT_EQ(as_text(*fetched), "# A heading");
            EXPECT_EQ(fetched->detected_mime, "text/markdown");
            EXPECT_EQ(fetched->origin, "/home/kim/notes.md");
            EXPECT_EQ(fetched->suggested_title, "notes") << "the name without its extension";
        }

        TEST(FetcherTest, TheFileFetcherPassesThePortsErrorsThrough) {
            // A missing file and a directory are different fixes, so they stay
            // different errors — the same codes Phase 6 gave.
            testing::FakeFileSystem files;
            files.add_directory("/home/kim/somewhere");
            const FileFetcher fetcher{files};

            const core::Result<FetchedContent> missing = fetcher.fetch("/nowhere.txt");
            const core::Result<FetchedContent> directory = fetcher.fetch("/home/kim/somewhere");

            ASSERT_FALSE(missing);
            ASSERT_FALSE(directory);
            EXPECT_EQ(missing.error().code, ErrorCode::FileNotFound);
            EXPECT_EQ(directory.error().code, ErrorCode::FileUnreadable);
        }

        TEST(FetcherTest, TheFileFetcherClaimsAnythingThatIsNotAnotherFetchersShape) {
            testing::FakeFileSystem files;
            const FileFetcher fetcher{files};

            EXPECT_TRUE(fetcher.can_handle("/home/kim/notes.txt"));
            EXPECT_TRUE(fetcher.can_handle("relative/path")) << "including one that does not exist yet";
            EXPECT_FALSE(fetcher.can_handle("-")) << "which is standard input";
            EXPECT_FALSE(fetcher.can_handle(""));
        }

        TEST(FetcherTest, TheMemoryFetcherHandsBackWhatItWasGiven) {
            const MemoryFetcher fetcher{"pasted prose", "<paste>"};

            const core::Result<FetchedContent> fetched = fetcher.fetch("<paste>");

            ASSERT_TRUE(fetched);
            EXPECT_EQ(as_text(*fetched), "pasted prose");
            EXPECT_EQ(fetched->origin, "<paste>");
            EXPECT_EQ(fetched->detected_mime, "text/plain") << "detected from content, since there is no name";
        }

        TEST(FetcherTest, TheMemoryFetcherClaimsOnlyItsOwnLocator) {
            // One that answered "yes" to everything would swallow every path in
            // a registry that asked it first.
            const MemoryFetcher fetcher{"pasted prose", "<paste>"};

            EXPECT_TRUE(fetcher.can_handle("<paste>"));
            EXPECT_FALSE(fetcher.can_handle("/home/kim/notes.txt"));
            EXPECT_FALSE(fetcher.fetch("/home/kim/notes.txt"));
        }

        // ---- the plain-text extractor ------------------------------------------

        TEST(PlainTextExtractorTest, ItPassesTheBytesThroughUnchanged) {
            const PlainTextExtractor extractor;
            FetchedContent content;
            content.bytes = {std::byte{'h'}, std::byte{'i'}};
            content.suggested_title = "greeting";

            const core::Result<ExtractedText> extracted = extractor.extract(content);

            ASSERT_TRUE(extracted);
            EXPECT_EQ(extracted->text, "hi");
            ASSERT_TRUE(extracted->title.has_value());
            EXPECT_EQ(*extracted->title, "greeting");
            EXPECT_TRUE(extracted->sections.empty()) << "a plain file is one undivided text";
        }

        TEST(PlainTextExtractorTest, ItDoesNotValidateTheUtf8) {
            // Normalisation does that, and reports the byte offset. A second
            // opinion here would be one with a worse message.
            const PlainTextExtractor extractor;
            FetchedContent content;
            content.bytes = {std::byte{0xFF}, std::byte{0xFE}};

            EXPECT_TRUE(extractor.extract(content));
        }

        TEST(PlainTextExtractorTest, ItIsDownToPlainTextAlone) {
            // Markdown, code and subtitles were parked here through TX-001 so
            // that nothing which imported before the split stopped importing
            // during it. TX-002, TX-003 and TX-004 have each taken theirs back.
            const PlainTextExtractor extractor;
            const std::span<const std::string_view> types = extractor.mime_types();

            EXPECT_NE(std::ranges::find(types, "text/plain"), types.end());
            for (const std::string_view taken: {"text/markdown", "text/x-code", "text/x-subrip", "text/vtt"}) {
                EXPECT_EQ(std::ranges::find(types, taken), types.end()) << taken << " has its own extractor now";
            }
        }

    }  // namespace
}  // namespace typeit::app
