#!/bin/sh
set -eu

if [ -n "${CI:-}" ]; then
  printf '%s\n' 'play-playable0-terminal: refused because CI is set' >&2
  exit 2
fi

source_dir=$(CDPATH= cd "$(dirname "$0")/.." && pwd -P)
build_dir="$source_dir/build/libretro-local"
verify_script="$source_dir/scripts/verify-libretro-local.sh"
mode=launch

usage() {
  printf '%s\n' 'usage: sh scripts/play-playable0-terminal.sh [--check|--smoke]' >&2
  exit 2
}

if [ "$#" -gt 1 ]; then usage; fi
if [ "$#" -eq 1 ]; then
  case $1 in
    --check) mode=check ;;
    --smoke) mode=smoke ;;
    *) usage ;;
  esac
fi

case "$(uname -s)" in
  Darwin) core="$build_dir/gamebloke_libretro.dylib" ;;
  Linux) core="$build_dir/gamebloke_libretro.so" ;;
  *) printf 'play-playable0-terminal: unsupported host %s\n' "$(uname -s)" >&2; exit 1 ;;
esac

host="$build_dir/gamebloke-terminal"
content="$build_dir/generated/fixtures/owned-playable0-interactive.gba"
sh "$verify_script"

if [ "$mode" = check ]; then
  "$host" --check "$core" "$content"
  printf 'play-playable0-terminal: checks passed\nHost: %s\nCore: %s\nGuest: %s\n' \
    "$host" "$core" "$content"
  exit 0
fi

if [ "$mode" = smoke ]; then
  printf '%s\n' 'play-playable0-terminal: bounded PTY smoke passed through verify-libretro-local'
  exit 0
fi

printf '%s\n' 'Gamebloke is running the original local BIOS-free Playable 0 demo.'
printf '%s\n' 'Controls: D/F press/release RIGHT; J/K press/release A; Space releases all; R resets; Q quits.'
printf '%s\n' 'The view is a 60x20 terminal preview with mode-3 video only; no audio, saves, or retail compatibility are included.'
exec "$host" "$core" "$content"
