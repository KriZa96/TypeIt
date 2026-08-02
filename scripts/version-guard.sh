#!/usr/bin/env bash
# The versioning policy as code: docs/VERSIONING.md section 12, docs/CI_CD.md
# section 6. Every guard here is exercised by scripts/version-tools-test.sh,
# which watches each one fail on a deliberate violation.
#
#   version-guard.sh version                 the full version the source declares
#   version-guard.sh check-tag vX.Y.Z        tag <-> source, pre-release, CHANGELOG
#   version-guard.sh check-literals          no version literal outside CMakeLists.txt
#   version-guard.sh derive                  the version this build should report
#   version-guard.sh notices [base]          non-blocking reminders, never fails
#
# Runs against the current working directory, so the tests can point it at a
# fixture instead of the repository it lives in.
set -euo pipefail

die() {
    echo "::error::$*"
    exit 1
}

# The core MAJOR.MINOR.PATCH from the project() call. The `VERSION x.y.z` line
# stands alone inside project(); cmake_minimum_required puts its VERSION on the
# same line as the command, so it cannot match.
core_version() {
    local version
    version=$(sed -n 's/^[[:space:]]*VERSION[[:space:]]\{1,\}\([0-9]\{1,\}\(\.[0-9]\{1,\}\)\{2\}\)[[:space:]]*$/\1/p' CMakeLists.txt | head -1)
    [[ -n $version ]] || die "no project(VERSION x.y.z) found in CMakeLists.txt"
    echo "$version"
}

prerelease() {
    sed -n 's/^set(TYPEIT_VERSION_PRERELEASE[[:space:]]\{1,\}"\([^"]*\)".*/\1/p' CMakeLists.txt | head -1
}

full_version() {
    local core pre
    core=$(core_version)
    pre=$(prerelease)
    [[ -z $pre ]] && echo "$core" || echo "${core}-${pre}"
}

# Tag <-> source, pre-release consistency, and the CHANGELOG entry. Three
# guards rather than one comparison, because a single mismatch message does not
# tell you which mistake you made.
check_tag() {
    local tag=${1:?usage: check-tag <tag>}
    local want=${tag#refs/tags/}
    want=${want#v}

    [[ $want =~ ^[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]+)?$ ]] ||
        die "tag '$tag' is not vMAJOR.MINOR.PATCH[-prerelease]"

    if [[ ${want%%-*} != "$(core_version)" ]]; then
        die "tag says ${want%%-*} but CMakeLists.txt says $(core_version). Refusing to release."
    fi

    if [[ $want != "$(full_version)" ]]; then
        local pre=${want#*-}
        [[ $want == *-* ]] || pre="(none)"
        die "tag pre-release is $pre but TYPEIT_VERSION_PRERELEASE is '$(prerelease)'"
    fi

    grep -q "^## \[${want}\]" CHANGELOG.md ||
        die "CHANGELOG.md has no '## [${want}]' heading. A release with no notes is not a release."
}

# The version this project declares must appear in exactly one place. Prose is
# allowed to name it; anything the build reads is not, or two versions end up
# inside one binary (VERSIONING section 6).
check_literals() {
    local version hits
    version=$(core_version)
    # The guard's own fixtures assert on literal versions, so that one file
    # is exempt. Nothing else is: prose belongs in docs/ or the CHANGELOG.
    hits=$(git grep -n --fixed-strings -- "$version" -- \
        ':!CMakeLists.txt' ':!CHANGELOG.md' ':!docs/' ':!scripts/version-tools-test.sh' || true)
    [[ -z $hits ]] || {
        echo "$hits"
        die "version literal '$version' outside CMakeLists.txt. Read it from typeit/core/Version.h."
    }
}

# tag  -> the tag. main -> version+sha. anything else -> version-dev.run+sha.
# Build metadata after '+' is ignored by SemVer precedence, so a development
# build can never outrank a release.
derive() {
    local ref=${GITHUB_REF:-} sha=${GITHUB_SHA:-} run=${GITHUB_RUN_NUMBER:-0} full
    full=$(full_version)
    case $ref in
        refs/tags/v*)
            echo "${ref#refs/tags/v}"
            ;;
        refs/heads/main)
            echo "${full}+${sha:0:7}"
            ;;
        *)
            # A pre-release suffix already opened the hyphen, so dev extends it
            # with a dot rather than a second hyphen: x.y.z-alpha.1.dev.7+abc.
            [[ $full == *-* ]] && echo "${full}.dev.${run}+${sha:0:7}" || echo "${full}-dev.${run}+${sha:0:7}"
            ;;
    esac
}

# Reminders, not gates. They are annotations rather than pull request comments
# on purpose: a fork's token cannot post a comment, and a guard that fails on
# fork pull requests teaches people to ignore it.
notices() {
    local base=${1:-} changed
    if [[ -z $base ]] || ! git cat-file -e "${base}^{commit}" 2>/dev/null; then
        echo "no usable base commit; skipping notices"
        return 0
    fi
    changed=$(git diff --name-only "$base" HEAD)

    if grep -qE '^(libs|src|include)/' <<<"$changed" && ! grep -qx 'CHANGELOG.md' <<<"$changed"; then
        echo "::warning::This changes code but not CHANGELOG.md. If behaviour changed, add an entry under '## [Unreleased]'."
    fi

    if grep -q '^libs/core/metrics/' <<<"$changed"; then
        echo "::warning::Metric definitions changed. Redefining a published metric is MAJOR within a release line (VERSIONING section 11.1)."
    fi
}

case ${1:-} in
    version) full_version ;;
    check-tag) check_tag "${2:-}" ;;
    check-literals) check_literals ;;
    derive) derive ;;
    notices) notices "${2:-}" ;;
    *)
        sed -n '2,12p' "$0"
        exit 2
        ;;
esac
