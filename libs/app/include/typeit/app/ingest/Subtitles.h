// Subtitles, as typing material (TX-004, TEXT_SOURCES §4).
//
// Unusually good material, and not obviously a document format. Subtitles are
// natural conversational prose with realistic punctuation — contractions,
// questions, interruptions — they are freely available in quantity, and they
// are already cut into short lines.
//
// That last property is also the problem. A cue is wrapped to fit a screen, so
// its line breaks fall wherever the width ran out rather than where the
// sentence did. Typing them as written means typing a sentence broken in three
// arbitrary places, which is a test of the subtitler's line-wrapping and not of
// anybody's typing. So the cues are re-joined into sentences.
#ifndef TYPEIT_APP_INGEST_SUBTITLES_H
#define TYPEIT_APP_INGEST_SUBTITLES_H

#include <span>
#include <string_view>

#include "typeit/app/ingest/Ingestion.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {

    /// SubRip and WebVTT, stripped back to what was said.
    class SubtitleExtractor final : public ITextExtractor {
    public:
        [[nodiscard]] std::span<const std::string_view> mime_types() const override;

        [[nodiscard]] core::Result<ExtractedText> extract(const FetchedContent& content) const override;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_INGEST_SUBTITLES_H
