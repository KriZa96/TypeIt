#!/usr/bin/env bash
# Pin every third-party GitHub Action to a full commit SHA.
#
#   scripts/pin-actions.sh            report unpinned `uses:` refs, exit 1 if any
#   scripts/pin-actions.sh --write    resolve tags to SHAs in place (needs an authenticated gh)
#
# A tag is mutable: whoever can move it can run code in a job that holds this
# repository's credentials. See docs/CI_CD.md section 8.
set -euo pipefail

cd "$(dirname "$0")/.."
mode=${1:---check}

rc=0
while IFS= read -r file; do
    while IFS=: read -r line content; do
        [[ $content =~ uses:[[:space:]]*[\'\"]?([^@\'\"[:space:]]+)@([^[:space:]\'\"#]+) ]] || continue
        action=${BASH_REMATCH[1]}
        ref=${BASH_REMATCH[2]}
        case $action in ./* | docker://*) continue ;; esac
        [[ $ref =~ ^[0-9a-f]{40}$ ]] && continue

        if [[ $mode == --write ]]; then
            sha=$(gh api "repos/${action}/commits/${ref}" --jq .sha)
            sed -i "${line}s|${action}@${ref}.*|${action}@${sha} # ${ref}|" "$file"
            echo "$file:$line: $action $ref -> $sha"
        else
            echo "$file:$line: $action@$ref is not a 40-character commit SHA"
            rc=1
        fi
    done < <(grep -n 'uses:' "$file" || true)
done < <(find .github -type f \( -name '*.yml' -o -name '*.yaml' \) | sort)

if [[ $rc -ne 0 ]]; then
    echo "Pin them with: scripts/pin-actions.sh --write"
fi
exit $rc
