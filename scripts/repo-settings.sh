#!/usr/bin/env bash
# The parts of the repository that live in GitHub's settings rather than in the
# tree: labels and branch protection. Kept here so they are reviewable and
# repeatable instead of being clicked once and forgotten.
#
#   scripts/repo-settings.sh labels        create the labels the backlog uses
#   scripts/repo-settings.sh protect       protect main and v2
#   scripts/repo-settings.sh               both
#
# Needs `gh auth login` with admin rights on the repository. Everything here is
# idempotent: run it again after changing it.
set -euo pipefail

repo=${REPO:-KriZa96/TypeIt}

labels() {
    # phase, type, layer, size, priority, plus the four that carry meaning of
    # their own. Colours group them: one hue per family.
    local spec=(
        "phase:0A|ededed" "phase:0|ededed" "phase:1|ededed" "phase:2|ededed" "phase:3|ededed"
        "phase:4|ededed" "phase:5|ededed" "phase:6|ededed" "phase:6A|ededed" "phase:7|ededed"
        "phase:8|ededed" "phase:9|ededed"
        "type:feat|1d76db" "type:fix|d73a4a" "type:refactor|1d76db" "type:test|1d76db"
        "type:docs|0075ca" "type:build|1d76db" "type:ci|1d76db" "type:perf|1d76db"
        "type:chore|1d76db" "type:style|1d76db"
        "layer:core|5319e7" "layer:app|5319e7" "layer:infra|5319e7" "layer:tui|5319e7"
        "layer:cli|5319e7" "layer:build|5319e7"
        "size:xs|c2e0c6" "size:s|c2e0c6" "size:m|c2e0c6" "size:l|c2e0c6"
        "priority:p0|b60205" "priority:p1|d93f0b" "priority:p2|fbca04" "priority:p3|fef2c0"
        "closes-defect|d73a4a" "blocked|000000" "good-first-issue|7057ff" "skip-changelog|ededed"
    )
    for entry in "${spec[@]}"; do
        gh label create "${entry%%|*}" --repo "$repo" --color "${entry##*|}" --force >/dev/null
        echo "label ${entry%%|*}"
    done
}

protect() {
    # Linear history, because the whole backlog assumes fast-forward merges;
    # the aggregator check, because it is the one status that reports on every
    # pull request including documentation-only ones.
    for branch in main v2; do
        gh api --silent -X PUT "repos/${repo}/branches/${branch}/protection" \
            --input - <<'JSON'
{
  "required_status_checks": { "strict": true, "contexts": ["all-checks-passed"] },
  "enforce_admins": false,
  "required_pull_request_reviews": null,
  "restrictions": null,
  "required_linear_history": true,
  "allow_force_pushes": false,
  "allow_deletions": false,
  "required_conversation_resolution": true
}
JSON
        echo "protected $branch"
    done
}

case ${1:-all} in
    labels) labels ;;
    protect) protect ;;
    all)
        labels
        protect
        ;;
    *)
        echo "usage: repo-settings.sh [labels|protect]" >&2
        exit 2
        ;;
esac
