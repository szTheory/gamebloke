#!/bin/sh
# check-junit.sh JUNIT INVENTORY [EXCLUDE_LABEL_REGEX]
#
# Compares the CTest junit file against the sorted tests/inventory/<lane>.txt.
# Tracer subset (plan 02 completes the contract). Reason lines:
#   inventory: no-junit
#   inventory: missing: <name>
#   inventory: extra: <name>
#   inventory: failed: <name>
# Always prints one summary line, which later plans and CI grep verbatim:
#   inventory: expected N, ran M, skipped K (allowed J)
# Exit codes: 0 ok, 1 inventory failure, 2 usage error.
set -eu
LC_ALL=C
export LC_ALL

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
  echo "check-junit: usage: check-junit.sh JUNIT INVENTORY [EXCLUDE_LABEL_REGEX]" >&2
  exit 2
fi
junit=$1
inventory=$2
exclude=${3:-}

if [ ! -f "$inventory" ]; then
  echo "check-junit: usage: inventory not found: $inventory" >&2
  exit 2
fi

junit_arg=$junit
if [ ! -f "$junit" ]; then
  echo "inventory: no-junit: $junit"
  junit_arg=/dev/null
fi

awk -v exclude="$exclude" -v nojunit="$([ -f "$junit" ] && echo 0 || echo 1)" '
function attr(line, key,    m, s) {
  if (match(line, key "=\"[^\"]*\"")) {
    s = substr(line, RSTART + length(key) + 2, RLENGTH - length(key) - 3)
    return s
  }
  return ""
}
function finish_case() {
  incase = 0
}
FNR == 1 { fileno++ }
fileno == 1 {
  # inventory: "<name> <labels>[ skip-ok]"
  if ($0 ~ /^#/ || $0 ~ /^[ \t]*$/) next
  name = $1
  labels = $2
  listed[name] = 1
  skipok[name] = ($3 == "skip-ok")
  skip = 0
  if (exclude != "") {
    n = split(labels, parts, ",")
    for (i = 1; i <= n; i++) if (parts[i] ~ exclude) skip = 1
  }
  if (!skip) { expected[name] = 1; nexpected++ }
  next
}
fileno == 2 {
  if (insysout) {
    if (index($0, "</system-out>")) insysout = 0
    next
  }
  if (index($0, "<system-out>") && !index($0, "</system-out>")) { insysout = 1; next }
  if (index($0, "<testcase ")) {
    cur = attr($0, "name")
    st = attr($0, "status")
    seen[cur]++
    status[cur] = st
    incase = !($0 ~ /\/>[ \t]*$/)
    next
  }
  if (incase) {
    if (index($0, "<failure") || index($0, "<error")) failed[cur] = 1
    if (index($0, "<skipped")) skipped[cur] = 1
    if (index($0, "</testcase>")) incase = 0
  }
}
END {
  bad = 0; ran = 0; nskipped = 0; allowed = 0
  for (name in expected) {
    if (!(name in seen)) { print "inventory: missing: " name; bad = 1 }
  }
  for (name in seen) {
    if (!(name in listed)) { print "inventory: extra: " name; bad = 1; continue }
    if (!(name in expected)) continue
    if (status[name] == "run" && !(name in failed)) { ran++; continue }
    if (name in skipped) {
      nskipped++
      if (skipok[name]) allowed++
    }
    print "inventory: failed: " name; bad = 1
  }
  printf "inventory: expected %d, ran %d, skipped %d (allowed %d)\n", nexpected + 0, ran, nskipped, allowed
  if (nojunit) bad = 1
  exit bad
}
' "$inventory" "$junit_arg"
