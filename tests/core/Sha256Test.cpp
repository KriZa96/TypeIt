// SHA-256 against the published vectors.
//
// The whole reason to hand-roll a hash rather than take a dependency is that
// its correctness is checkable from a table anyone can look up. So it is
// checked from a table anyone can look up.

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <string_view>

#include "typeit/core/util/Sha256.h"

namespace typeit::core {
    namespace {

        TEST(Sha256Test, TheFipsVectors) {
            struct Vector {
                std::string_view input;
                std::string_view digest;
            };
            const Vector vectors[] = {
                    {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
                    {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
                    {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
            };
            for (const Vector& vector: vectors) {
                EXPECT_EQ(sha256_hex(vector.input), vector.digest) << vector.input;
            }
        }

        TEST(Sha256Test, TheMillionAVector) {
            // A million characters, which is what exercises the block loop
            // rather than the padding.
            EXPECT_EQ(sha256_hex(std::string(1'000'000, 'a')),
                      "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
        }

        TEST(Sha256Test, TheLengthGoesIntoAnExtraBlockWhenItDoesNotFit) {
            // 56 bytes of message leaves no room for the 0x80 and the eight
            // length bytes, which is the one branch in the padding that is
            // easy to get wrong and hard to notice.
            for (const std::size_t length: {55U, 56U, 57U, 63U, 64U, 65U}) {
                EXPECT_EQ(sha256_hex(std::string(length, 'x')).size(), 64U) << length;
            }
            EXPECT_EQ(sha256_hex(std::string(56, 'x')),
                      "04c26261370ee7541549d16dee320c723e3fd14671e66a099afe0a377c16888e");
        }

        TEST(Sha256Test, TheSameInputAlwaysHashesTheSame) {
            EXPECT_EQ(sha256_hex("the quick brown fox"), sha256_hex("the quick brown fox"));
            EXPECT_NE(sha256_hex("the quick brown fox"), sha256_hex("the quick brown fox."));
        }

        TEST(Sha256Test, EmbeddedNulsAreHashedRatherThanEndingTheInput) {
            using namespace std::string_view_literals;
            EXPECT_NE(sha256_hex("a\0b"sv), sha256_hex("a"sv));
        }

    }  // namespace
}  // namespace typeit::core
