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
default_dir=
mkdir -p "$fixture_dir"
: > "$log_file"
exec 3>&1 4>&2
exec > "$log_file" 2>&1
cleanup_local_verification() {
  local rc=$?
  if [[ -n "$default_dir" ]]; then
    rm -f "$default_dir"/* 2>/dev/null || true
    rmdir "$default_dir" 2>/dev/null || true
  fi
  if [[ "$rc" -ne 0 ]]; then cat "$log_file" >&4; fi
  exit "$rc"
}
trap cleanup_local_verification EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

case "$(uname -s)" in
  Darwin) core="$build_dir/gamebloke_libretro.dylib" ;;
  Linux) core="$build_dir/gamebloke_libretro.so" ;;
  *) printf 'verify-libretro-local: unsupported host %s\n' "$(uname -s)" >&2; exit 1 ;;
esac

printf '%s\n' 'verify-libretro-local: local-only original fixture path; no CI execution'
sh -n "$source_dir/scripts/play-playable0-terminal.sh"
if ! command -v python3 >/dev/null 2>&1; then
  printf '%s\n' 'verify-libretro-local: python3 is required for bounded PTY verification' >&2
  exit 1
fi
if [[ ! -f "$source_dir/tests/libretro_fixture_provenance_test.py" ]]; then
  printf '%s\n' 'verify-libretro-local: fixture provenance regression test is missing' >&2
  exit 1
fi
printf '%s\n' 'verify-libretro-local: checking local-only fixture provenance controls'
CMAKE="$cmake_bin" python3 "$source_dir/tests/libretro_fixture_provenance_test.py"
if [[ ! -f "$source_dir/tests/terminal_frontend_test.py" ]]; then
  printf '%s\n' 'verify-libretro-local: terminal PTY tracer is missing' >&2
  exit 1
fi
"$cmake_bin" -S "$source_dir" -B "$build_dir" \
  -DGBL_LANE=local-playable0 -DCMAKE_BUILD_TYPE=Release
"$cmake_bin" --build "$build_dir" --target \
  gamebloke_libretro libretro_frontend_test gamebloke-terminal gbl_instruction_image \
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

manifest_expected_digest() {
  python3 - "$source_dir/tests/fixtures/manifest.json" \
    "$source_dir/tests/fixtures/playable0-NOTICE.md" "$1" <<'PY'
import json
import re
import sys

manifest_path, notice_path, fixture_id = sys.argv[1:]
with open(manifest_path, encoding="utf-8") as source:
    fixtures = json.load(source)["fixtures"]
with open(notice_path, encoding="utf-8") as source:
    notice = source.read()

interactive_fixture_ids = {
    "owned-playable0-interactive",
    "owned-playable0-interactive-input-disabled",
    "owned-playable0-interactive-wrong-color",
}

def require_local_only(fixture):
    fixture_id = fixture.get("id", "<unknown>")
    if fixture.get("permitted_use") != ["local-only"]:
        raise SystemExit(
            "interactive fixture {} must remain permitted_use=['local-only']".format(
                fixture_id
            )
        )
    if fixture.get("license", {}).get("redistributable") != "unknown":
        raise SystemExit(
            "interactive fixture {} must keep license.redistributable='unknown'".format(
                fixture_id
            )
        )

if fixture_id == "generator":
    generator_digests = set()
    for fixture in fixtures:
        if fixture["id"] not in interactive_fixture_ids:
            continue
        require_local_only(fixture)
        match = re.search(
            r"tests/fixtures/instruction_image\.c SHA-256=([0-9a-f]{64})",
            fixture["build"]["recipe"],
        )
        if match is None:
            raise SystemExit("manifest is missing the interactive generator digest")
        generator_digests.add(match.group(1))
    if len(generator_digests) != 1:
        raise SystemExit("interactive generator digests disagree in the manifest")
    expected = generator_digests.pop()
else:
    matches = [fixture for fixture in fixtures if fixture["id"] == fixture_id]
    if len(matches) != 1:
        raise SystemExit("manifest must contain exactly one {} fixture".format(fixture_id))
    artifact = matches[0]["build"]["artifact"]
    expected = artifact["sha256"]
    if fixture_id in interactive_fixture_ids:
        require_local_only(matches[0])
        artifact_name = fixture_id + ".gba"
        expected_path = "generated/fixtures/" + artifact_name
        artifact_path = artifact.get("path")
        if artifact_path != expected_path:
            raise SystemExit(
                "manifest artifact path for {} must be {}, got {!r}".format(
                    fixture_id, expected_path, artifact_path
                )
            )

        notice_rows = {}
        for line_number, line in enumerate(notice.splitlines(), 1):
            row = re.fullmatch(
                r"\s*\|\s*`([^`]+)`\s*\|\s*`([0-9a-f]{64})`\s*\|\s*",
                line,
            )
            if row is not None:
                notice_rows.setdefault(row.group(1), []).append(
                    (row.group(2), line_number)
                )

        rows = notice_rows.get(artifact_name, [])
        if len(rows) != 1:
            raise SystemExit(
                "fixture notice must contain exactly one digest row for {} "
                "(found {})".format(artifact_name, len(rows))
            )
        notice_digest, line_number = rows[0]
        if notice_digest != expected:
            raise SystemExit(
                "fixture notice digest mismatch for {} at line {}: "
                "notice={}, manifest={}".format(
                    artifact_name, line_number, notice_digest, expected
                )
            )

if re.fullmatch(r"[0-9a-f]{64}", expected) is None:
    raise SystemExit("manifest has an invalid SHA-256 for {}".format(fixture_id))
if fixture_id not in interactive_fixture_ids and chr(96) + expected + chr(96) not in notice:
    raise SystemExit("fixture notice does not record the manifest SHA-256 for {}".format(fixture_id))
print(expected)
PY
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

interactive_generator_expected="$(manifest_expected_digest generator)"
check_digest "$source_dir/tests/fixtures/instruction_image.c" \
  "$interactive_generator_expected"
for name in owned-playable0-interactive.gba \
    owned-playable0-interactive-input-disabled.gba \
    owned-playable0-interactive-wrong-color.gba; do
  check_digest "$fixture_dir/$name" \
    "$(manifest_expected_digest "${name%.gba}")"
done

if ! git -C "$source_dir" check-ignore -q \
    "$fixture_dir/owned-playable0-interactive.gba"; then
  printf '%s\n' 'verify-libretro-local: generated guest is not ignored' >&2
  exit 1
fi

"$build_dir/gamebloke-terminal" --check "$core" \
  "$fixture_dir/owned-playable0-interactive.gba"
python3 "$source_dir/tests/terminal_frontend_test.py" tracer \
  "$build_dir/gamebloke-terminal" "$core" \
  "$fixture_dir/owned-playable0-interactive.gba"

"$build_dir/libretro_frontend_test" tracer "$core" \
  "$fixture_dir/owned-playable0.gba"
"$build_dir/libretro_frontend_test" live "$core" \
  "$fixture_dir/owned-playable0-interactive.gba"
"$build_dir/libretro_frontend_test" negative-input "$core" \
  "$fixture_dir/owned-playable0-interactive-input-disabled.gba"
"$build_dir/libretro_frontend_test" negative-color "$core" \
  "$fixture_dir/owned-playable0-interactive-wrong-color.gba"

default_dir="$(mktemp -d "$build_dir/default-generator.XXXXXX")"
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
