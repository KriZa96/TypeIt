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
#   version-guard.sh check-schema [base]     a released schema file was not edited
#   version-guard.sh check-config [base]     a config key was not renamed without a migration
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
# A shipped schema file is history: every database in the wild was built by it,
# and editing it changes what those databases were *supposed* to be without
# changing what they are (VERSIONING section 5). A schema change is a new file
# with the next number.
#
# New files are welcome; edits to existing ones are not. Deferred here from
# CI-008, because there was no schema to guard until TI-057.
check_schema() {
    local base=${1:-}
    local schema_dir=libs/infra/schema

    # Deliberately no "does the directory exist" shortcut: deleting the whole
    # schema directory would then disable the guard that exists to notice
    # exactly that. An empty diff passes on its own.
    if [[ -z $base ]] || ! git rev-parse --verify --quiet "$base" >/dev/null; then
        echo "no usable base commit; skipping the schema guard"
        return 0
    fi

    local changed
    changed=$(git diff --name-only --diff-filter=MD "$base"...HEAD -- "$schema_dir" || true)
    [[ -z $changed ]] || die "$(printf 'a released schema file was modified or deleted: %s\n' "$changed")A schema change is a new file with the next number, plus a migration test from the previous one (VERSIONING section 5)."

    local added
    added=$(git diff --name-only --diff-filter=A "$base"...HEAD -- "$schema_dir" || true)
    if [[ -n $added ]]; then
        echo "new schema files: $added"
        # A new schema file has to be numbered above every released one, or the
        # migrator will skip it on databases that are already past it.
        local highest_released highest_added
        highest_released=$(git ls-tree -r --name-only "$base" -- "$schema_dir" | sed -n 's|.*/\([0-9]\{1,\}\)_.*|\1|p' | sort -n | tail -1)
        highest_added=$(printf '%s\n' "$added" | sed -n 's|.*/\([0-9]\{1,\}\)_.*|\1|p' | sort -n | tail -1)
        if [[ -n $highest_released && -n $highest_added ]] && ((10#$highest_added <= 10#$highest_released)); then
            die "new schema file $highest_added is not above the released $highest_released"
        fi
    fi
}

# A renamed configuration key is somebody's setting quietly reverting to its
# default. VERSIONING section 5 says a rename bumps `config_version` and ships a
# migration; this makes that a rule rather than a habit.
#
# The keys are read from the writer — TomlConfigStore::render is the one place
# that spells every key — so a key that disappears from the written template has
# either been renamed or removed, and both need the same treatment. Deferred
# here from CI-008, which had no config to guard.
check_config() {
    local base=${1:-}
    local writer=libs/infra/src/config/TomlConfigStore.cpp
    local migration=libs/infra/src/config/ConfigMigration.cpp

    [[ -f $writer ]] || { echo "no config writer yet"; return 0; }

    if [[ -z $base ]] || ! git rev-parse --verify --quiet "$base" >/dev/null; then
        echo "no usable base commit; skipping the config guard"
        return 0
    fi

    # `out << "some_key       = "` in the rendered template, one per setting.
    local before after gone
    before=$(git show "$base:$writer" 2>/dev/null | sed -n 's/.*out << "\([a-z_]\{1,\}\)[ ]*=.*/\1/p' | sort -u || true)
    after=$(sed -n 's/.*out << "\([a-z_]\{1,\}\)[ ]*=.*/\1/p' "$writer" | sort -u)
    gone=$(comm -23 <(echo "$before") <(echo "$after") || true)

    [[ -n $gone ]] || return 0

    echo "config keys removed or renamed: $(echo "$gone" | tr '\n' ' ')"

    # A rename is fine when the version went up and a migration came with it.
    local version_before version_after
    version_before=$(git show "$base:$migration" 2>/dev/null | grep -c 'ConfigRename{' || true)
    version_after=$(grep -c 'ConfigRename{' "$migration" 2>/dev/null || true)
    if ((version_after <= version_before)); then
        die "a configuration key was renamed or removed without a migration. VERSIONING section 5: bump config_version and add a ConfigRename, so the value the user set is not silently lost."
    fi
    echo "and a migration came with them"
}

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
    check-schema) check_schema "${2:-}" ;;
    check-config) check_config "${2:-}" ;;
    *)
        sed -n '2,12p' "$0"
        exit 2
        ;;
esac
