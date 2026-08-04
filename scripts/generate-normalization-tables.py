#!/usr/bin/env python3
"""Generate the NFC tables from the Unicode Character Database.

    scripts/generate-normalization-tables.py [--check]

Writes libs/core/include/typeit/core/text/NormalizationTables.h. With --check
it regenerates into memory and fails if the committed header differs, which is
what stops the tables drifting from the data they came from.

The same argument as the width tables: hand-maintained Unicode data rots, and
here the symptom is that `e` + U+0301 and `é` are two different texts, so the
same file imported from two editors deduplicates into two library entries. The
inputs are downloaded rather than vendored and pinned by version and SHA-256.

Hangul is deliberately absent: its composition is arithmetic (UAX #15, §16)
and a table of eleven thousand syllables would be data standing in for four
lines of code.
"""
import argparse
import hashlib
import re
import sys
import urllib.request
from pathlib import Path

# Pinned, and the same release the width tables came from. Bumping this is a
# deliberate act: run without --check, review the diff, record it in the
# changelog.
UNICODE_VERSION = "17.0.0"

SOURCES = {
    "UnicodeData.txt": f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd/UnicodeData.txt",
    "CompositionExclusions.txt": (
        f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd/CompositionExclusions.txt"
    ),
    "DerivedGeneralCategory.txt": (
        f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd/extracted/DerivedGeneralCategory.txt"
    ),
}

EXPECTED_SHA256 = {
    "UnicodeData.txt": "2e1efc1dcb59c575eedf5ccae60f95229f706ee6d031835247d843c11d96470c",
    "CompositionExclusions.txt": "2f239196ef3b5b61db5cc476e9bd80f534d15aa1b74e1be1dea5d042a344c85f",
    "DerivedGeneralCategory.txt": "d62e5bab70ca74f099343f71224fa051cb1fdd61a1ab45c0488c44cfc0b6102e",
}

# Every punctuation category. Symbols (S*) are left alone: stripping `+` and
# `=` out of a maths text would not simplify it, it would break it.
PUNCTUATION_CATEGORIES = {"Pc", "Pd", "Ps", "Pe", "Pi", "Pf", "Po"}

CATEGORY_LINE = re.compile(r"^([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))?\s*;\s*([^#\s]+)")

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "libs/core/include/typeit/core/text/NormalizationTables.h"
CACHE = ROOT / "build/unicode-data"


def fetch(name: str) -> str:
    """Download once per checkout, then reuse. Verified against the pin."""
    CACHE.mkdir(parents=True, exist_ok=True)
    cached = CACHE / f"{UNICODE_VERSION}-{name}"
    if not cached.exists():
        with urllib.request.urlopen(SOURCES[name], timeout=60) as response:
            cached.write_bytes(response.read())

    payload = cached.read_bytes()
    digest = hashlib.sha256(payload).hexdigest()
    expected = EXPECTED_SHA256.get(name)
    if expected and digest != expected:
        sys.exit(f"{name}: sha256 {digest} does not match the pinned {expected}")
    if not expected:
        print(f"# {name} sha256 = {digest}", file=sys.stderr)
    return payload.decode("utf-8")


def parse_unicode_data(
    text: str,
) -> tuple[dict[int, int], dict[int, list[int]], dict[int, int]]:
    """Combining classes, canonical decompositions and lowercase mappings.

    Compatibility decompositions — the ones tagged `<font>`, `<circle>` and
    friends — are skipped: NFC leaves them alone, and folding them in would
    turn `ﬁ` into `fi` behind the user's back.
    """
    combining: dict[int, int] = {}
    decomposition: dict[int, list[int]] = {}
    lowercase: dict[int, int] = {}
    for line in text.splitlines():
        fields = line.split(";")
        if len(fields) < 14:
            continue
        code = int(fields[0], 16)
        if fields[3] != "0":
            combining[code] = int(fields[3])
        mapping = fields[5].strip()
        if mapping and not mapping.startswith("<"):
            decomposition[code] = [int(part, 16) for part in mapping.split()]
        if fields[13].strip():
            lowercase[code] = int(fields[13], 16)
    return combining, decomposition, lowercase


def parse_punctuation(text: str) -> list[tuple[int, int]]:
    """Every code point Unicode calls punctuation, coalesced into ranges."""
    found: list[tuple[int, int]] = []
    for line in text.splitlines():
        line = line.split("#", 1)[0]
        match = CATEGORY_LINE.match(line)
        if match is None or match.group(3) not in PUNCTUATION_CATEGORIES:
            continue
        first = int(match.group(1), 16)
        found.append((first, int(match.group(2), 16) if match.group(2) else first))

    merged: list[tuple[int, int]] = []
    for first, last in sorted(found):
        if merged and first <= merged[-1][1] + 1:
            merged[-1] = (merged[-1][0], max(merged[-1][1], last))
        else:
            merged.append((first, last))
    return merged


def parse_exclusions(text: str) -> set[int]:
    """The exclusions that have to be listed because they cannot be derived."""
    excluded: set[int] = set()
    for line in text.splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        excluded.add(int(line.split("..", 1)[0], 16))
    return excluded


def merge_combining(combining: dict[int, int]) -> list[tuple[int, int, int]]:
    """Coalesce equal classes into ranges so the lookup binary searches a few
    hundred entries rather than a few thousand."""
    merged: list[tuple[int, int, int]] = []
    for code in sorted(combining):
        klass = combining[code]
        if merged and merged[-1][2] == klass and code == merged[-1][1] + 1:
            merged[-1] = (merged[-1][0], code, klass)
        else:
            merged.append((code, code, klass))
    return merged


def compositions(
    decomposition: dict[int, list[int]],
    combining: dict[int, int],
    listed_exclusions: set[int],
) -> list[tuple[int, int, int]]:
    """The pairs that recompose, per UAX #15's Composition_Exclusion.

    Three kinds are excluded. The script-listed ones, which are historical.
    Singletons, because a decomposition of one is a spelling correction and
    recomposing it would undo it. And non-starter decompositions, whose first
    character already carries a combining class — composing those would
    reorder the sequence.
    """
    pairs: list[tuple[int, int, int]] = []
    for code, mapping in sorted(decomposition.items()):
        if len(mapping) != 2:
            continue
        if code in listed_exclusions:
            continue
        if combining.get(mapping[0], 0) != 0:
            continue
        pairs.append((mapping[0], mapping[1], code))
    return sorted(pairs)


def format_combining(ranges: list[tuple[int, int, int]]) -> str:
    rows = "\n".join(
        f"        {{0x{first:04X}, 0x{last:04X}, {klass}}}," for first, last, klass in ranges
    )
    return (
        "/// Canonical combining classes, coalesced into ranges. Everything\n"
        "/// absent is class 0, which is most of Unicode.\n"
        f"inline constexpr std::array<CombiningClassRange, {len(ranges)}> kCombiningClasses{{{{\n"
        f"{rows}\n"
        "}};\n"
    )


def format_decompositions(entries: list[tuple[int, list[int]]]) -> str:
    rows = "\n".join(
        f"        {{0x{code:04X}, 0x{mapping[0]:04X}, "
        f"0x{(mapping[1] if len(mapping) > 1 else 0):04X}}},"
        for code, mapping in entries
    )
    return (
        "/// Canonical decompositions, sorted by code point. A `second` of zero\n"
        "/// is a singleton. These are the one-step mappings, not the fully\n"
        "/// expanded ones — the decomposer recurses, which costs three\n"
        "/// iterations at worst and saves the table repeating itself.\n"
        f"inline constexpr std::array<Decomposition, {len(entries)}> kCanonicalDecompositions{{{{\n"
        f"{rows}\n"
        "}};\n"
    )


def format_compositions(pairs: list[tuple[int, int, int]]) -> str:
    rows = "\n".join(
        f"        {{0x{first:04X}, 0x{second:04X}, 0x{composite:04X}}},"
        for first, second, composite in pairs
    )
    return (
        "/// The pairs that recompose, sorted by (first, second) so the\n"
        "/// composer can binary search. Excludes the singletons, the\n"
        "/// non-starter decompositions and the script-listed exclusions.\n"
        f"inline constexpr std::array<Composition, {len(pairs)}> kCanonicalCompositions{{{{\n"
        f"{rows}\n"
        "}};\n"
    )


def format_punctuation(ranges: list[tuple[int, int]]) -> str:
    rows = "\n".join(f"        {{0x{first:04X}, 0x{last:04X}}}," for first, last in ranges)
    return (
        "/// Pc, Pd, Ps, Pe, Pi, Pf and Po: what the simplify toggle removes.\n"
        f"inline constexpr std::array<CodeRange, {len(ranges)}> kPunctuation{{{{\n"
        f"{rows}\n"
        "}};\n"
    )


def format_lowercase(mappings: list[tuple[int, int]]) -> str:
    rows = "\n".join(f"        {{0x{upper:04X}, 0x{lower:04X}}}," for upper, lower in mappings)
    return (
        "/// Simple lowercase mappings, sorted by code point. Simple, not full:\n"
        "/// the mappings that grow (German sharp s to `ss`) are conditional on\n"
        "/// language, and a typing test that quietly lengthens the text has\n"
        "/// changed what the user agreed to type.\n"
        f"inline constexpr std::array<CaseMapping, {len(mappings)}> kSimpleLowercase{{{{\n"
        f"{rows}\n"
        "}};\n"
    )


def generate() -> str:
    combining, decomposition, lowercase = parse_unicode_data(fetch("UnicodeData.txt"))
    listed = parse_exclusions(fetch("CompositionExclusions.txt"))
    punctuation = parse_punctuation(fetch("DerivedGeneralCategory.txt"))

    combining_ranges = merge_combining(combining)
    decompositions = sorted(decomposition.items())
    pairs = compositions(decomposition, combining, listed)
    lowercase_pairs = sorted(lowercase.items())

    return f"""// GENERATED FILE — do not edit.
//
// Produced by scripts/generate-normalization-tables.py from the Unicode
// Character Database, version {UNICODE_VERSION}. Regenerate rather than patch: a
// hand-edited entry here disagrees with the next regeneration, and the symptom
// is the same text importing twice under two different hashes.
#ifndef TYPEIT_CORE_TEXT_NORMALIZATIONTABLES_H
#define TYPEIT_CORE_TEXT_NORMALIZATIONTABLES_H

#include <array>
#include <cstdint>

namespace typeit::core::tables {{

struct CombiningClassRange {{
    char32_t first;
    char32_t last;
    std::uint8_t combining_class;
}};

struct Decomposition {{
    char32_t code;
    char32_t first;
    char32_t second;  ///< Zero for a singleton decomposition.
}};

struct Composition {{
    char32_t first;
    char32_t second;
    char32_t composite;
}};

struct CodeRange {{
    char32_t first;
    char32_t last;
}};

struct CaseMapping {{
    char32_t from;
    char32_t to;
}};

/// The Unicode release these tables were generated from.
inline constexpr const char* kNormalizationUnicodeVersion = "{UNICODE_VERSION}";

{format_combining(combining_ranges)}
{format_decompositions(decompositions)}
{format_compositions(pairs)}
{format_punctuation(punctuation)}
{format_lowercase(lowercase_pairs)}
}}  // namespace typeit::core::tables

#endif  // TYPEIT_CORE_TEXT_NORMALIZATIONTABLES_H
"""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="fail if the committed header differs from a fresh generation",
    )
    arguments = parser.parse_args()

    generated = generate()
    if arguments.check:
        current = HEADER.read_text(encoding="utf-8") if HEADER.exists() else ""
        if current != generated:
            print(
                f"{HEADER.relative_to(ROOT)} is not what the generator produces. "
                "Run scripts/generate-normalization-tables.py and commit the result.",
                file=sys.stderr,
            )
            return 1
        print(f"{HEADER.relative_to(ROOT)} matches Unicode {UNICODE_VERSION}.")
        return 0

    HEADER.write_text(generated, encoding="utf-8")
    print(f"wrote {HEADER.relative_to(ROOT)} from Unicode {UNICODE_VERSION}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
