#!/bin/sh
# Static policy gate for the CI workflow (D-01, D-10, D-11, D-16).
#
#   check-workflow.sh [workflow-file]        default: .github/workflows/ci.yml
#
# Reads the workflow as text (POSIX sh plus awk; two-space-style block YAML
# like ci.yml) and reports every violation, so a later edit cannot quietly
# weaken the required check. Workflow preset names come from
# `cmake --workflow --list-presets` run at the repo root.
#
# Reason lines (a trailing `: <detail>` is a job id, preset name or line number):
#   workflow: moving-label: <line>           a runner value ending in -latest
#   workflow: path-filter: <line>            paths:/paths-ignore: under on:
#   workflow: pr-target: <line>              pull_request_target trigger
#   workflow: needs-missing: <job>           job absent from the CI required needs
#   workflow: required-check                 no job named `CI required` with if: always()
#   workflow: expression-in-run: <line>      ${{ ... }} in a run: value or block
#   workflow: non-preset-run: <job>          build/asan/hygiene run line that invokes
#                                            cmake, ctest, cpack, ninja or make other
#                                            than `cmake --workflow --preset <name>`
#   workflow: unknown-preset: <name>         preset not in --list-presets
#   workflow: no-timeout: <job>              job without timeout-minutes
#   workflow: permissions                    top-level permissions not exactly contents: read
#   workflow: persist-credentials: <line>    checkout without persist-credentials: false
#   workflow: continue-on-error: <line>      continue-on-error anywhere
#   workflow: no-jobs                        no job found (an unreadable shape fails closed)
#   workflow: ok (<n> jobs)                  nothing found
# Exit 1 on any finding; exit 2 on usage or when presets cannot be listed.
set -eu
LC_ALL=C
export LC_ALL

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if [ "$#" -gt 1 ]; then
  echo "workflow: usage: check-workflow.sh [workflow-file]" >&2
  exit 2
fi
file=${1:-"$root/.github/workflows/ci.yml"}
if [ ! -f "$file" ]; then
  echo "workflow: usage: not a file: $file" >&2
  exit 2
fi

if ! listing=$(cd "$root" && "${CMAKE:-cmake}" --workflow --list-presets 2>&1); then
  echo "workflow: usage: cmake --workflow --list-presets failed" >&2
  printf '%s\n' "$listing" >&2
  exit 2
fi
presets=$(printf '%s\n' "$listing" | sed -n 's/^[[:space:]]*"\([^"]*\)".*/\1/p' | tr '\n' ' ')
if [ -z "$presets" ]; then
  echo "workflow: usage: no workflow presets found" >&2
  exit 2
fi

awk -v presets="$presets" '
function trim(s) { sub(/^[ \t]+/, "", s); sub(/[ \t\r]+$/, "", s); return s }
function indent_of(s) { match(s, /^ */); return RLENGTH }
function unquote(s) {
  if (s ~ /^".*"$/ || s ~ /^\047.*\047$/) s = substr(s, 2, length(s) - 2)
  return s
}
function report(msg) { findings++; print msg }
function in_list(item, list,   n, a, i) {
  n = split(list, a, " ")
  for (i = 1; i <= n; i++) if (a[i] == item) return 1
  return 0
}
# Cut a trailing comment (a # at line start or after whitespace, outside quotes).
function strip_comment(s,   i, n, ch, q) {
  q = ""
  n = length(s)
  for (i = 1; i <= n; i++) {
    ch = substr(s, i, 1)
    if (q != "") { if (ch == q) q = ""; continue }
    if (ch == "\"" || ch == "\047") { q = ch; continue }
    if (ch == "#" && (i == 1 || substr(s, i - 1, 1) == " " || substr(s, i - 1, 1) == "\t")) return substr(s, 1, i - 1)
  }
  return s
}

BEGIN {
  heavy["build"] = 1; heavy["asan"] = 1; heavy["hygiene"] = 1
  cmd_re = "(^|[^A-Za-z0-9_./-])(cmake|ctest|cpack|ninja|make)([^A-Za-z0-9_-]|$)"
}

{
  sub(/\r$/, "")
  n = NR
  ind = indent_of($0)
  blank = ($0 ~ /^[ \t]*$/)

  # Inside a block scalar: content, not structure. Run blocks are kept for the
  # expression and preset rules (a ${{ in a shell comment is still expanded).
  if (inblk) {
    if (blank || ind > blk_key) {
      if (blk_run) {
        nruns++; rl_val[nruns] = $0; rl_blk[nruns] = 1; rl_job[nruns] = job; rl_line[nruns] = n
      }
      next
    }
    inblk = 0
  }
  if (blank) next
  t = trim(strip_comment($0))
  if (t == "") next

  dashed = 0
  kw = t
  keyind = ind
  if (kw ~ /^-( +|$)/) {
    dashed = 1
    sub(/^- */, "", kw)
    keyind = ind + (length(t) - length(kw))
  }
  nl++; lv[nl] = n
  tx[n] = t; kwd[n] = kw; dsh[n] = dashed; ind_of[n] = ind; kin[n] = keyind

  blockstart = 0
  if (kw ~ /^[A-Za-z0-9_.-]+:[ \t]*[|>][+-]?[0-9]*[+-]?$/) {
    blockstart = 1
    inblk = 1
    blk_key = keyind
    blk_run = (kw ~ /^run:/)
  }

  # Top-level key tracking.
  if (ind == 0 && !dashed && kw ~ /^[^ :][^:]*:/) {
    top = kw
    sub(/:.*/, "", top)
    top = unquote(top)
    if (top == "permissions") {
      perm_seen = 1
      pv = kw
      sub(/^[^:]*:[ \t]*/, "", pv)
      if (pv != "") perm_inline = pv
    }
    if (top == "jobs") { jobs_seen = 1; job_ind = 0; job = "" }
  } else if (top == "permissions" && ind > 0) {
    perm_children++
    pl = t
    gsub(/[ \t]+/, " ", pl)
    permline[perm_children] = pl
  }

  # Whole-file rules on any structural line.
  if (t ~ /[A-Za-z0-9]-latest([^A-Za-z0-9_.-]|$)/) report("workflow: moving-label: " n)
  if (t ~ /pull_request_target/) report("workflow: pr-target: " n)
  if (top == "on" && t ~ /(^|[^A-Za-z0-9_-])paths(-ignore)?[ \t]*:/) report("workflow: path-filter: " n)
  if (kw ~ /^continue-on-error[ \t]*:/) report("workflow: continue-on-error: " n)

  # Job structure.
  if (top == "jobs" && ind > 0) {
    if (job_ind == 0) job_ind = ind
    if (ind == job_ind && !dashed && kw ~ /^[A-Za-z0-9_-]+:[ \t]*$/) {
      job = kw
      sub(/:.*/, "", job)
      njobs++; jobid[njobs] = job
      jkey[job] = 0
      collecting = ""
    } else if (job != "") {
      if (jkey[job] == 0) jkey[job] = ind
      if (!dashed && ind == jkey[job]) {
        collecting = ""
        key = kw
        sub(/:.*/, "", key)
        val = kw
        sub(/^[^:]*:[ \t]*/, "", val)
        if (key == "name") jname[job] = unquote(val)
        if (key == "if") jif[job] = val
        if (key == "timeout-minutes") jtimeout[job] = 1
        if (key == "needs") {
          if (val == "") collecting = job
          else {
            gsub("[][,\"\047]", " ", val)
            jneeds[job] = jneeds[job] " " val
          }
        }
      } else if (dashed && collecting != "") {
        item = unquote(trim(kw))
        jneeds[collecting] = jneeds[collecting] " " item
      }
    }
  }

  # Inline run values (a block run was recorded line by line above).
  if (!blockstart && kw ~ /^run:[ \t]*[^ \t]/) {
    rv = kw
    sub(/^run:[ \t]*/, "", rv)
    nruns++; rl_val[nruns] = rv; rl_blk[nruns] = 0; rl_job[nruns] = job; rl_line[nruns] = n
  }
}

END {
  if (njobs == 0) report("workflow: no-jobs")

  # Run lines: no expression, and heavy lanes only call workflow presets.
  for (r = 1; r <= nruns; r++) {
    v = rl_val[r]
    if (index(v, "${{") > 0) report("workflow: expression-in-run: " rl_line[r])
    if (rl_job[r] in heavy) {
      s = trim(v)
      if (rl_blk[r] && s ~ /^#/) continue
      if (s ~ cmd_re) {
        if (s ~ /^cmake --workflow --preset [A-Za-z0-9_.-]+$/) {
          name = s
          sub(/^cmake --workflow --preset /, "", name)
          if (!in_list(name, presets)) report("workflow: unknown-preset: " name)
        } else {
          report("workflow: non-preset-run: " rl_job[r])
        }
      }
    }
  }

  # Every job has a timeout.
  for (j = 1; j <= njobs; j++)
    if (!(jobid[j] in jtimeout)) report("workflow: no-timeout: " jobid[j])

  # The single required check runs always and needs every other job.
  rid = ""
  for (j = 1; j <= njobs; j++) if (jname[jobid[j]] == "CI required") rid = jobid[j]
  if (rid == "" || jif[rid] != "always()") report("workflow: required-check")
  if (rid != "") {
    for (j = 1; j <= njobs; j++)
      if (jobid[j] != rid && !in_list(jobid[j], jneeds[rid])) report("workflow: needs-missing: " jobid[j])
  }

  # Top-level permissions are exactly contents: read.
  if (!perm_seen || perm_inline != "" || perm_children != 1 || permline[1] != "contents: read")
    report("workflow: permissions")

  # Every checkout step sets persist-credentials: false.
  for (i = 1; i <= nl; i++) {
    ln = lv[i]
    if (kwd[ln] !~ /^uses:[ \t]*["\047]?actions\/checkout@/) continue
    if (dsh[ln]) D = ind_of[ln]; else D = kin[ln] - 2
    s0 = i
    if (!dsh[ln]) {
      while (s0 > 1 && !(dsh[lv[s0]] && ind_of[lv[s0]] == D)) s0--
    }
    e0 = i
    while (e0 < nl && ind_of[lv[e0 + 1]] > D) e0++
    ok = 0
    for (k = s0; k <= e0; k++)
      if (kwd[lv[k]] ~ /^persist-credentials:[ \t]*["\047]?false["\047]?$/) ok = 1
    if (!ok) report("workflow: persist-credentials: " ln)
  }

  if (findings > 0) exit 1
  print "workflow: ok (" njobs " jobs)"
}
' "$file"
