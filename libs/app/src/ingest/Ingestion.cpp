#include "typeit/app/ingest/Ingestion.h"

#include <string_view>

namespace typeit::app {

    // Anchored here rather than defaulted in the header: a polymorphic class
    // whose vtable has no home emits one in every translation unit that
    // includes it, and the linker keeps them all.
    IContentFetcher::~IContentFetcher() = default;
    ITextExtractor::~ITextExtractor() = default;

    std::string_view as_text(const FetchedContent& content) {
        // `std::byte` and `char` are the two spellings of a byte. This is the
        // one place that crosses between them, rather than every extractor
        // writing its own cast.
        //
        // The directive has to sit on the line immediately above the cast:
        // separated from it by so much as this comment, it applies to the
        // comment and the build fails with the warning it was meant to silence.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return {reinterpret_cast<const char*>(content.bytes.data()), content.bytes.size()};
    }

}  // namespace typeit::app
