// SHA-256, because deduplication needs a content hash and nothing else here
// provides one.
//
// Not a security boundary: this hashes text the user themselves imported, to
// answer "have I seen this file before". It is SHA-256 rather than something
// cheaper because the answer is stored in a database column that outlives the
// process, and a collision there silently merges two texts.
//
// FIPS 180-4. Sixty lines, no dependency, and testable against the published
// vectors — which is the whole reason not to reach for a library.
#ifndef TYPEIT_CORE_UTIL_SHA256_H
#define TYPEIT_CORE_UTIL_SHA256_H

#include <string>
#include <string_view>

namespace typeit::core {

    /// Lowercase hex, 64 characters. The form the database column holds.
    [[nodiscard]] std::string sha256_hex(std::string_view data);

}  // namespace typeit::core

#endif  // TYPEIT_CORE_UTIL_SHA256_H
