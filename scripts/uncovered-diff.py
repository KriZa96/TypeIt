#!/usr/bin/env python3
"""Which lines this branch added are not covered by any test.

A global coverage percentage barely moves when fifty untested lines land in a
large project, so the percentage is a poor gate on a pull request and the diff
is the one that actually enforces the standard (CI_CD section 7).

    uncovered-diff.py <gcovr.json> <base-ref>

Writes a markdown summary to stdout and exits non-zero if any added line in the
rebuilt tree is uncovered. Advisory by design: the percentage gates in
coverage.sh are what fail a build, and this is what tells a reviewer where to
look.
"""

from __future__ import annotations

import json
import re
import subprocess
import sys
from collections import defaultdict

# Only the rebuilt tree. src/ is the 1.0 code being deleted whole at TI-097.
MEASURED_PREFIX = "libs/"

HUNK = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")


def added_lines(base: str) -> dict[str, set[int]]:
    """Line numbers added or changed by this branch, per file."""
    diff = subprocess.run(
        ["git", "diff", "--unified=0", f"{base}...HEAD", "--", MEASURED_PREFIX],
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    ).stdout

    added: dict[str, set[int]] = defaultdict(set)
    path = ""
    for line in diff.splitlines():
        if line.startswith("+++ b/"):
            path = line[6:]
        elif line.startswith("@@"):
            match = HUNK.match(line)
            if match and path:
                start = int(match.group(1))
                count = int(match.group(2) or 1)
                added[path].update(range(start, start + count))
    return added


# gcov attributes a function's epilogue to its closing brace and emits a line
# for every defaulted special member, so a report that took those literally
# would list a closing brace on every function that returns by value. They are
# noise, not gaps, and a report full of noise is one nobody reads.
NOISE = re.compile(r"^\s*(\}\s*;?\s*(//.*)?|.*=\s*default;\s*(//.*)?)$")


def is_noise(path: str, number: int) -> bool:
    try:
        with open(path, encoding="utf-8") as handle:
            for index, text in enumerate(handle, start=1):
                if index == number:
                    return bool(NOISE.match(text))
    except OSError:
        return False
    return False


def uncovered(report: str) -> dict[str, set[int]]:
    with open(report, encoding="utf-8") as handle:
        data = json.load(handle)

    missed: dict[str, set[int]] = defaultdict(set)
    for entry in data.get("files", []):
        for line in entry.get("lines", []):
            if line["count"] == 0 and not is_noise(entry["file"], line["line_number"]):
                missed[entry["file"]].add(line["line_number"])
    return missed


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2

    report, base = sys.argv[1], sys.argv[2]
    added = added_lines(base)
    missed = uncovered(report)

    findings = {
        path: sorted(lines & missed.get(path, set()))
        for path, lines in added.items()
        if lines & missed.get(path, set())
    }

    if not findings:
        print("### Coverage of new lines\n")
        print("Every line this branch adds under `libs/` is covered by a test.")
        return 0

    total = sum(len(lines) for lines in findings.values())
    print("### Uncovered new lines\n")
    print(f"{total} line(s) added by this branch are not reached by any test.\n")
    print("| File | Lines |")
    print("|---|---|")
    for path, lines in sorted(findings.items()):
        shown = ", ".join(str(line) for line in lines[:20])
        if len(lines) > 20:
            shown += f", … (+{len(lines) - 20})"
        print(f"| `{path}` | {shown} |")
    print(
        "\nUncovered code in `core` is a review finding: there is nothing to stand up, "
        "no I/O to fake and no dependency to blame (TESTING section 9)."
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
