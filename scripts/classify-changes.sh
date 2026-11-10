#!/bin/sh
# Decides whether a change needs CI's heavy lanes (D-02, D-11).
#
#   classify-changes.sh <changed-file-count>      marked changed paths on stdin
#
# Reads one marker line per API entry on standard input and takes the pull
# request's changed-file count as its argument. The workflow emits `F <path>`
# for every entry's new name and, for a renamed file, an extra `R <old path>`
# line (see the `changes` job in .github/workflows/ci.yml). A line with neither
# marker is not a path and counts as a non-planning path. Prints exactly one
# line, `heavy=false` or `heavy=true`, and a one-line reason on standard error.
#
# Fault prevented: a code change skipping CI as "planning only". `heavy=false`
# is printed only when ALL of these hold; every other input fails closed:
#   - at least one path is listed;
#   - the number of `F` entries is at least the changed-file count (the
#     paginated API can cut a listing short without an error). Names are NOT
#     counted: each rename carries an extra `R` name, so counting names would let
#     renames pad a truncated listing up to the count;
#   - every `F` and `R` path is under `.planning/` (a literal prefix followed by
#     at least one character; `.planningx/a` and `.planning-evil/x` are not);
#   - no `(listing failed)` line appears (the workflow adds it when the API
#     call fails; it has no marker, so it counts as heavy);
#   - the count is at most 3000 (the files API lists at most 3000).
# The whole input is read, so the writer never meets a closed pipe. Paths are
# compared as bytes.
#
# Exit codes: 0 verdict printed, 2 usage (count missing or not all digits).
set -eu
LC_ALL=C
export LC_ALL

case ${1-} in
  '' | *[!0-9]*)
    echo "classify: usage: classify-changes.sh <changed-file-count> < paths" >&2
    cat >/dev/null
    exit 2
    ;;
esac
count=$1

entries=0
other=0
failed=0
while IFS= read -r line || [ -n "$line" ]; do
  [ -n "$line" ] || continue
  case "$line" in
    '(listing failed)')
      failed=1
      continue
      ;;
    'F '*)
      entries=$((entries + 1))
      path=${line#F }
      ;;
    'R '*) path=${line#R } ;;
    *) path='' ;;
  esac
  case "$path" in
    .planning/?*) ;;
    *) other=$((other + 1)) ;;
  esac
done

verdict() {
  echo "heavy=$1"
  echo "classify: $2" >&2
  exit 0
}

[ "$failed" -eq 0 ] || verdict true "listing failed"
[ "$entries" -gt 0 ] || verdict true "empty listing"
[ "$other" -eq 0 ] || verdict true "non-planning path"
# Strip leading zeros so a count such as 0003 is not read as octal, and treat
# a number too long for shell arithmetic as over the cap.
while :; do
  case $count in
    0?*) count=${count#0} ;;
    *) break ;;
  esac
done
if [ "${#count}" -gt 9 ] || [ "$count" -gt 3000 ]; then
  verdict true "over cap"
fi
if [ "$entries" -lt "$count" ]; then
  verdict true "short listing ($entries entries for $count files)"
fi
verdict false "planning-only ($entries entries)"
