#!/bin/sh
set -eu

if [ -n "${CI:-}" ]; then
  printf '%s\n' 'play-playable0-local: refused because CI is set' >&2
  exit 2
fi

source_dir=$(CDPATH= cd "$(dirname "$0")/.." && pwd -P)
build_dir="$source_dir/build/libretro-local"
retroarch_dir="$build_dir/retroarch"
verify_script="$source_dir/scripts/verify-libretro-local.sh"
mode=launch

usage() {
  printf '%s\n' 'usage: sh scripts/play-playable0-local.sh [--check|--smoke]' >&2
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
  Darwin)
    core="$build_dir/gamebloke_libretro.dylib"
    default_bin=/Applications/RetroArch.app/Contents/MacOS/RetroArch
    ;;
  Linux)
    core="$build_dir/gamebloke_libretro.so"
    default_bin=/usr/bin/retroarch
    ;;
  *) printf 'play-playable0-local: unsupported host %s\n' "$(uname -s)" >&2; exit 1 ;;
esac

content="$build_dir/generated/fixtures/owned-playable0-interactive.gba"
if [ -n "${RETROARCH_BIN:-}" ]; then
  retroarch_bin=$RETROARCH_BIN
else
  retroarch_bin=$default_bin
fi

if [ ! -x "$retroarch_bin" ]; then
  printf 'play-playable0-local: RetroArch executable not found or not executable: %s\n' \
    "$retroarch_bin" >&2
  printf '%s\n' 'Set RETROARCH_BIN to an installed RetroArch executable.' >&2
  exit 1
fi

if [ ! -f "$core" ] || [ ! -f "$content" ]; then
  printf '%s\n' 'play-playable0-local: local module or interactive guest is missing' >&2
  exit 1
fi

file "$retroarch_bin" | grep -qE 'Mach-O|ELF' || {
  printf 'play-playable0-local: override is not a native executable: %s\n' \
    "$retroarch_bin" >&2
  exit 1
}
file "$core" | grep -qE 'Mach-O|ELF' || {
  printf 'play-playable0-local: Gamebloke core is not a native module: %s\n' \
    "$core" >&2
  exit 1
}

host_arch=$(uname -m)
case $host_arch in
  aarch64|arm64)
    host_arch=arm64
    file_arch_pattern='aarch64|arm64'
    ;;
  amd64|x86_64)
    host_arch=x86_64
    file_arch_pattern='x86[-_]64'
    ;;
  *)
    file_arch_pattern=$host_arch
    ;;
esac
file "$retroarch_bin" | grep -qE "$file_arch_pattern" || {
  printf 'play-playable0-local: RetroArch architecture does not include host %s\n' \
    "$host_arch" >&2
  exit 1
}
file "$core" | grep -qE "$file_arch_pattern" || {
  printf 'play-playable0-local: Gamebloke module architecture does not include host %s\n' \
    "$host_arch" >&2
  exit 1
}

if ! command -v strings >/dev/null 2>&1 ||
   ! strings "$retroarch_bin" | grep -q -- '--config=FILE' ||
   ! strings "$retroarch_bin" | grep -q -- '--max-frames=NUMBER' ||
   ! strings "$retroarch_bin" | grep -q -- '--max-frames-ss-path=FILE' ||
   ! strings "$retroarch_bin" | grep -q -- '--log-file=FILE'; then
  printf '%s\n' 'play-playable0-local: installed RetroArch lacks the verified bounded CLI options' >&2
  exit 1
fi

bash "$verify_script"

mkdir -p "$retroarch_dir" "$retroarch_dir/saves" \
  "$retroarch_dir/states" "$retroarch_dir/system" \
  "$retroarch_dir/history" "$retroarch_dir/screenshots" \
  "$retroarch_dir/remaps" "$retroarch_dir/cache"
config="$retroarch_dir/retroarch.cfg"
log_file="$retroarch_dir/retroarch.log"
screenshot="$retroarch_dir/smoke.png"
launch_error="$retroarch_dir/launch-error.log"

cat > "$config" <<EOF
config_save_on_exit = "false"
audio_enable = "false"
audio_sync = "false"
video_vsync = "true"
log_verbosity = "true"
rewind_enable = "false"
run_ahead_enabled = "false"
cheats_enable = "false"
savestate_auto_save = "false"
savestate_auto_load = "false"
autosave_interval = "0"
history_list_enable = "false"
input_player1_a = "x"
input_player1_right = "right"
input_menu_toggle = "f1"
savefile_directory = "$retroarch_dir/saves"
savestate_directory = "$retroarch_dir/states"
system_directory = "$retroarch_dir/system"
content_history_dir = "$retroarch_dir/history"
screenshot_directory = "$retroarch_dir/screenshots"
input_remapping_directory = "$retroarch_dir/remaps"
cache_directory = "$retroarch_dir/cache"
EOF

app_bundle=
case $retroarch_bin in
  */Contents/MacOS/RetroArch) app_bundle=${retroarch_bin%/Contents/MacOS/RetroArch} ;;
esac

print_controls() {
  printf '%s\n' 'Gamebloke is running the local original BIOS-free demo through its Gamebloke core.'
  printf '%s\n' 'Controls: Right Arrow moves the marker; X presses the GBA A button; release the key to release it.'
  printf '%s\n' 'Reset: open RetroArch Quick Menu (F1), then choose Restart.'
  printf '%s\n' 'Exit: use RetroArch menu > Quit RetroArch.'
  printf '%s\n' 'The demo has mode-3 video only; audio and save devices are not implemented.'
}

if [ "$mode" = check ]; then
  printf 'play-playable0-local: checks passed\nRetroArch: %s\nCore: %s\nGuest: %s\nConfig: %s\n' \
    "$retroarch_bin" "$core" "$content" "$config"
  printf 'Launch arguments: --config=%s -L %s %s --verbose --log-file=%s\n' \
    "$config" "$core" "$content" "$log_file"
  print_controls
  exit 0
fi

if [ "$mode" = smoke ]; then
  rm -f "$log_file" "$screenshot" "$launch_error"
  [ -n "$app_bundle" ] || {
    printf '%s\n' 'play-playable0-local: --smoke requires RetroArch.app; set RETROARCH_BIN to its Contents/MacOS/RetroArch executable' >&2
    exit 1
  }
  if ! open -n -g -j -a "$retroarch_bin" --args \
    --config="$config" -L "$core" "$content" --verbose \
    --log-file="$log_file" --max-frames=120 --max-frames-ss \
    --max-frames-ss-path="$screenshot" > "$launch_error" 2>&1; then
    cat "$launch_error" >&2
    if grep -q 'kLSNoLaunchPermissionErr' "$launch_error"; then
      printf '%s\n' 'play-playable0-local: macOS managed-launch policy blocked RetroArch; owner must have the installed app permitted before smoke or G6 review can continue' >&2
    elif grep -q 'kLSNoExecutableErr' "$launch_error"; then
      printf '%s\n' 'play-playable0-local: LaunchServices could not resolve the RetroArch executable; verify the app bundle and RETROARCH_BIN path' >&2
    fi
    exit 1
  fi
  attempt=0
  while [ "$attempt" -lt 30 ]; do
    if [ -f "$log_file" ] && [ -f "$screenshot" ] &&
       grep -q 'Gamebloke local BIOS-free .gba content loaded' "$log_file"; then
      file "$screenshot"
      printf 'play-playable0-local: installed RetroArch smoke passed; log=%s screenshot=%s\n' \
        "$log_file" "$screenshot"
      exit 0
    fi
    sleep 1
    attempt=$((attempt + 1))
  done
  printf 'play-playable0-local: bounded RetroArch smoke did not produce verified content log and screenshot; inspect %s\n' \
    "$log_file" >&2
  if [ -f "$log_file" ]; then cat "$log_file" >&2; fi
  exit 1
fi

print_controls
if [ -n "$app_bundle" ]; then
  if ! open -n -a "$retroarch_bin" --args --config="$config" -L "$core" "$content" \
      --verbose --log-file="$log_file" > "$launch_error" 2>&1; then
    cat "$launch_error" >&2
    if grep -q 'kLSNoLaunchPermissionErr' "$launch_error"; then
      printf '%s\n' 'play-playable0-local: macOS managed-launch policy blocked RetroArch; the owner must make the installed app launchable before playing the demo' >&2
    fi
    exit 1
  fi
else
  "$retroarch_bin" --config="$config" -L "$core" "$content" \
    --verbose --log-file="$log_file"
fi
