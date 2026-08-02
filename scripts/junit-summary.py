#!/usr/bin/env python3
"""Write a ctest JUnit XML as a markdown summary, naming every failing test.

    scripts/junit-summary.py <junit.xml> [title]

Goes to $GITHUB_STEP_SUMMARY when set, stdout otherwise. A failure the reader
has to dig a log out for is a failure that gets ignored.
"""
import os
import sys
import xml.etree.ElementTree as ET

path, title = sys.argv[1], (sys.argv[2] if len(sys.argv) > 2 else "Tests")

if not os.path.exists(path):
    out = f"### {title}\n\nNo test results at `{path}` — the run did not get that far.\n"
else:
    cases = ET.parse(path).getroot().iter("testcase")
    failed, skipped, passed, seconds = [], [], 0, 0.0
    for case in cases:
        seconds += float(case.get("time") or 0)
        if case.find("failure") is not None or case.find("error") is not None:
            failed.append(case.get("name"))
        elif case.find("skipped") is not None:
            skipped.append(case.get("name"))
        else:
            passed += 1

    mark = "❌" if failed else "✅"
    out = (
        f"### {mark} {title} — {passed} passed, {len(failed)} failed, "
        f"{len(skipped)} skipped ({seconds:.1f}s)\n"
    )
    if failed:
        out += "\nFailing:\n\n" + "".join(f"- `{name}`\n" for name in failed)

# encoding="utf-8" explicitly: Python on the Windows runners defaults to cp1252
# and the tick in the heading is then an unhandled UnicodeEncodeError, which
# fails a job whose tests all passed.
if "GITHUB_STEP_SUMMARY" in os.environ:
    with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as f:
        f.write(out)
else:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stdout.write(out)
