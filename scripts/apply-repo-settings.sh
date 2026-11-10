#!/bin/sh
# Apply or check the `main` branch ruleset (D-02, D-13, orchestrator resolution 2).
#
#   apply-repo-settings.sh --check | --apply | --print-canonical [--repo <owner/name>]
#
# The only repository setting this script ever writes is the ruleset in
# .github/rulesets/main.json, found by name, so a re-run updates it in place.
# Everything else is reported, never applied: squash commit title, Actions
# sha_pinning_required, secret scanning, push protection, and the rules the
# owner did not request (deletion, force-push, linear history, merge-method
# restriction). There is no admin-bypass path.
#
#   --print-canonical  canonical lines of the committed ruleset; offline.
#   --check            compare the committed file with the live ruleset
#                      (projected onto the committed file's keys, so a
#                      server-filled default is not drift) and print advisory
#                      lines for the settings above. Writes nothing.
#   --apply            validate the file, refuse until a `CI required` check
#                      run has succeeded on main, create (POST) or update (PUT)
#                      the ruleset, then run --check.
#   --repo             owner/name; default is read with `gh repo view`.
#
# All JSON is parsed by cmake/RulesetCheck.cmake (string(JSON)); gh is only
# asked to fetch or write, never to filter, so a stub gh on PATH can serve every
# branch offline. `--paginate --slurp` needs gh 2.48 or later. CMAKE overrides
# the cmake program.
#
# Reason lines and exit codes:
#   repo-settings: usage               exit 2 (before any gh call)
#   repo-settings: ruleset ok (id <n>) --check, no drift
#   repo-settings: ruleset-missing     --check, exit 1
#   repo-settings: ruleset-drift       --check, exit 1, followed by a diff
#   repo-settings: no-ci-required-run  --apply refused, exit 1, nothing written
#   repo-settings: advisory: ...       settings reported, not applied
#   repo-settings: gh-failed: <endpoint>  exit 3
set -eu
LC_ALL=C
export LC_ALL

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cmake_bin=${CMAKE:-cmake}
ruleset=.github/rulesets/main.json
unauthorized='not applied: outside the owner'"'"'s ruleset-only authorization'

usage() {
  echo "repo-settings: usage: apply-repo-settings.sh --check | --apply | --print-canonical [--repo <owner/name>]" >&2
  exit 2
}

mode=""
repo=""
while [ "$#" -gt 0 ]; do
  case $1 in
    --check|--apply|--print-canonical)
      [ -z "$mode" ] || usage
      mode=$1
      ;;
    --repo)
      [ "$#" -ge 2 ] || usage
      repo=$2
      shift
      ;;
    *) usage ;;
  esac
  shift
done
[ -n "$mode" ] || usage
if [ -n "$repo" ]; then
  # The value becomes part of an API path.
  case $repo in
    */*/*|*[!A-Za-z0-9._/-]*|/*|*/) usage ;;
    */*) ;;
    *) usage ;;
  esac
fi

cd "$root"

# rs <mode> [-Dvar=value...]: one cmake/RulesetCheck.cmake mode.
rs() {
  rs_mode=$1
  shift
  "$cmake_bin" "-DMODE=$rs_mode" "$@" -P "$root/cmake/RulesetCheck.cmake"
}

if [ "$mode" = "--print-canonical" ]; then
  rs canon "-DFILE=$ruleset" "-DREF=$ruleset"
  exit 0
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

gh_fail() {
  echo "repo-settings: gh-failed: $1" >&2
  sed 's/^/repo-settings: gh: /' "$tmp/err" >&2 || true
  exit 3
}

# gh_get <endpoint> <outfile>
gh_get() {
  gh api "$1" > "$2" 2> "$tmp/err" || gh_fail "$1"
}

if [ -z "$repo" ]; then
  # The only jq use: a default repository name. Skipped whenever --repo is given.
  repo=$(gh repo view --json nameWithOwner --jq .nameWithOwner 2> "$tmp/err") || gh_fail "repo view"
fi

# find_id: prints the id of the ruleset named main, or nothing.
find_id() {
  gh api --paginate --slurp "repos/$repo/rulesets" > "$tmp/list.json" 2> "$tmp/err" \
    || gh_fail "repos/$repo/rulesets"
  rs find-id "-DFILE=$tmp/list.json" > "$tmp/id.txt"
  found_id=$(cat "$tmp/id.txt")
  case $found_id in
    *[!0-9]*) echo "repo-settings: gh-failed: ruleset id is not a number" >&2; exit 3 ;;
  esac
}

check_status=0

do_check() {
  check_status=0
  find_id
  if [ -z "$found_id" ]; then
    echo "repo-settings: ruleset-missing"
    check_status=1
  else
    gh_get "repos/$repo/rulesets/$found_id" "$tmp/remote.json"
    rs canon "-DFILE=$ruleset" "-DREF=$ruleset" > "$tmp/local.canon"
    rs canon "-DFILE=$tmp/remote.json" "-DREF=$ruleset" > "$tmp/remote.canon"
    if diff -u -L committed -L live "$tmp/local.canon" "$tmp/remote.canon" > "$tmp/drift.diff"; then
      echo "repo-settings: ruleset ok (id $found_id)"
    else
      echo "repo-settings: ruleset-drift"
      cat "$tmp/drift.diff"
      check_status=1
    fi
  fi

  # Advisory only: read, report, never change.
  gh_get "repos/$repo" "$tmp/repo.json"
  gh_get "repos/$repo/actions/permissions" "$tmp/perms.json"
  rs advisory "-DREPO_JSON=$tmp/repo.json" "-DPERMS_JSON=$tmp/perms.json" > "$tmp/advisory.txt"
  while IFS= read -r line; do
    echo "repo-settings: advisory: $line ($unauthorized)"
  done < "$tmp/advisory.txt"
  echo "repo-settings: advisory: rules not requested: deletion, non_fast_forward, required_linear_history, merge-method restriction ($unauthorized)"
}

if [ "$mode" = "--check" ]; then
  do_check
  exit "$check_status"
fi

# --apply
rs validate "-DFILE=$ruleset" > "$tmp/validate.txt" 2>&1 || {
  cat "$tmp/validate.txt" >&2
  echo "repo-settings: invalid-ruleset" >&2
  exit 1
}

# Pitfall 7: a ruleset that requires `CI required` before any run has reported
# it blocks every pull request, including the one that adds the check.
gh_get "repos/$repo/commits/main/check-runs?check_name=CI%20required" "$tmp/checkruns.json"
rs ci-required-ok "-DFILE=$tmp/checkruns.json" > "$tmp/ci_ok.txt"
if [ "$(cat "$tmp/ci_ok.txt")" != "success" ]; then
  echo "repo-settings: no-ci-required-run (no successful 'CI required' check run on main yet; nothing written)" >&2
  exit 1
fi

find_id
if [ -z "$found_id" ]; then
  gh api --method POST "repos/$repo/rulesets" --input "$ruleset" > "$tmp/apply.json" 2> "$tmp/err" \
    || gh_fail "POST repos/$repo/rulesets"
  echo "repo-settings: created ruleset main"
else
  gh api --method PUT "repos/$repo/rulesets/$found_id" --input "$ruleset" > "$tmp/apply.json" 2> "$tmp/err" \
    || gh_fail "PUT repos/$repo/rulesets/$found_id"
  echo "repo-settings: updated ruleset main (id $found_id)"
fi

do_check
exit "$check_status"
