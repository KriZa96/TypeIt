#include "typeit/core/text_supply/WordPoolProvider.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "typeit/core/util/Result.h"

namespace typeit::core {
    namespace {

        /// Unchecked on purpose: every index below sits inside the loop that
        /// bounds it. One accessor with one suppression rather than a
        /// suppression on every line, as elsewhere in this library.
        constexpr char at(std::string_view text, std::size_t index) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            return text[index];
        }

        [[nodiscard]] bool is_space(char letter) {
            return std::isspace(static_cast<unsigned char>(letter)) != 0;
        }

        /// Whitespace-separated tokens, kept as written.
        ///
        /// Not a Unicode word segmentation: a token here is what a typist types
        /// between two spaces, punctuation and all, and splitting `don't` into
        /// three would generate text nobody writes.
        [[nodiscard]] std::vector<std::string_view> tokenise(std::string_view text) {
            std::vector<std::string_view> tokens;
            std::size_t cursor = 0;
            while (cursor < text.size()) {
                while (cursor < text.size() && is_space(at(text, cursor))) {
                    ++cursor;
                }
                const std::size_t start = cursor;
                while (cursor < text.size() && !is_space(at(text, cursor))) {
                    ++cursor;
                }
                if (cursor > start) {
                    tokens.push_back(text.substr(start, cursor - start));
                }
            }
            return tokens;
        }

        /// The token as the options ask for it: stripped of punctuation, or
        /// lowercased, or left exactly as the source wrote it.
        [[nodiscard]] std::string shaped(std::string_view token, const WordPoolOptions& options) {
            std::string out;
            out.reserve(token.size());
            for (const char letter: token) {
                const auto byte = static_cast<unsigned char>(letter);
                // Multi-byte UTF-8 always has the top bit set, so `ispunct`
                // never sees a continuation byte and `é` survives whatever the
                // options say.
                const bool punctuation = byte < 0x80 && std::ispunct(byte) != 0;
                if (punctuation && !options.punctuation) {
                    continue;
                }
                if (options.capitalisation) {
                    out += letter;
                    continue;
                }
                // Only ASCII is folded. `tolower` on a UTF-8 continuation byte
                // is undefined, and folding `Č` needs Unicode case tables this
                // library deliberately does not carry.
                out += byte < 0x80 ? static_cast<char>(std::tolower(byte)) : letter;
            }
            return out;
        }

    }  // namespace

    std::vector<WordWeight> word_frequencies(std::string_view text, WordPoolOptions options) {
        std::vector<WordWeight> pool;
        for (const std::string_view token: tokenise(text)) {
            std::string word = shaped(token, options);
            if (word.size() < options.minimum_length) {
                // Measured after shaping: `a,` is a two-character token and a
                // one-character word, and it is the word that matters.
                continue;
            }

            const auto found = std::ranges::find(pool, word, &WordWeight::word);
            if (found != pool.end()) {
                ++found->count;
                continue;
            }
            // First-appearance order, not sorted: a `std::map` would make the
            // weighted pick depend on the alphabet rather than on the text,
            // which is harmless until two runs are compared and the seeds no
            // longer mean the same thing.
            pool.push_back(WordWeight{.word = std::move(word), .count = 1});
        }
        return pool;
    }

    Result<std::unique_ptr<WordPoolProvider>> WordPoolProvider::create(std::string_view text, std::uint64_t seed,
                                                                       WordPoolOptions options) {
        std::vector<WordWeight> pool = word_frequencies(text, options);
        if (pool.empty()) {
            // A pool of no words is an endless stream of nothing, which hangs a
            // run rather than reporting anything.
            return fail(ErrorCode::EmptyText, "no words survived tokenising this text");
        }
        // Not make_unique: the constructor is private, because a provider over
        // an empty pool is not a thing to hand anybody.
        return std::unique_ptr<WordPoolProvider>{new WordPoolProvider{std::move(pool), seed, options}};
    }

    WordPoolProvider::WordPoolProvider(std::vector<WordWeight> pool, std::uint64_t seed, WordPoolOptions options) :
        pool_{std::move(pool)}, options_{options}, prng_{seed} {
        cumulative_.reserve(pool_.size());
        for (const WordWeight& entry: pool_) {
            total_ += entry.count;
            cumulative_.push_back(total_);
        }
    }

    const std::string& WordPoolProvider::draw() {
        // A point on `[0, total)` and the first running total past it, which is
        // exactly sampling proportional to count.
        const std::size_t point = prng_.below(total_);
        const auto found = std::ranges::upper_bound(cumulative_, point);
        const auto index = static_cast<std::size_t>(found - cumulative_.begin());
        return pool_.at(std::min(index, pool_.size() - 1)).word;
    }

    std::string WordPoolProvider::next_chunk() {
        std::string chunk;
        for (std::size_t at = 0; at < options_.words_per_chunk; ++at) {
            if (at > 0) {
                // Separators between words and never at an edge, so a chunk
                // joined to the next one cannot split a word or double a space.
                chunk += ' ';
            }
            chunk += draw();
        }
        return chunk;
    }

}  // namespace typeit::core
