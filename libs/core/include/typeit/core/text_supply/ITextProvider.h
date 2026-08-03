// Where the text a run types comes from (ADR-013, GAMEPLAY section 5.4).
//
// The successor to `ITextSource`, whose single `get_text()` conflated "where do
// the bytes come from" with "what does the typist see next". Getting content
// into the library is `infra`'s job; serving it during a run is this one's, and
// the difference is what lets endless mode work identically whether the words
// came from a file or a website.
#ifndef TYPEIT_CORE_TEXT_SUPPLY_ITEXTPROVIDER_H
#define TYPEIT_CORE_TEXT_SUPPLY_ITEXTPROVIDER_H

#include <cstdint>
#include <string>

namespace typeit::core {

    class ITextProvider {
    public:
        ITextProvider() = default;
        virtual ~ITextProvider() = default;
        ITextProvider(const ITextProvider&) = delete;
        ITextProvider& operator=(const ITextProvider&) = delete;
        ITextProvider(ITextProvider&&) = delete;
        ITextProvider& operator=(ITextProvider&&) = delete;

        /// The next block of text to type, as UTF-8. Empty exactly when
        /// `has_more()` is false, so a caller that forgets to ask gets nothing
        /// rather than something wrong.
        [[nodiscard]] virtual std::string next_chunk() = 0;

        /// Whether another chunk is coming. False forever for a finite source
        /// that has run out; true forever for an endless one.
        [[nodiscard]] virtual bool has_more() const = 0;

        /// The seed this provider draws from, recorded with the run so that the
        /// same text can be produced again exactly (GAMEPLAY section 5.4). A
        /// provider with nothing random about it still carries one, so that a
        /// caller never has to ask which kind it is holding.
        [[nodiscard]] virtual std::uint64_t seed() const noexcept = 0;
    };

}  // namespace typeit::core

#endif  // TYPEIT_CORE_TEXT_SUPPLY_ITEXTPROVIDER_H
