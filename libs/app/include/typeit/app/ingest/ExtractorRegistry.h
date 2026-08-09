// MIME type → extractor (TX-001).
//
// The seam that makes adding a format one new file plus one registration.
//
// **Two extractors claiming one type is a startup error, not last-wins.** A
// silent overwrite means the build order decides which one runs, and the
// symptom is an EPUB extracted as a ZIP on one machine and correctly on
// another — a bug nobody reproduces.
#ifndef TYPEIT_APP_INGEST_EXTRACTORREGISTRY_H
#define TYPEIT_APP_INGEST_EXTRACTORREGISTRY_H

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "typeit/app/ingest/Ingestion.h"
#include "typeit/core/util/Result.h"

namespace typeit::app {

    class ExtractorRegistry {
    public:
        /// Claims every type the extractor names.
        ///
        /// Fails, rather than replacing, if any of them is already claimed —
        /// and claims **none** of them in that case, so a rejected
        /// registration leaves the registry as it was rather than half
        /// applied.
        [[nodiscard]] core::Status add(const std::shared_ptr<ITextExtractor>& extractor);

        /// The extractor for a type, or `nullptr`.
        ///
        /// A parameter after the type — `text/plain;charset=utf-8` — is matched
        /// on the type alone: a charset is a detail of the bytes, and an
        /// extractor that handled `text/plain` and not `text/plain;charset=…`
        /// would fail on exactly the files somebody bothered to label.
        [[nodiscard]] const ITextExtractor* find(std::string_view mime) const;

        /// Every registered type, sorted. For `--doctor`, and for a test that
        /// wants to say what is registered rather than infer it.
        [[nodiscard]] std::vector<std::string> types() const;

        [[nodiscard]] bool empty() const noexcept { return by_mime_.empty(); }

    private:
        std::map<std::string, std::shared_ptr<ITextExtractor>, std::less<>> by_mime_;
    };

}  // namespace typeit::app

#endif  // TYPEIT_APP_INGEST_EXTRACTORREGISTRY_H
