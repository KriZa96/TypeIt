#include "typeit/core/util/Sha256.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

// SHA-256 is index arithmetic over fixed-size arrays, and every index below is
// bounded by a loop constant that is smaller than the array it indexes. Writing
// it through a bounds-checked accessor would replace something checkable
// against FIPS 180-4 line by line with something that no longer looks like the
// specification — and the check that matters here is Sha256Test's published
// vectors, which a wrong index cannot pass.
//
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
namespace typeit::core {
    namespace {

        /// The first thirty-two bits of the fractional parts of the cube roots
        /// of the first sixty-four primes. FIPS 180-4 §4.2.2.
        constexpr std::array<std::uint32_t, 64> kRoundConstants{{
                0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
                0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
                0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
                0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
                0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
                0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
                0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
                0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
        }};

        /// The fractional parts of the square roots of the first eight primes.
        constexpr std::array<std::uint32_t, 8> kInitialState{{
                0x6A09E667,
                0xBB67AE85,
                0x3C6EF372,
                0xA54FF53A,
                0x510E527F,
                0x9B05688C,
                0x1F83D9AB,
                0x5BE0CD19,
        }};

        constexpr std::size_t kBlockBytes = 64;

        using State = std::array<std::uint32_t, 8>;
        using Block = std::array<unsigned char, kBlockBytes>;

        void compress(State& state, const Block& block) {
            std::array<std::uint32_t, 64> schedule{};
            for (std::size_t i = 0; i < 16; ++i) {
                schedule[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24U) |
                              (static_cast<std::uint32_t>(block[(i * 4) + 1]) << 16U) |
                              (static_cast<std::uint32_t>(block[(i * 4) + 2]) << 8U) |
                              static_cast<std::uint32_t>(block[(i * 4) + 3]);
            }
            for (std::size_t i = 16; i < 64; ++i) {
                const std::uint32_t s0 =
                        std::rotr(schedule[i - 15], 7) ^ std::rotr(schedule[i - 15], 18) ^ (schedule[i - 15] >> 3U);
                const std::uint32_t s1 =
                        std::rotr(schedule[i - 2], 17) ^ std::rotr(schedule[i - 2], 19) ^ (schedule[i - 2] >> 10U);
                schedule[i] = schedule[i - 16] + s0 + schedule[i - 7] + s1;
            }

            State working = state;
            for (std::size_t i = 0; i < 64; ++i) {
                const std::uint32_t s1 =
                        std::rotr(working[4], 6) ^ std::rotr(working[4], 11) ^ std::rotr(working[4], 25);
                const std::uint32_t choice = (working[4] & working[5]) ^ (~working[4] & working[6]);
                const std::uint32_t temp1 = working[7] + s1 + choice + kRoundConstants[i] + schedule[i];
                const std::uint32_t s0 =
                        std::rotr(working[0], 2) ^ std::rotr(working[0], 13) ^ std::rotr(working[0], 22);
                const std::uint32_t majority =
                        (working[0] & working[1]) ^ (working[0] & working[2]) ^ (working[1] & working[2]);
                const std::uint32_t temp2 = s0 + majority;

                working[7] = working[6];
                working[6] = working[5];
                working[5] = working[4];
                working[4] = working[3] + temp1;
                working[3] = working[2];
                working[2] = working[1];
                working[1] = working[0];
                working[0] = temp1 + temp2;
            }

            for (std::size_t i = 0; i < state.size(); ++i) {
                state[i] += working[i];
            }
        }

    }  // namespace

    std::string sha256_hex(std::string_view data) {
        State state = kInitialState;
        Block block{};

        std::size_t offset = 0;
        for (; offset + kBlockBytes <= data.size(); offset += kBlockBytes) {
            for (std::size_t i = 0; i < kBlockBytes; ++i) {
                block[i] = static_cast<unsigned char>(data[offset + i]);
            }
            compress(state, block);
        }

        // The tail: what is left, a 0x80 byte, zeroes, and the length in bits
        // as a big-endian 64-bit number. If the length does not fit in this
        // block it goes in the next one, which is the only fiddly part.
        // Written as a remainder rather than as `size - offset`, which is the
        // same number: gcc at -O2 can prove a modulo is smaller than its
        // divisor and cannot prove the subtraction is, and reports the write
        // below as running off the end of the block.
        const std::size_t remaining = data.size() % kBlockBytes;
        block = {};
        for (std::size_t i = 0; i < remaining; ++i) {
            block[i] = static_cast<unsigned char>(data[offset + i]);
        }
        block[remaining] = 0x80;

        constexpr std::size_t kLengthBytes = 8;
        if (remaining + 1 + kLengthBytes > kBlockBytes) {
            compress(state, block);
            block = {};
        }

        const std::uint64_t bits = std::uint64_t{data.size()} * 8U;
        for (std::size_t i = 0; i < kLengthBytes; ++i) {
            block[kBlockBytes - 1 - i] = static_cast<unsigned char>((bits >> (8U * i)) & 0xFFU);
        }
        compress(state, block);

        constexpr std::string_view kHexDigits = "0123456789abcdef";
        std::string hex;
        hex.reserve(state.size() * 8);
        for (const std::uint32_t word: state) {
            for (int shift = 28; shift >= 0; shift -= 4) {
                hex.push_back(kHexDigits[(word >> static_cast<unsigned>(shift)) & 0xFU]);
            }
        }
        return hex;
    }

}  // namespace typeit::core
// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
