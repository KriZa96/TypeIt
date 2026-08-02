#!/usr/bin/env bash
# Compute and apply the next version. Driven by .github/workflows/version-bump.yml,
# which opens a pull request with the result — it never pushes to a branch that
# ships (docs/VERSIONING.md section 12.3: the pipeline suggests, a human confirms).
#
#   version-bump.sh next  <major|minor|patch|prerelease>   print the next version
#   version-bump.sh apply <major|minor|patch|prerelease>   edit CMakeLists.txt and CHANGELOG.md
#
# `patch` on a pre-release cuts the release it was leading up to, so the
# 2.0.0-beta.1 -> 2.0.0-beta.2 -> 2.0.0-rc.1 -> 2.0.0 chain needs no fourth verb.
# Moving from beta to rc is a judgement call and stays manual.
set -euo pipefail

die() {
    echo "::error::$*" >&2
    exit 1
}

here=$(cd "$(dirname "$0")" && pwd)
core=$("$here/version-guard.sh" version)
pre=${core#*-}
[[ $core == *-* ]] || pre=""
core=${core%%-*}
IFS=. read -r major minor patch <<<"$core"

next_version() {
    case ${1:-} in
        major) echo "$((major + 1)).0.0" ;;
        minor) echo "${major}.$((minor + 1)).0" ;;
        patch)
            # A pre-release is already numbered for the release it precedes:
            # 2.0.0-rc.1 becomes 2.0.0, not 2.0.1.
            [[ -n $pre ]] && echo "$core" || echo "${major}.${minor}.$((patch + 1))"
            ;;
        prerelease)
            [[ -n $pre ]] || die "no pre-release to bump; set TYPEIT_VERSION_PRERELEASE first"
            [[ $pre =~ ^(.*[.])?([0-9]+)$ ]] ||
                die "pre-release '$pre' does not end in a number; choose the next identifier by hand"
            echo "${core}-${BASH_REMATCH[1]}$((BASH_REMATCH[2] + 1))"
            ;;
        *) die "usage: version-bump.sh next|apply <major|minor|patch|prerelease>" ;;
    esac
}

apply() {
    local next new_core new_pre today previous
    next=$(next_version "$1")
    new_core=${next%%-*}
    new_pre=${next#*-}
    [[ $next == *-* ]] || new_pre=""
    today=$(date -u +%Y-%m-%d)

    # An empty Unreleased block means nobody wrote down what is being released.
    awk '/^## \[Unreleased\]/{f=1;next} /^## \[/{f=0} f' CHANGELOG.md | grep -q '[^[:space:]]' ||
        die "the '## [Unreleased]' block is empty; there is nothing to release"

    sed -i "s/^\([[:space:]]*\)VERSION ${core}$/\1VERSION ${new_core}/" CMakeLists.txt
    sed -i "s/^set(TYPEIT_VERSION_PRERELEASE \"${pre}\"/set(TYPEIT_VERSION_PRERELEASE \"${new_pre}\"/" CMakeLists.txt
    [[ $("$here/version-guard.sh" version) == "$next" ]] || die "CMakeLists.txt did not take the edit"

    # The Unreleased block becomes the release, and a fresh empty one takes its
    # place above.
    sed -i "s|^## \[Unreleased\]$|## [Unreleased]\n\n## [${next}] - ${today}|" CHANGELOG.md

    # Link references: Unreleased now compares from the new tag, and the new tag
    # compares from whatever was previously on top.
    previous=$(sed -n 's|^\[\([^]]*\)\]: .*/compare/v\([^.]*\.[^.]*\..*\)\.\.\.HEAD$|\2|p;s|^\[Unreleased\]: .*/compare/v\(.*\)\.\.\.HEAD$|\1|p' CHANGELOG.md | head -1)
    [[ -n $previous ]] || die "cannot find the [Unreleased] link reference in CHANGELOG.md"
    sed -i \
        -e "s|^\[Unreleased\]: \(.*\)/compare/v${previous}\.\.\.HEAD$|[Unreleased]: \1/compare/v${next}...HEAD\n[${next}]: \1/compare/v${previous}...v${next}|" \
        CHANGELOG.md

    echo "$next"
}

case ${1:-} in
    next) next_version "${2:-}" ;;
    apply) apply "${2:-}" ;;
    *) die "usage: version-bump.sh next|apply <major|minor|patch|prerelease>" ;;
esac
