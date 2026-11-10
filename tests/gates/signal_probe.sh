#!/bin/sh
# Signal probe for the scan scripts (CR-01).
#
#   signal_probe.sh <script> <finding-regex> <work-dir>
#
# Runs `sh <script> --dir <planted dir>` while a shim `xargs` holds the scan
# open, sends the shell a signal at that moment, and requires that the script
# ends with the signal's exit status (143 for TERM, 129 for HUP) and prints no
# `ok (` line. A trap action that returns lets the script resume with its
# temporary directory gone and report a clean result; that is the failure this
# probe exists to catch (`signal-probe: resumed`).
#
# The planted directory holds one real finding (a ROM-named file, which both
# scan scripts refuse, and a home-style path line for the hygiene scan), and an
# unsignalled run must report it first, so a "clean" signalled run cannot be
# explained by an empty sandbox (`signal-probe: baseline`).
#
# SIGINT is not probed: a background job of a non-interactive shell starts with
# SIGINT ignored, and an ignored signal cannot be trapped. scripts/check-traps.sh
# checks the INT handler statically.
#
# Reason codes: baseline, not-reached, resumed, wrong-exit, usage.
set -u
LC_ALL=C
export LC_ALL

[ "$#" -eq 3 ] || { echo 'signal-probe: usage: signal_probe.sh <script> <finding-regex> <work-dir>' >&2; exit 2; }
script=$1
finding_re=$2
work=$3

real_xargs=$(command -v xargs) || { echo 'signal-probe: usage: no xargs on PATH' >&2; exit 2; }

rm -rf "$work"
mkdir -p "$work/bin" "$work/scan"

# The planted finding. The path fragments are joined here so no committed line
# is itself a home-style path.
printf '%s%s\n' '/Use' 'rs/someone/x' > "$work/scan/notes.txt"
printf 'x\n' > "$work/scan/planted.gba"

# Shim: the first call announces itself, then waits for the probe's go file, so
# the signal lands while the script waits on a foreground child.
cat > "$work/bin/xargs" <<'SHIM'
#!/bin/sh
if [ ! -f "$GBL_PROBE_DIR/ready" ]; then
  : > "$GBL_PROBE_DIR/ready"
  i=0
  while [ ! -f "$GBL_PROBE_DIR/go" ] && [ "$i" -lt 30 ]; do
    sleep 1
    i=$((i + 1))
  done
fi
exec "$GBL_REAL_XARGS" "$@"
SHIM
chmod +x "$work/bin/xargs"

# Baseline: the planted input is a finding when nothing interrupts the scan.
base=$(sh "$script" --dir "$work/scan" 2>&1)
base_rc=$?
if [ "$base_rc" -ne 1 ] || ! printf '%s\n' "$base" | grep -Eq -e "$finding_re"; then
  echo "signal-probe: baseline: the planted input was not reported (exit $base_rc)"
  printf '%s\n' "$base"
  exit 1
fi

fail=0
for pair in TERM:143 HUP:129; do
  sig=${pair%%:*}
  want=${pair#*:}
  run=$work/$sig
  mkdir -p "$run"
  PATH="$work/bin:$PATH" GBL_PROBE_DIR=$run GBL_REAL_XARGS=$real_xargs \
    sh "$script" --dir "$work/scan" > "$run/out" 2>&1 &
  pid=$!

  i=0
  while [ ! -f "$run/ready" ] && [ "$i" -lt 30 ]; do
    sleep 1
    i=$((i + 1))
  done
  if [ ! -f "$run/ready" ]; then
    kill -s KILL "$pid" 2>/dev/null
    wait "$pid" 2>/dev/null
    echo "signal-probe: not-reached: $sig: the scan never reached xargs"
    fail=1
    continue
  fi

  kill -s "$sig" "$pid"
  : > "$run/go"
  wait "$pid"
  rc=$?

  if grep -Eq 'ok \(' "$run/out"; then
    echo "signal-probe: resumed: $sig: the script reported ok after the signal (exit $rc)"
    fail=1
  elif [ "$rc" -ne "$want" ]; then
    echo "signal-probe: wrong-exit: $sig: wanted $want, got $rc"
    fail=1
  else
    echo "signal-probe: $sig ok (exit $rc)"
  fi
done

[ "$fail" -eq 0 ] || exit 1
echo 'signal-probe: ok'
