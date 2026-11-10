#!/bin/sh
# Rights signature scan (EVID-04): refuses Nintendo logo bytes, known BIOS
# images, ROM/save file names, and unlisted binary files, using committed
# SHA-256 digests only. No logo, BIOS, or ROM bytes are stored or printed.
#
#   signature-scan.sh --tree                  tracked plus untracked-not-ignored files
#   signature-scan.sh --dir <path>            every file under <path>, no git (controls)
#   signature-scan.sh --extra-digests <file>  test-only extra rows: `<64hex> logo:<label>` or `bios:<label>`
#   signature-scan.sh --allowlist <file>      overrides the default allowlist (controls)
#   signature-scan.sh --print-config          print the default list paths and exit 0
#
# Rules (each finding prints `signature: <code>: <path>`):
#   logo-window             SHA-256 of bytes 4..159 of any regular file of at least 160 bytes
#                           is a listed logo digest. Never allowlisted.
#   bios-digest             SHA-256 of a file of exactly 16384 bytes is a listed BIOS digest.
#                           Never allowlisted.
#   rom-extension           name ends .gba .agb .mb .bin .rom .bios .sav .srm .sa1 .eep
#                           .eeprom .fla .flash or .ss0-.ss9 (case-insensitive) and the
#                           path is not allowlisted
#   binary-not-allowlisted  the file is binary (grep -I) and not allowlisted
#
# Other reason codes: bad-digest-row (a malformed row in a digest list),
# missing-list (a list file is absent), usage.
# Exit 0 clean, 1 on findings, 2 on a usage error. Hashing needs the CMake named by
# the CMAKE environment variable (default: cmake on PATH).
set -eu
LC_ALL=C
export LC_ALL

GBL_ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
GBL_LOGO_LIST_DEFAULT=$GBL_ROOT/tests/signatures/logo.sha256
GBL_BIOS_LIST_DEFAULT=$GBL_ROOT/tests/signatures/bios.sha256
GBL_ALLOWLIST_DEFAULT=$GBL_ROOT/tests/signatures/allowlist.txt
GBL_SIG_LIB=$GBL_ROOT/scripts/lib/signature-common.sh
export GBL_SIG_LIB

usage() {
  echo 'signature: usage: signature-scan.sh --tree | --dir <path> [--extra-digests <file>] [--allowlist <file>] | --print-config' >&2
  exit 2
}

# abspath <path>: made absolute before the scan changes directory.
abspath() {
  case $1 in
    /*) printf '%s\n' "$1" ;;
    *) printf '%s\n' "$PWD/$1" ;;
  esac
}

mode=""
dir=""
extra=""
allow=$GBL_ALLOWLIST_DEFAULT
print_config=0
while [ "$#" -gt 0 ]; do
  case $1 in
    --tree)
      [ -z "$mode" ] || usage
      mode=tree
      shift
      ;;
    --dir)
      [ "$#" -ge 2 ] && [ -z "$mode" ] || usage
      mode=dir
      dir=$2
      shift 2
      ;;
    --extra-digests)
      [ "$#" -ge 2 ] || usage
      extra=$(abspath "$2")
      shift 2
      ;;
    --allowlist)
      [ "$#" -ge 2 ] || usage
      allow=$(abspath "$2")
      shift 2
      ;;
    --print-config)
      print_config=1
      shift
      ;;
    *) usage ;;
  esac
done

if [ "$print_config" -eq 1 ]; then
  echo "logo-list: $GBL_LOGO_LIST_DEFAULT"
  echo "bios-list: $GBL_BIOS_LIST_DEFAULT"
  echo "allowlist: $GBL_ALLOWLIST_DEFAULT"
  exit 0
fi
[ -n "$mode" ] || usage

# shellcheck source=lib/signature-common.sh
. "$GBL_SIG_LIB"

tmp=$(mktemp -d "${TMPDIR:-/tmp}/gbl-signature.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
GBL_SIG_TMP=$tmp
export GBL_SIG_TMP
: > "$tmp/findings"
: > "$tmp/logo.hex"
: > "$tmp/bios.hex"
: > "$tmp/badrows"
: > "$tmp/wlist"
: > "$tmp/blist"
echo 0 > "$tmp/n"

# A list that is absent fails closed: a missing digest list would mean no
# protection, so it is never read as empty.
for list in "$GBL_LOGO_LIST_DEFAULT" "$GBL_BIOS_LIST_DEFAULT" "$allow"; do
  if [ ! -f "$list" ]; then
    echo "signature: missing-list: $list"
    exit 1
  fi
done
if [ -n "$extra" ] && [ ! -f "$extra" ]; then
  echo "signature: missing-list: $extra"
  exit 1
fi

gbl_sig_load_list "$GBL_LOGO_LIST_DEFAULT" logo
gbl_sig_load_list "$GBL_BIOS_LIST_DEFAULT" bios
if [ -n "$extra" ]; then
  gbl_sig_load_list "$extra" extra
fi
if [ -s "$tmp/badrows" ]; then
  while IFS= read -r row; do
    echo "signature: bad-digest-row: $row"
  done < "$tmp/badrows"
  exit 1
fi

# Allowlist: repository-relative paths, `#` comments and blank lines ignored.
grep -v -e '^#' -e '^$' "$allow" > "$tmp/allow" || true

case "$mode" in
  tree)
    root=$(git -C "$GBL_ROOT" rev-parse --show-toplevel 2>/dev/null) || {
      echo 'signature: not-a-git-repository: --tree needs a git work tree' >&2
      exit 2
    }
    cd "$root"
    git ls-files -z --cached --others --exclude-standard > "$tmp/list"
    ;;
  dir)
    [ -d "$dir" ] || { echo "signature: usage: not a directory: $dir" >&2; exit 2; }
    cd "$dir"
    find . -type f -print0 > "$tmp/list"
    ;;
esac

xargs -0 sh -c '. "$GBL_SIG_LIB"; gbl_sig_scan_files "$@"; exit 0' sh < "$tmp/list"
count=$(cat "$tmp/n")

# One batched hash per kind. Window digests (w) and whole-file digests (b) come
# out as `<hex> <n>`; the path for <n> was saved by the worker.
for kind in w b; do
  : > "$tmp/sums.$kind"
  if [ -s "$tmp/${kind}list" ]; then
    xargs -n 100 sh -c '. "$GBL_SIG_LIB"; gbl_sig_hash_batch "$@"' sh "$kind" \
      < "$tmp/${kind}list" > "$tmp/raw.$kind" || {
      echo 'signature: hash-failed: could not run cmake -E sha256sum' >&2
      exit 2
    }
    sed -n 's/^\([0-9a-f]\{64\}\)  [wb]\.\([0-9][0-9]*\)$/\1 \2/p' \
      "$tmp/raw.$kind" > "$tmp/sums.$kind"
  fi
done

report_matches() {
  # report_matches <digest-list> <sums> <code>
  awk 'FILENAME == ARGV[1] { set[$1] = 1; next } ($1 in set) { print $2 }' "$1" "$2" |
    while IFS= read -r num; do
      printf 'signature: %s: ' "$3" >> "$tmp/findings"
      cat "$tmp/path.$num" >> "$tmp/findings"
      echo >> "$tmp/findings"
    done
}
report_matches "$tmp/logo.hex" "$tmp/sums.w" logo-window
report_matches "$tmp/bios.hex" "$tmp/sums.b" bios-digest

if [ -s "$tmp/findings" ]; then
  cat "$tmp/findings"
  exit 1
fi
echo "signature: ok ($count files)"
