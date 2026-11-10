#!/bin/sh
# Repository hygiene gate (VERIF-07): refuses credential shapes, email addresses
# outside the noreply and example sets, and absolute home-directory paths.
#
#   hygiene.sh --tree          tracked plus untracked-not-ignored files (run in a git work tree)
#   hygiene.sh --dir <path>    every file under <path>, no git (used by the controls)
#
# Exit 0 when clean, 1 on findings, 2 on a usage error.
#
# Findings print `hygiene: <code>: <path>:<line>`. The matched text is never
# printed, so a real or planted secret is not copied into logs.
#
# Reason codes: credential, email, home-path, usage.
set -eu
LC_ALL=C
export LC_ALL

# POSIX ERE sets. Written so that none of them match their own source line
# (escaped dots, bracket classes), so this script is scanned like every other
# file with no exemption.
GBL_HY_HOME='(/Users/|/home/)[A-Za-z0-9._-]+|[A-Za-z]:\\Users\\[A-Za-z0-9._-]+|-Users-[A-Za-z0-9._]+-'
GBL_HY_HOME_OK='(/Users/|/home/)runner'
GBL_HY_EMAIL='[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)*\.[A-Za-z][A-Za-z]+'
GBL_HY_EMAIL_OK='@users\.noreply\.github\.com|noreply@github\.com|noreply@anthropic\.com|@example\.(com|org|net)|git@github\.com'
GBL_HY_CRED='AKIA[0-9A-Z]{16}|ASIA[0-9A-Z]{16}|gh[pousr]_[A-Za-z0-9]{36,}|github_pat_[A-Za-z0-9_]{22,}|xox[baprs]-[A-Za-z0-9-]{10,}|AIza[0-9A-Za-z_-]{35}|sk-ant-[A-Za-z0-9_-]{20,}|-----BEGIN [A-Z ]*PRIVATE KEY-----'
GBL_HY_CRED_ASSIGN='(api[_-]?key|secret|token|passw(or)?d)[[:space:]]*[:=][[:space:]]*["'"'"']?[A-Za-z0-9/+_.-]{16,}'
export GBL_HY_HOME GBL_HY_HOME_OK GBL_HY_EMAIL GBL_HY_EMAIL_OK GBL_HY_CRED GBL_HY_CRED_ASSIGN

usage() {
  echo 'hygiene: usage: hygiene.sh --tree | --dir <path>' >&2
  exit 2
}

# Per-file worker, run by `sh -c` over NUL-delimited file names (xargs -0), so
# a name with a newline or a leading dash is still scanned. A pattern hit is
# reduced to its line number; the matched text goes no further than the filter.
# The allowances (runner home, noreply and example addresses) apply to the whole
# matched token, not to the line, so one allowed address cannot hide another.
# No single quote may appear in this text: it sits inside a single-quoted string.
GBL_HY_WORKER='
for f; do
  [ -f "$f" ] || continue
  grep -Iq "" -- "$f" 2>/dev/null || continue
  grep -n -o -E -e "$GBL_HY_HOME" -- "$f" |
    grep -v -E "^[0-9]+:($GBL_HY_HOME_OK)\$" | cut -d: -f1 | sort -un |
    while read -r n; do printf "hygiene: home-path: %s:%s\n" "$f" "$n"; done
  grep -n -o -E -e "$GBL_HY_EMAIL" -- "$f" |
    grep -v -E "^[0-9]+:.*($GBL_HY_EMAIL_OK)\$" | cut -d: -f1 | sort -un |
    while read -r n; do printf "hygiene: email: %s:%s\n" "$f" "$n"; done
  { grep -n -E -e "$GBL_HY_CRED" -- "$f"; grep -n -E -e "$GBL_HY_CRED_ASSIGN" -- "$f"; } |
    cut -d: -f1 | sort -un |
    while read -r n; do printf "hygiene: credential: %s:%s\n" "$f" "$n"; done
done
exit 0
'

[ "$#" -ge 1 ] || usage
mode=$1
case "$mode" in
  --tree) [ "$#" -eq 1 ] || usage ;;
  --dir) [ "$#" -eq 2 ] || usage ;;
  *) usage ;;
esac

tmp=$(mktemp -d "${TMPDIR:-/tmp}/gbl-hygiene.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

case "$mode" in
  --tree)
    root=$(git rev-parse --show-toplevel 2>/dev/null) || {
      echo 'hygiene: not-a-git-repository: --tree needs a git work tree' >&2
      exit 2
    }
    cd "$root"
    git ls-files -z --cached --others --exclude-standard > "$tmp/list"
    ;;
  --dir)
    dir=$2
    [ -d "$dir" ] || { echo "hygiene: usage: not a directory: $dir" >&2; exit 2; }
    find "$dir" -type f -print0 > "$tmp/list"
    ;;
esac

count=$(tr -cd '\0' < "$tmp/list" | wc -c | tr -d ' ')

xargs -0 sh -c "$GBL_HY_WORKER" sh < "$tmp/list" > "$tmp/findings"

if [ -s "$tmp/findings" ]; then
  cat "$tmp/findings"
  exit 1
fi
echo "hygiene: ok ($count files)"
