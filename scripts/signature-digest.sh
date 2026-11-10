#!/bin/sh
# Owner helper (D-08): prints only the SHA-256 digest of a file's Nintendo logo
# window, or of a BIOS-sized file, so the owner can supply the committed digest
# without any bytes entering the repository.
#
#   signature-digest.sh logo <file>   bytes 4..159 (156 bytes); file of at least 160 bytes
#                                     prints `<64hex> nintendo-logo-window`
#   signature-digest.sh bios <file>   file of exactly 16384 bytes
#                                     prints `<64hex> bios`
#
# The helper reads the file in place, writes only a temporary window file that
# it deletes on exit, and prints only a digest. Run it as the owner on a file
# outside the repository. Hashing uses the CMake named by the CMAKE environment
# variable (default: cmake on PATH).
#
# Reason codes: too-short (logo, under 160 bytes), wrong-size (bios, not 16384
# bytes), no-file, usage. Exit 0 ok, 1 refused, 2 usage error.
set -eu
LC_ALL=C
export LC_ALL

GBL_ROOT=$(cd "$(dirname "$0")/.." && pwd -P)
# shellcheck source=lib/signature-common.sh
. "$GBL_ROOT/scripts/lib/signature-common.sh"

usage() {
  echo 'signature-digest: usage: signature-digest.sh logo <file> | bios <file>' >&2
  exit 2
}

[ "$#" -eq 2 ] || usage
kind=$1
file=$2
case "$kind" in
  logo | bios) ;;
  *) usage ;;
esac
# A leading dash must not be read as an option by the tools below.
case "$file" in
  -*) file=./$file ;;
esac
[ -f "$file" ] || { echo "signature-digest: no-file: $file"; exit 1; }

tmp=$(mktemp -d "${TMPDIR:-/tmp}/gbl-sigdigest.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

size=$(gbl_size "$file")
case "$kind" in
  logo)
    if [ "$size" -lt 160 ]; then
      echo 'signature-digest: too-short: a logo window needs a file of at least 160 bytes'
      exit 1
    fi
    gbl_window_file "$file" "$tmp/window"
    set -- $(gbl_sha256 "$tmp/window")
    printf '%s nintendo-logo-window\n' "$1"
    ;;
  bios)
    if [ "$size" -ne 16384 ]; then
      echo 'signature-digest: wrong-size: a BIOS image is exactly 16384 bytes'
      exit 1
    fi
    set -- $(gbl_sha256 "$file")
    printf '%s bios\n' "$1"
    ;;
esac
