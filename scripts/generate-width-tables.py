#!/usr/bin/env python3
"""Generate the display width tables from the Unicode Character Database.

    scripts/generate-width-tables.py [--check]

Writes libs/core/include/typeit/core/text/WidthTables.h. With --check it
regenerates into memory and fails if the committed header differs, which is
what stops the tables drifting from the data they came from.

Hand-maintained width ranges rot: every Unicode release moves something, and
the symptom is a cursor one column off in the middle of a typing run. The
inputs are downloaded rather than vendored — 3 MB of text that is only read by
this script — and pinned by version and SHA-256, so a regeneration either
produces the same header or fails loudly.
"""
import argparse
import hashlib
import re
import sys
import urllib.request
from pathlib import Path

# Pinned. Bumping this is a deliberate act: run without --check, review the
# diff in the generated header, and record the version in the changelog.
UNICODE_VERSION = "17.0.0"

SOURCES = {
    "EastAsianWidth.txt": f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd/EastAsianWidth.txt",
    "emoji-data.txt": f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd/emoji/emoji-data.txt",
    "DerivedGeneralCategory.txt": (
        f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd/extracted/DerivedGeneralCategory.txt"
    ),
}

# Verified on every run. A file that changes under a pinned version means the
# download was tampered with or the mirror is wrong; either way, stop.
EXPECTED_SHA256 = {
    "EastAsianWidth.txt": "ea7ce50f3444a050333448dffef1cadd9325af55cbb764b4a2280faf52170a33",
    "emoji-data.txt": "2cb2bb9455cda83e8481541ecf5b6dfda66a3bb89efa3fa7c5297eccf607b72b",
    "DerivedGeneralCategory.txt": "d62e5bab70ca74f099343f71224fa051cb1fdd61a1ab45c0488c44cfc0b6102e",
}

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "libs/core/include/typeit/core/text/WidthTables.h"
CACHE = ROOT / "build/unicode-data"

RANGE_LINE = re.compile(
    r"^([0-9A-F]{4,6})(?:\.\.([0-9A-F]{4,6}))?\s*;\s*([^#\s]+)"
)


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


def parse(text: str, wanted: set[str]) -> list[tuple[int, int]]:
    """Every range whose property is in `wanted`, as (first, last) pairs."""
    found: list[tuple[int, int]] = []
    for line in text.splitlines():
        line = line.split("#", 1)[0]
        match = RANGE_LINE.match(line)
        if match is None:
            continue
        if match.group(3) not in wanted:
            continue
        first = int(match.group(1), 16)
        last = int(match.group(2), 16) if match.group(2) else first
        found.append((first, last))
    return found


def merge(ranges: list[tuple[int, int]]) -> list[tuple[int, int]]:
    """Sort and coalesce, so the lookup can binary search and the output is
    stable whatever order the source files list things in."""
    merged: list[tuple[int, int]] = []
    for first, last in sorted(ranges):
        if merged and first <= merged[-1][1] + 1:
            merged[-1] = (merged[-1][0], max(merged[-1][1], last))
        else:
            merged.append((first, last))
    return merged


def format_table(name: str, ranges: list[tuple[int, int]], comment: str) -> str:
    rows = "\n".join(
        f"        {{0x{first:04X}, 0x{last:04X}}}," for first, last in ranges
    )
    return (
        f"/// {comment}\n"
        f"inline constexpr std::array<CodePointRange, {len(ranges)}> {name}{{{{\n"
        f"{rows}\n"
        "}};\n"
    )


def generate() -> str:
    east_asian = fetch("EastAsianWidth.txt")
    emoji = fetch("emoji-data.txt")
    categories = fetch("DerivedGeneralCategory.txt")

    # Wide and Fullwidth are two columns by definition; Emoji_Presentation is
    # what makes an emoji render as two rather than one.
    wide = merge(parse(east_asian, {"W", "F"}) + parse(emoji, {"Emoji_Presentation"}))

    # Non-spacing and enclosing marks hang off the previous character and take
    # no column of their own. Mc is spacing and keeps its column. Cf covers the
    # joiners and variation selectors; Cc the controls, which are never
    # rendered as themselves.
    zero = merge(parse(categories, {"Mn", "Me", "Cf", "Cc"}))

    return f"""// GENERATED FILE — do not edit.
//
// Produced by scripts/generate-width-tables.py from the Unicode Character
// Database, version {UNICODE_VERSION}. Regenerate rather than patch: a hand-edited
// range here disagrees with the next regeneration and the symptom is a cursor
// one column out of place.
#ifndef TYPEIT_CORE_TEXT_WIDTHTABLES_H
#define TYPEIT_CORE_TEXT_WIDTHTABLES_H

#include <array>
#include <cstddef>

namespace typeit::core::tables {{

struct CodePointRange {{
    char32_t first;
    char32_t last;
}};

/// The Unicode release these tables were generated from.
inline constexpr const char* kUnicodeVersion = "{UNICODE_VERSION}";

{format_table("kWide", wide, "East Asian Wide and Fullwidth, plus Emoji_Presentation: two columns.")}
{format_table("kZeroWidth", zero, "Mn, Me, Cf and Cc: no column of their own.")}
}}  // namespace typeit::core::tables

#endif  // TYPEIT_CORE_TEXT_WIDTHTABLES_H
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
                "Run scripts/generate-width-tables.py and commit the result.",
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
