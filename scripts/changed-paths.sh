#!/usr/bin/env bash
# Decide whether a change can possibly affect the build.
#
# Writes `code=true|false` to $GITHUB_OUTPUT (stdout when run locally). Set
# BASE to the commit to compare against; an unknown or missing base builds,
# because guessing wrong in that direction only costs runner minutes.
set -euo pipefail

base=${BASE:-}
out=${GITHUB_OUTPUT:-/dev/stdout}

if [[ -z $base ]] || ! git cat-file -e "${base}^{commit}" 2>/dev/null; then
    echo "code=true" >>"$out"
    exit 0
fi

# Anything not documentation is code enough: build files, sources, workflows.
if git diff --name-only "$base" HEAD | grep -qvE '^(docs/|CHANGELOG\.md$|LICENSE$|[^/]*\.md$)'; then
    echo "code=true" >>"$out"
else
    echo "code=false" >>"$out"
fi
