#!/usr/bin/env bash
#
# Line coverage for the rebuilt libraries, with the gates from TESTING section 9.
#
#   coverage.sh report [build-dir]   configure, build, run, and write the reports
#   coverage.sh gate   [build-dir]   check the thresholds against an existing report
#
# The two are separate so CI can publish a report even when the gate fails: a
# number nobody can see is not a diagnostic.
#
# Only the rebuilt tree is measured. src/ is the 1.0 code being replaced whole
# at TI-097, and holding it to a coverage bar it was never written for would
# mean either a meaningless number or a suppression nobody reads.
set -euo pipefail

BUILD_DIR="${2:-build/coverage}"
REPORT_DIR="${BUILD_DIR}/coverage"

# TESTING section 9. `core` overall, plus the three areas where being wrong is
# silent and there is nothing to blame.
CORE_MINIMUM=90
AREA_MINIMUM=95
AREAS=(libs/core/src/metrics libs/core/src/text libs/core/src/modes)

# TESTING section 9's figure for the use-case layer. It is high because a
# service is orchestration over fakes: there is no I/O to stand up, so an
# uncovered line is a path nobody thought about.
APP_MINIMUM=85

# `infra` is lower on purpose (TESTING section 9): some of its error paths need
# a full disk, a permission failure or a corrupt database to reach, and a test
# that arranges those is a test that only passes on the machine it was written
# on. The number is a floor, not a target — it measures 84% today.
INFRA_MINIMUM=75

require() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "coverage.sh needs $1" >&2
        exit 127
    fi
}

report() {
    require gcovr
    require cmake
    require ctest

    cmake --preset linux-coverage -B "${BUILD_DIR}" >/dev/null
    cmake --build "${BUILD_DIR}" --parallel
    # A failing suite still produces a report; the tests' own job is to fail the
    # build elsewhere, and a coverage run that stops at the first failure tells
    # you nothing about the rest of the tree.
    ctest --test-dir "${BUILD_DIR}" --output-on-failure || true

    mkdir -p "${REPORT_DIR}"
    # --merge-mode-functions: gcc 16 reports some inlined member functions at
    # two different lines and gcovr refuses to merge them, which stops the whole
    # report. CI's gcc 14 does not, so without this the script works in CI and
    # fails on a current toolchain — the worse of the two places to be broken.
    gcovr \
        --root . \
        --filter 'libs/' \
        --exclude '.*_deps.*' \
        --gcov-ignore-parse-errors \
        --merge-mode-functions=separate \
        --json "${REPORT_DIR}/coverage.json" \
        --xml "${REPORT_DIR}/coverage.xml" \
        --html-details "${REPORT_DIR}/index.html" \
        --txt "${REPORT_DIR}/coverage.txt" \
        --print-summary
}

# Percentage of covered lines under a path prefix, from the gcovr JSON.
percent_for() {
    python3 - "$1" "${REPORT_DIR}/coverage.json" <<'PY'
import json
import sys

prefix, report = sys.argv[1], sys.argv[2]
with open(report, encoding="utf-8") as handle:
    data = json.load(handle)

total = 0
covered = 0
for entry in data.get("files", []):
    if not entry["file"].startswith(prefix):
        continue
    for line in entry.get("lines", []):
        total += 1
        covered += 1 if line["count"] > 0 else 0

print(f"{(100.0 * covered / total) if total else 0.0:.2f} {covered} {total}")
PY
}

gate() {
    if [ ! -f "${REPORT_DIR}/coverage.json" ]; then
        echo "no report at ${REPORT_DIR}/coverage.json — run 'coverage.sh report' first" >&2
        exit 1
    fi

    local failed=0
    local percent covered total

    read -r percent covered total < <(percent_for libs/core/src)
    printf '%-28s %6s%%  (%s/%s lines, minimum %s%%)\n' "core" "${percent}" "${covered}" "${total}" "${CORE_MINIMUM}"
    if awk -v value="${percent}" -v minimum="${CORE_MINIMUM}" 'BEGIN { exit !(value < minimum) }'; then
        echo "  below the gate" >&2
        failed=1
    fi

    read -r percent covered total < <(percent_for libs/app/src)
    printf '%-28s %6s%%  (%s/%s lines, minimum %s%%)\n' "app" "${percent}" "${covered}" "${total}" "${APP_MINIMUM}"
    if awk -v value="${percent}" -v minimum="${APP_MINIMUM}" 'BEGIN { exit !(value < minimum) }'; then
        echo "  below the gate" >&2
        failed=1
    fi

    read -r percent covered total < <(percent_for libs/infra/src)
    printf '%-28s %6s%%  (%s/%s lines, minimum %s%%)\n' "infra" "${percent}" "${covered}" "${total}" \
        "${INFRA_MINIMUM}"
    if awk -v value="${percent}" -v minimum="${INFRA_MINIMUM}" 'BEGIN { exit !(value < minimum) }'; then
        echo "  below the gate" >&2
        failed=1
    fi

    for area in "${AREAS[@]}"; do
        read -r percent covered total < <(percent_for "${area}")
        printf '%-28s %6s%%  (%s/%s lines, minimum %s%%)\n' "${area#libs/core/src/}" "${percent}" "${covered}" \
            "${total}" "${AREA_MINIMUM}"
        if awk -v value="${percent}" -v minimum="${AREA_MINIMUM}" 'BEGIN { exit !(value < minimum) }'; then
            echo "  below the gate" >&2
            failed=1
        fi
    done

    if [ "${failed}" -ne 0 ]; then
        echo >&2
        echo "Coverage gates not met. Uncovered code in core has no excuse: there is nothing" >&2
        echo "to stand up, no I/O to fake and no dependency to blame (TESTING section 9)." >&2
        exit 1
    fi
}

case "${1:-}" in
    report) report ;;
    gate) gate ;;
    *)
        echo "usage: coverage.sh {report|gate} [build-dir]" >&2
        exit 2
        ;;
esac
