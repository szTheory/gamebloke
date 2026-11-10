#!/bin/sh
# Decides whether a change needs CI's heavy lanes (D-02, D-11).
#
#   classify-changes.sh <changed-file-count>      changed paths on stdin
#
# Reads the changed paths on standard input, one per line, with both the old
# and the new name of a rename, and takes the pull request's changed-file
# count as its argument. Prints exactly one line, `heavy=false` or
# `heavy=true`, and a one-line reason on standard error.
#
# Fault prevented: a code change skipping CI as "planning only". `heavy=false`
# is printed only when ALL of these hold; every other input fails closed:
#   - at least one path is listed;
#   - the number of listed names is at least the changed-file count (a rename
#     adds a second name, so a complete listing is never shorter; the
#     paginated API can cut a listing short without an error);
#   - every listed path is under `.planning/` (a literal prefix followed by at
#     least one character; `.planningx/a` and `.planning-evil/x` are not);
#   - no `(listing failed)` line appears (the workflow adds it when the API
#     call fails; it is not under `.planning/`, so it counts as heavy);
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

listed=0
other=0
failed=0
while IFS= read -r path || [ -n "$path" ]; do
  [ -n "$path" ] || continue
  listed=$((listed + 1))
  case "$path" in
    '(listing failed)') failed=1 ;;
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
[ "$listed" -gt 0 ] || verdict true "empty listing"
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
if [ "$listed" -lt "$count" ]; then
  verdict true "short listing ($listed names for $count files)"
fi
verdict false "planning-only ($listed paths)"
