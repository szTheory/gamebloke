#!/bin/sh
# Repository hygiene gate (VERIF-07): refuses credential shapes, email addresses
# outside the noreply and example sets, and absolute home-directory paths.
#
#   hygiene.sh --tree          tracked plus untracked-not-ignored files (run in a git work tree)
#   hygiene.sh --staged        staged blobs (not the working tree) plus the author and committer identity
#   hygiene.sh --dir <path>    every file under <path>, no git (used by the controls)
#
# Exit 0 when clean, 1 on findings, 2 on a usage error.
#
# Findings print `hygiene: <code>: <path>:<line>`. The matched text is never
# printed, so a real or planted secret is not copied into logs.
#
# Reason codes: credential, email, home-path, identity, usage.
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
  echo 'hygiene: usage: hygiene.sh --tree | --staged | --dir <path>' >&2
  exit 2
}

# Per-file worker, run by `sh -c` over NUL-delimited file names (xargs -0), so
# a name with a newline or a leading dash is still scanned. With GBL_HY_STAGED
# set it reads the staged blob into GBL_HY_TMP instead of the working file. A pattern hit is
# reduced to its line number; the matched text goes no further than the filter.
# The allowances (runner home, noreply and example addresses) apply to the whole
# matched token, not to the line, so one allowed address cannot hide another.
# No single quote may appear in this text: it sits inside a single-quoted string.
GBL_HY_WORKER='
for f; do
  if [ -n "${GBL_HY_STAGED:-}" ]; then
    git show ":$f" > "$GBL_HY_TMP" 2>/dev/null || continue
    src=$GBL_HY_TMP
  else
    [ -f "$f" ] || continue
    src=$f
  fi
  grep -Iq "" -- "$src" 2>/dev/null || continue
  grep -n -o -E -e "$GBL_HY_HOME" -- "$src" |
    grep -v -E "^[0-9]+:($GBL_HY_HOME_OK)\$" | cut -d: -f1 | sort -un |
    while read -r n; do printf "hygiene: home-path: %s:%s\n" "$f" "$n"; done
  grep -n -o -E -e "$GBL_HY_EMAIL" -- "$src" |
    grep -v -E "^[0-9]+:.*($GBL_HY_EMAIL_OK)\$" | cut -d: -f1 | sort -un |
    while read -r n; do printf "hygiene: email: %s:%s\n" "$f" "$n"; done
  { grep -n -E -e "$GBL_HY_CRED" -- "$src"; grep -n -E -e "$GBL_HY_CRED_ASSIGN" -- "$src"; } |
    cut -d: -f1 | sort -un |
    while read -r n; do printf "hygiene: credential: %s:%s\n" "$f" "$n"; done
done
exit 0
'

[ "$#" -ge 1 ] || usage
mode=$1
case "$mode" in
  --tree | --staged) [ "$#" -eq 1 ] || usage ;;
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
  --staged)
    root=$(git rev-parse --show-toplevel 2>/dev/null) || {
      echo 'hygiene: not-a-git-repository: --staged needs a git work tree' >&2
      exit 2
    }
    cd "$root"
    git diff --cached --name-only -z --diff-filter=ACMR > "$tmp/list"
    GBL_HY_STAGED=1
    GBL_HY_TMP="$tmp/blob"
    export GBL_HY_STAGED GBL_HY_TMP
    ;;
  --dir)
    dir=$2
    [ -d "$dir" ] || { echo "hygiene: usage: not a directory: $dir" >&2; exit 2; }
    find "$dir" -type f -print0 > "$tmp/list"
    ;;
esac

count=$(tr -cd '\0' < "$tmp/list" | wc -c | tr -d ' ')

xargs -0 sh -c "$GBL_HY_WORKER" sh < "$tmp/list" > "$tmp/findings"

# Identity (staged mode only): the author and committer addresses must be a
# GitHub noreply address, so a personal address is not published by a commit.
# The address itself is never printed.
if [ "$mode" = "--staged" ]; then
  for role in author committer; do
    case "$role" in
      author) ident=$(git var GIT_AUTHOR_IDENT 2>/dev/null || true) ;;
      committer) ident=$(git var GIT_COMMITTER_IDENT 2>/dev/null || true) ;;
    esac
    addr=$(printf '%s\n' "$ident" | sed -n 's/^[^<]*<\([^>]*\)>.*/\1/p')
    case "$addr" in
      *@users.noreply.github.com | noreply@github.com) ;;
      *) echo "hygiene: identity: $role" >> "$tmp/findings" ;;
    esac
  done
fi

if [ -s "$tmp/findings" ]; then
  cat "$tmp/findings"
  exit 1
fi
echo "hygiene: ok ($count files)"
