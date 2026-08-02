#!/usr/bin/env bash
# Negative tests for the version guards and the bump arithmetic. A guard nobody
# has watched fail is decoration, so every guard here is given something that
# must make it fail, and the pass cases sit next to them.
#
# Each case runs against a throwaway fixture repository, never against this one.
#
#   scripts/version-tools-test.sh
set -uo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
guard="$root/scripts/version-guard.sh"
bump="$root/scripts/version-bump.sh"
passed=0
failed=0

report() {
    if [[ $1 == pass ]]; then
        passed=$((passed + 1))
        echo "  ok    $2"
    else
        failed=$((failed + 1))
        echo "  FAIL  $2"
        [[ -n ${3:-} ]] && echo "          $3"
    fi
}

assert_ok() {
    local what=$1 out
    shift
    if out=$("$@" 2>&1); then report pass "$what"; else report fail "$what" "$out"; fi
}

assert_fails() {
    local what=$1 out
    shift
    if out=$("$@" 2>&1); then report fail "$what" "succeeded, but should have been blocked"; else report pass "$what"; fi
}

assert_eq() {
    local what=$1 want=$2 got=$3
    if [[ $want == "$got" ]]; then report pass "$what"; else report fail "$what" "want '$want', got '$got'"; fi
}

# A minimal repository shaped like this one: fixture <core> <prerelease>
fixture() {
    local dir core=$1 pre=${2:-}
    dir=$(mktemp -d)
    cat >"$dir/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.24)

project(
    TypeIt
    VERSION ${core}
    LANGUAGES CXX)

set(TYPEIT_VERSION_PRERELEASE "${pre}" CACHE STRING "SemVer pre-release identifier, or empty")
EOF
    cat >"$dir/CHANGELOG.md" <<'EOF'
# Changelog

## [Unreleased]

### Added

- Something worth releasing.

## [1.0.0] - 2025-05-17

### Added

- The first one.

[Unreleased]: https://example.invalid/compare/v1.0.0...HEAD
[1.0.0]: https://example.invalid/releases/tag/v1.0.0
EOF
    mkdir -p "$dir/src"
    echo 'int main() { return 0; }' >"$dir/src/main.cpp"
    git -C "$dir" init -q
    git -C "$dir" add -A
    echo "$dir"
}

in_fixture() {
    local dir=$1
    shift
    (cd "$dir" && "$@")
}

echo "version-guard: tag guards"
d=$(fixture 2.0.0 alpha.1)
assert_fails "a tag ahead of the source is blocked" in_fixture "$d" "$guard" check-tag v9.9.9
assert_fails "a release tag with a pre-release still set is blocked" in_fixture "$d" "$guard" check-tag v2.0.0
assert_fails "a tag with the wrong pre-release is blocked" in_fixture "$d" "$guard" check-tag v2.0.0-beta.1
assert_fails "a tag with no CHANGELOG heading is blocked" in_fixture "$d" "$guard" check-tag v2.0.0-alpha.1
assert_fails "a malformed tag is blocked" in_fixture "$d" "$guard" check-tag v2.0
sed -i 's/^## \[Unreleased\]$/## [Unreleased]\n\n## [2.0.0-alpha.1] - 2026-01-01/' "$d/CHANGELOG.md"
assert_ok "the matching tag with notes passes" in_fixture "$d" "$guard" check-tag v2.0.0-alpha.1
assert_ok "refs\/tags\/ prefixed form passes" in_fixture "$d" "$guard" check-tag refs/tags/v2.0.0-alpha.1
rm -rf "$d"

d=$(fixture 2.0.0 "")
assert_ok "a release tag against a cleared pre-release needs only its notes" \
    bash -c "cd '$d' && sed -i 's/^## \\[Unreleased\\]$/## [Unreleased]\\n\\n## [2.0.0] - 2026-01-01/' CHANGELOG.md && '$guard' check-tag v2.0.0"
rm -rf "$d"

echo "version-guard: version literals"
d=$(fixture 2.0.0 alpha.1)
assert_ok "a tree with no stray literal passes" in_fixture "$d" "$guard" check-literals
echo 'constexpr auto kVersion = "2.0.0";' >"$d/src/version.cpp"
git -C "$d" add -A
assert_fails "a version literal in a source file is blocked" in_fixture "$d" "$guard" check-literals
rm -rf "$d"

echo "version-guard: derivation"
d=$(fixture 2.0.0 alpha.1)
assert_eq "a tag build takes the tag" "2.0.0-alpha.1" \
    "$(in_fixture "$d" env GITHUB_REF=refs/tags/v2.0.0-alpha.1 GITHUB_SHA=abcdef1234 "$guard" derive)"
assert_eq "a main build carries the sha as build metadata" "2.0.0-alpha.1+abcdef1" \
    "$(in_fixture "$d" env GITHUB_REF=refs/heads/main GITHUB_SHA=abcdef1234 "$guard" derive)"
assert_eq "a branch build is a dev version" "2.0.0-alpha.1.dev.7+abcdef1" \
    "$(in_fixture "$d" env GITHUB_REF=refs/heads/feat/x GITHUB_RUN_NUMBER=7 GITHUB_SHA=abcdef1234 "$guard" derive)"
rm -rf "$d"

d=$(fixture 2.0.0 "")
assert_eq "a branch build with no pre-release opens one" "2.0.0-dev.7+abcdef1" \
    "$(in_fixture "$d" env GITHUB_REF=refs/heads/feat/x GITHUB_RUN_NUMBER=7 GITHUB_SHA=abcdef1234 "$guard" derive)"
rm -rf "$d"

echo "version-guard: notices never fail"
d=$(fixture 2.0.0 alpha.1)
git -C "$d" -c user.email=t@t -c user.name=t commit -qm init
base=$(git -C "$d" rev-parse HEAD)
echo 'int extra() { return 1; }' >>"$d/src/main.cpp"
git -C "$d" add -A
git -C "$d" -c user.email=t@t -c user.name=t commit -qm change
assert_ok "a code change with no changelog entry warns rather than fails" in_fixture "$d" "$guard" notices "$base"
assert_eq "and says so" "1" \
    "$(in_fixture "$d" "$guard" notices "$base" | grep -c 'CHANGELOG.md')"
rm -rf "$d"

echo "version-bump: arithmetic"
d=$(fixture 2.0.0 alpha.1)
assert_eq "major clears the pre-release" "3.0.0" "$(in_fixture "$d" "$bump" next major)"
assert_eq "minor clears the pre-release" "2.1.0" "$(in_fixture "$d" "$bump" next minor)"
assert_eq "patch on a pre-release cuts that release" "2.0.0" "$(in_fixture "$d" "$bump" next patch)"
assert_eq "prerelease increments the trailing number" "2.0.0-alpha.2" "$(in_fixture "$d" "$bump" next prerelease)"
rm -rf "$d"

d=$(fixture 2.0.0 beta.1)
assert_eq "beta.1 to beta.2" "2.0.0-beta.2" "$(in_fixture "$d" "$bump" next prerelease)"
rm -rf "$d"
d=$(fixture 2.0.0 rc.1)
assert_eq "rc.1 to the release" "2.0.0" "$(in_fixture "$d" "$bump" next patch)"
rm -rf "$d"
d=$(fixture 1.4.2 "")
assert_eq "patch with no pre-release increments patch" "1.4.3" "$(in_fixture "$d" "$bump" next patch)"
assert_fails "bumping a pre-release that does not exist is refused" in_fixture "$d" "$bump" next prerelease
rm -rf "$d"
d=$(fixture 2.0.0 nightly)
assert_fails "an unnumbered pre-release is refused rather than guessed" in_fixture "$d" "$bump" next prerelease
rm -rf "$d"

echo "version-bump: apply"
d=$(fixture 2.0.0 alpha.1)
git -C "$d" -c user.email=t@t -c user.name=t commit -qm init
before=$(git -C "$d" rev-list --count HEAD)
assert_ok "apply rewrites the tree" in_fixture "$d" "$bump" apply prerelease
assert_eq "the source now declares the new version" "2.0.0-alpha.2" "$(in_fixture "$d" "$guard" version)"
assert_eq "the changelog gained a dated heading" "1" \
    "$(grep -c '^## \[2.0.0-alpha.2\] - [0-9-]\{10\}$' "$d/CHANGELOG.md")"
assert_eq "and an empty Unreleased block above it" "0" \
    "$(awk '/^## \[Unreleased\]/{f=1;next} /^## \[/{f=0} f' "$d/CHANGELOG.md" | grep -c '[^[:space:]]')"
assert_eq "the Unreleased link compares from the new tag" "1" \
    "$(grep -c '^\[Unreleased\]: .*/compare/v2.0.0-alpha.2\.\.\.HEAD$' "$d/CHANGELOG.md")"
assert_eq "and the new tag compares from the previous one" "1" \
    "$(grep -c '^\[2.0.0-alpha.2\]: .*/compare/v1.0.0\.\.\.v2.0.0-alpha.2$' "$d/CHANGELOG.md")"
assert_eq "it commits nothing and pushes nothing" "$before" "$(git -C "$d" rev-list --count HEAD)"
assert_ok "the result passes its own tag guard" in_fixture "$d" "$guard" check-tag v2.0.0-alpha.2
rm -rf "$d"

d=$(fixture 2.0.0 alpha.1)
sed -i '/^- Something worth releasing\.$/d;/^### Added$/d' "$d/CHANGELOG.md"
assert_fails "releasing an empty Unreleased block is refused" in_fixture "$d" "$bump" apply prerelease
rm -rf "$d"

echo
echo "$passed passed, $failed failed"
[[ $failed -eq 0 ]]
