#!/bin/sh
# check-junit.sh JUNIT INVENTORY [EXCLUDE_LABEL_REGEX]
#
# EXCLUDE_LABEL_REGEX is matched against each WHOLE label (it is anchored here,
# like the ctest preset's `^post$`), so a label that merely contains the text,
# such as `postgres`, is never excluded from the expected set.
#
# Compares the CTest junit file against the sorted tests/inventory/<lane>.txt.
# ctest exits 0 for skipped and disabled tests, so this check is what makes a
# silently omitted test fail the verdict. Every finding is reported before the
# script exits. Reason lines (stable; negative controls match on them):
#   inventory: no-junit: <path>
#   inventory: empty-junit
#   inventory: missing: <name>
#   inventory: extra: <name>
#   inventory: duplicate: <name>
#   inventory: failed: <name>
#   inventory: disabled: <name>
#   inventory: skipped-not-allowed: <name>
#   inventory: skipped-no-reason: <name>
# Every skipped test that is allowed is listed as
#   SKIPPED <name>: <mechanism> | <reason>
# and one summary line is always printed, which later plans and CI grep:
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

nojunit=0
junit_arg=$junit
if [ ! -f "$junit" ]; then
  echo "inventory: no-junit: $junit"
  junit_arg=/dev/null
  nojunit=1
fi

awk -v exclude="$exclude" -v nojunit="$nojunit" '
# Value of the first attribute named key (written as " key=\"...\"") in tag.
function attr(tag, key,    s) {
  if (match(tag, " " key "=\"[^\"]*\"")) {
    s = substr(tag, RSTART + length(key) + 3, RLENGTH - length(key) - 4)
    return s
  }
  return ""
}
BEGIN {
  # Anchor the whole label: a leading ^ or trailing $ the caller wrote is
  # dropped first, so `post` and `^post$` mean the same thing.
  exclude_re = exclude
  sub(/^\^/, "", exclude_re)
  sub(/\$$/, "", exclude_re)
  exclude_re = "^(" exclude_re ")$"
}
FNR == 1 { fileno++ }
fileno == 1 {
  # inventory line: "<name> <labels>[ skip-ok]"
  if ($0 ~ /^#/ || $0 ~ /^[ \t]*$/) next
  name = $1
  labels = $2
  skipok[name] = ($3 == "skip-ok")
  skip = 0
  if (exclude != "") {
    n = split(labels, parts, ",")
    for (i = 1; i <= n; i++) if (parts[i] ~ exclude_re) skip = 1
  }
  if (!skip) { expected[name] = 1; order[++norder] = name; nexpected++ }
  listed[name] = 1
  next
}
fileno == 2 { buf = buf $0 "\n" }
END {
  # One chunk per testcase. system-out can hold any text, including
  # semicolons, so the junit is split in awk and never held in a CMake list.
  ncases = split(buf, chunks, "<testcase ") - 1
  if (ncases < 0) ncases = 0
  if (!nojunit && ncases == 0) { print "inventory: empty-junit"; bad = 1 }
  for (i = 2; i <= ncases + 1; i++) {
    chunk = chunks[i]
    gt = index(chunk, ">")
    tag = substr(chunk, 1, gt)
    body = substr(chunk, gt + 1)
    cur = attr(" " tag, "name")
    if (!(cur in seen)) seq[++nseq] = cur
    seen[cur]++
    status[cur] = attr(" " tag, "status")
    if (index(body, "<failure") || index(body, "<error")) failed[cur] = 1
    sk = index(body, "<skipped")
    if (sk) {
      skipped[cur] = 1
      stag = substr(body, sk, index(substr(body, sk), ">"))
      mech[cur] = attr(stag, "message")
    }
    so = index(body, "<system-out>")
    if (so) {
      out = substr(body, so + 12)
      ec = index(out, "</system-out>")
      if (ec) out = substr(out, 1, ec - 1)
      r = index(out, "SKIPPED: ")
      if (r) {
        reason = substr(out, r + 9)
        nl = index(reason, "\n")
        if (nl) reason = substr(reason, 1, nl - 1)
        why[cur] = reason
      }
    }
  }

  ran = 0; nskipped = 0; allowed = 0
  for (i = 1; i <= norder; i++) {
    name = order[i]
    if (!(name in seen)) { print "inventory: missing: " name; bad = 1 }
  }
  for (i = 1; i <= nseq; i++) {
    name = seq[i]
    if (!(name in listed)) { print "inventory: extra: " name; bad = 1; continue }
    if (!(name in expected)) continue
    if (seen[name] > 1) { print "inventory: duplicate: " name; bad = 1; continue }
    if (status[name] == "disabled") {
      # CTest records no reason for DISABLED, so it is never allowed.
      print "inventory: disabled: " name; bad = 1; continue
    }
    if (name in failed || status[name] == "fail") {
      print "inventory: failed: " name; bad = 1; continue
    }
    if (name in skipped || status[name] == "notrun") {
      nskipped++
      if (!skipok[name]) {
        print "inventory: skipped-not-allowed: " name; bad = 1; continue
      }
      if (!(name in why) || why[name] == "") {
        print "inventory: skipped-no-reason: " name; bad = 1; continue
      }
      allowed++
      print "SKIPPED " name ": " mech[name] " | " why[name]
      continue
    }
    if (status[name] == "run") { ran++; continue }
    print "inventory: failed: " name; bad = 1
  }
  printf "inventory: expected %d, ran %d, skipped %d (allowed %d)\n", nexpected + 0, ran, nskipped, allowed
  if (nojunit) bad = 1
  exit bad
}
' "$inventory" "$junit_arg"
