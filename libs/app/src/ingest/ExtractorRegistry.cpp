#include "typeit/app/ingest/ExtractorRegistry.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/core/util/Result.h"

namespace typeit::app {
    namespace {

        /// The type without its parameters: `text/plain;charset=utf-8` is
        /// `text/plain`. A charset is a detail of the bytes, and an extractor
        /// that handled the bare type but not the labelled one would fail on
        /// exactly the files somebody bothered to label.
        [[nodiscard]] std::string_view bare(std::string_view mime) {
            const std::size_t parameter = mime.find(';');
            return parameter == std::string_view::npos ? mime : mime.substr(0, parameter);
        }

    }  // namespace

    core::Status ExtractorRegistry::add(const std::shared_ptr<ITextExtractor>& extractor) {
        if (extractor == nullptr) {
            return core::fail(core::ErrorCode::InvalidArgument, "an extractor registration needs an extractor");
        }

        // Checked before anything is inserted, so a rejected registration
        // leaves the registry as it was rather than half applied — the second
        // of two types claimed while the first was refused would be a registry
        // nobody could reason about.
        for (const std::string_view mime: extractor->mime_types()) {
            if (by_mime_.contains(bare(mime))) {
                // Not last-wins: a silent overwrite lets the build order decide
                // which extractor runs, and the symptom is an EPUB extracted as
                // a ZIP on one machine and correctly on another.
                return core::fail(core::ErrorCode::InvalidArgument,
                                  std::string{mime} + " is already claimed by another extractor");
            }
        }

        for (const std::string_view mime: extractor->mime_types()) {
            by_mime_.emplace(std::string{bare(mime)}, extractor);
        }
        return {};
    }

    const ITextExtractor* ExtractorRegistry::find(std::string_view mime) const {
        const auto found = by_mime_.find(bare(mime));
        return found == by_mime_.end() ? nullptr : found->second.get();
    }

    std::vector<std::string> ExtractorRegistry::types() const {
        std::vector<std::string> types;
        types.reserve(by_mime_.size());
        for (const auto& [mime, extractor]: by_mime_) {
            types.push_back(mime);
        }
        return types;
    }

}  // namespace typeit::app
