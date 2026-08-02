#!/usr/bin/env bash
# Report how far behind the pinned dependencies are. Nothing here bumps
# anything: C++ pins are bumped by hand with a build to prove it (docs/CI_CD.md
# section 8). This just makes "we are two years behind" impossible to not know.
#
#   scripts/dependency-freshness.sh          markdown table on stdout
#
# Set GH_TOKEN to avoid the unauthenticated rate limit.
set -euo pipefail

cd "$(dirname "$0")/.."

api() {
    local url=$1
    if [[ -n ${GH_TOKEN:-} ]]; then
        curl -sSf -H "Authorization: Bearer $GH_TOKEN" "$url"
    else
        curl -sSf "$url"
    fi
}

latest_tag() {
    local repo=$1 tag
    tag=$(api "https://api.github.com/repos/${repo}/releases/latest" | sed -n 's/.*"tag_name": *"\([^"]*\)".*/\1/p' | head -1)
    [[ -n $tag ]] || tag=$(api "https://api.github.com/repos/${repo}/tags?per_page=1" | sed -n 's/.*"name": *"\([^"]*\)".*/\1/p' | head -1)
    echo "${tag:-unknown}"
}

echo "| Dependency | Pinned | Latest upstream | |"
echo "|---|---|---|---|"

# Each FetchContent_Declare block gives a repository and the cache variable
# holding its ref; the variable's value is set at the top of the file.
while read -r repo var; do
    pinned=$(sed -n "s/^set(${var} \([^ ]*\) CACHE.*/\1/p" cmake/Dependencies.cmake | head -1)
    latest=$(latest_tag "$repo")
    if [[ $pinned == "$latest" ]]; then mark="current"; else mark="**behind**"; fi
    echo "| ${repo} | ${pinned:-?} | ${latest} | ${mark} |"
done < <(paste -d' ' \
    <(grep -oE 'github\.com/[^ ]+\.git' cmake/Dependencies.cmake | sed 's|.*github\.com/||;s|\.git$||') \
    <(grep -oE 'GIT_TAG \$\{[A-Za-z_]+\}' cmake/Dependencies.cmake | sed 's/.*{//;s/}//'))
