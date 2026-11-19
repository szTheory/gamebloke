#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${CI:-}" ]]; then
  printf '%s\n' 'verify-libretro-local: refused because CI is set' >&2
  exit 2
fi

source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
build_dir="$source_dir/build/libretro-local"
fixture_dir="$build_dir/generated/fixtures"
log_file="$build_dir/verify-libretro-local.log"
cmake_bin="${CMAKE:-cmake}"
mkdir -p "$fixture_dir"
: > "$log_file"
exec 3>&1 4>&2
exec > "$log_file" 2>&1
trap 'rc=$?; if [ "$rc" -ne 0 ]; then cat "$log_file" >&4; fi; exit "$rc"' 0

case "$(uname -s)" in
  Darwin) core="$build_dir/gamebloke_libretro.dylib" ;;
  Linux) core="$build_dir/gamebloke_libretro.so" ;;
  *) printf 'verify-libretro-local: unsupported host %s\n' "$(uname -s)" >&2; exit 1 ;;
esac

printf '%s\n' 'verify-libretro-local: local-only original fixture path; no CI execution'
"$cmake_bin" -S "$source_dir" -B "$build_dir" \
  -DGBL_LANE=local-playable0 -DCMAKE_BUILD_TYPE=Release
"$cmake_bin" --build "$build_dir" --target \
  gamebloke_libretro libretro_frontend_test gbl_instruction_image \
  gbl_owned_instruction_images

"$build_dir/gbl_instruction_image" "$fixture_dir" \
  --include-local-playable0 --include-local-playable0-interactive

check_digest() {
  local file="$1" expected="$2" actual
  actual=$("$cmake_bin" -E sha256sum "$file" | awk '{print $1}')
  if [[ "$actual" != "$expected" ]]; then
    printf 'verify-libretro-local: digest mismatch for %s: %s\n' "$file" "$actual" >&2
    return 1
  fi
}

# The legacy bounded guest and its negative controls are immutable inputs here.
check_digest "$fixture_dir/owned-playable0.gba" \
  9114432ee8b722080709c7171fde5fe56f2300a7deaf0c77588f85defc5ee899
check_digest "$fixture_dir/owned-playable0-input-disabled.gba" \
  e1012bd229cd18f0657215bed761ba9c7d0c672310d1a88cef895a61688710a6
check_digest "$fixture_dir/owned-playable0-wrong-color.gba" \
  d6693d2f5ce431791f8264108aec9daea35fe2ae8f95fd0f9077224ec576c4e8
check_digest "$fixture_dir/owned-playable0-wrong-address.gba" \
  e8b63e17f22a9b27cc7d9efbf42d8708c58826db9bb1acc5aaa450564dc18086

if ! git -C "$source_dir" check-ignore -q \
    "$fixture_dir/owned-playable0-interactive.gba"; then
  printf '%s\n' 'verify-libretro-local: generated guest is not ignored' >&2
  exit 1
fi

"$build_dir/libretro_frontend_test" tracer "$core" \
  "$fixture_dir/owned-playable0.gba"
"$build_dir/libretro_frontend_test" live "$core" \
  "$fixture_dir/owned-playable0-interactive.gba"
"$build_dir/libretro_frontend_test" negative-input "$core" \
  "$fixture_dir/owned-playable0-interactive-input-disabled.gba"
"$build_dir/libretro_frontend_test" negative-color "$core" \
  "$fixture_dir/owned-playable0-interactive-wrong-color.gba"

default_dir="$build_dir/default-generator"
mkdir -p "$default_dir"
"$build_dir/gbl_instruction_image" "$default_dir"
if [[ -e "$default_dir/owned-playable0-interactive.gba" ]]; then
  printf '%s\n' 'verify-libretro-local: default generator emitted local interactive guest' >&2
  exit 1
fi

printf 'verify-libretro-local: generator_sha256=%s\n' \
  "$("$cmake_bin" -E sha256sum "$source_dir/tests/fixtures/instruction_image.c" | awk '{print $1}')"
for name in owned-playable0-interactive.gba \
    owned-playable0-interactive-input-disabled.gba \
    owned-playable0-interactive-wrong-color.gba; do
  printf 'verify-libretro-local: artifact=%s sha256=%s\n' "$name" \
    "$("$cmake_bin" -E sha256sum "$fixture_dir/$name" | awk '{print $1}')"
done
printf 'verify-libretro-local: log=%s\n' "$log_file"
cat "$log_file" >&3
