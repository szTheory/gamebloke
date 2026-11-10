#!/bin/sh
# Shared shell functions for scripts/signature-scan.sh and
# scripts/signature-digest.sh. Sourced, never executed. Prints only paths and
# digests: file bytes never reach stdout, stderr, or a log.

# gbl_cmake <args>: run the CMake binary named by the CMAKE environment
# variable (tests pass ${CMAKE_COMMAND}), defaulting to `cmake` on PATH.
gbl_cmake() {
  "${CMAKE:-cmake}" "$@"
}

# gbl_window_file <src> <dst>: copy bytes 4..159 of <src> (156 bytes, 39 blocks
# of 4 after skipping the first) to <dst>.
gbl_window_file() {
  dd if="$1" bs=4 skip=1 count=39 of="$2" 2>/dev/null
}

# gbl_sha256 <file>...: one `cmake -E sha256sum` over many files; prints
# `<hex>  <file>` lines. Not file(READ HEX) with string(SHA256): that hashes the
# hex text, not the bytes.
gbl_sha256() {
  gbl_cmake -E sha256sum "$@"
}

# gbl_size <file>: byte count, whitespace stripped.
gbl_size() {
  wc -c < "$1" | tr -d ' \t\n'
}

# ---- scan internals (used through xargs -> sh -c, which does not inherit
# functions, so the scan sources this file inside each worker) ----------------

# gbl_sig_is_allowed <path>: exact line match in the cleaned allowlist. A name
# holding a newline is never allowed (grep -F would split it into patterns).
gbl_sig_is_allowed() {
  case $1 in
    *'
'*) return 1 ;;
  esac
  grep -Fxq -- "$1" "$GBL_SIG_TMP/allow"
}

# gbl_sig_scan_files <path>...: per-file rules. Findings for the extension and
# binary rules are appended to $GBL_SIG_TMP/findings. Files that need hashing
# are copied (window or whole) under a numbered name; hashing happens later in
# batches, with the original path kept in path.<n>.
gbl_sig_scan_files() {
  n=$(cat "$GBL_SIG_TMP/n")
  for f; do
    [ -L "$f" ] && continue
    [ -f "$f" ] || continue
    n=$((n + 1))
    p=${f#./}
    size=$(gbl_size "$f")
    printf '%s' "$p" > "$GBL_SIG_TMP/path.$n"

    # Rule 1 (logo-window): every file of at least 160 bytes, not only the
    # ones detected as binary, so a crafted file without NUL bytes cannot evade.
    if [ "$size" -ge 160 ]; then
      gbl_window_file "$f" "$GBL_SIG_TMP/w.$n"
      echo "$n" >> "$GBL_SIG_TMP/wlist"
    fi
    # Rule 2 (bios-digest): every file of exactly 16384 bytes.
    if [ "$size" -eq 16384 ]; then
      cp -- "$f" "$GBL_SIG_TMP/b.$n"
      echo "$n" >> "$GBL_SIG_TMP/blist"
    fi

    # Rules 3 and 4 are the only ones the allowlist exempts.
    if ! gbl_sig_is_allowed "$p"; then
      lower=$(printf '%s' "$p" | tr 'A-Z' 'a-z')
      case $lower in
        *.gba | *.agb | *.mb | *.bin | *.rom | *.bios | *.sav | *.srm | *.sa1 | \
        *.eep | *.eeprom | *.fla | *.flash | *.ss[0-9])
          printf 'signature: rom-extension: %s\n' "$p" >> "$GBL_SIG_TMP/findings" ;;
      esac
      # An empty file counts as text: grep -I exits 1 on it, so test size first.
      if [ -s "$f" ] && ! grep -Iq '' -- "$f" 2>/dev/null; then
        printf 'signature: binary-not-allowlisted: %s\n' "$p" >> "$GBL_SIG_TMP/findings"
      fi
    fi
  done
  printf '%s\n' "$n" > "$GBL_SIG_TMP/n"
  return 0
}

# gbl_sig_hash_batch <w|b> <n>...: hash $GBL_SIG_TMP/<kind>.<n> files in one
# cmake call (relative names, so the tmp path never reaches the output).
gbl_sig_hash_batch() {
  kind=$1
  shift
  names=""
  for num; do
    names="$names $kind.$num"
  done
  cd "$GBL_SIG_TMP" || return 1
  # shellcheck disable=SC2086  # names are `w.<digits>` / `b.<digits>`
  gbl_sha256 $names
}

# gbl_sig_load_list <file> <kind>: append the digests of <file> to
# $GBL_SIG_TMP/logo.hex or bios.hex, and report malformed rows to
# $GBL_SIG_TMP/badrows. kind: logo | bios | extra.
#   logo  : `<64hex> <label>` or the exact marker row `owner-pending <label>`
#   bios  : `<64hex> <label>`
#   extra : `<64hex> logo:<label>` or `<64hex> bios:<label>` (controls only)
gbl_sig_load_list() {
  lf=$1
  kind=$2
  ln=0
  while IFS= read -r line || [ -n "$line" ]; do
    ln=$((ln + 1))
    case $line in
      '' | '#'*) continue ;;
    esac
    hex=${line%% *}
    case $kind in
      logo)
        if printf '%s\n' "$line" | grep -Eq '^[0-9a-f]{64} [^ ]+$'; then
          echo "$hex" >> "$GBL_SIG_TMP/logo.hex"
        elif printf '%s\n' "$line" | grep -Eq '^owner-pending [^ ]+$'; then
          :
        else
          echo "$lf:$ln" >> "$GBL_SIG_TMP/badrows"
        fi
        ;;
      bios)
        if printf '%s\n' "$line" | grep -Eq '^[0-9a-f]{64} [^ ]+$'; then
          echo "$hex" >> "$GBL_SIG_TMP/bios.hex"
        else
          echo "$lf:$ln" >> "$GBL_SIG_TMP/badrows"
        fi
        ;;
      extra)
        if printf '%s\n' "$line" | grep -Eq '^[0-9a-f]{64} logo:[^ ]+$'; then
          echo "$hex" >> "$GBL_SIG_TMP/logo.hex"
        elif printf '%s\n' "$line" | grep -Eq '^[0-9a-f]{64} bios:[^ ]+$'; then
          echo "$hex" >> "$GBL_SIG_TMP/bios.hex"
        else
          echo "$lf:$ln" >> "$GBL_SIG_TMP/badrows"
        fi
        ;;
    esac
  done < "$lf"
}
