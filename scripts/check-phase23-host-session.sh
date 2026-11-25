#!/bin/sh
set -eu

# This local-only diagnostic refuses even an empty CI assignment before path
# resolution, output creation, package configuration, or fixture generation.
if [ "${CI+x}" = x ]; then
    printf '%s\n' 'phase23: refused because CI is set' >&2
    exit 2
fi

MODE=run
case "${1:-}" in
    --preflight) MODE=preflight; shift ;;
    --negative-controls) MODE=negative-controls; shift ;;
    "") ;;
    *) printf '%s\n' 'usage: sh scripts/check-phase23-host-session.sh [--preflight|--negative-controls]' >&2; exit 2 ;;
esac
NEGATIVE_ONLY=0
[ "$MODE" = negative-controls ] && NEGATIVE_ONLY=1
[ "$#" -eq 0 ] || {
    printf '%s\n' 'usage: sh scripts/check-phase23-host-session.sh [--preflight|--negative-controls]' >&2
    exit 2
}

SCRIPT_DIR=$(CDPATH= cd -P "$(dirname "$0")" && pwd -P)
REPO_ROOT=$(CDPATH= cd -P "$SCRIPT_DIR/.." && pwd -P)
PHASE_ROOT="$REPO_ROOT/build/phase23-local"
NOTICE="$REPO_ROOT/tests/fixtures/phase23-host-session-NOTICE.md"
SOURCE="$REPO_ROOT/tests/fixtures/phase23_host_session.c"
EXAMPLE="$REPO_ROOT/examples/phase23-host-session"
START_ID=$(date -u '+%Y%m%dT%H%M%SZ')-$$
RUN_ROOT="$PHASE_ROOT/run-$START_ID"
EXT_ROOT=
EXT_PHYSICAL=
NEG_ROOT=
CC_REQUEST=${CC:-cc}
CC_PATH=

fail()
{
    printf 'phase23: %s\n' "$*" >&2
    exit 1
}

sha256_file()
{
    digest_line=$(shasum -a 256 "$1") || fail "sha256 failed for $1"
    digest=$(printf '%s\n' "$digest_line" | awk 'NR == 1 { print $1 }')
    [ "${#digest}" -eq 64 ] || fail "malformed SHA-256 for $1"
    case "$digest" in *[!0123456789abcdef]*) fail "malformed SHA-256 for $1" ;; esac
    printf '%s\n' "$digest"
}

authority_check()
{
    authority_file=$1
    [ -f "$authority_file" ] && [ -s "$authority_file" ] &&
        [ ! -L "$authority_file" ] || {
        printf 'phase23: authority: notice unavailable or empty: %s\n' "$authority_file" >&2
        return 1
    }
    awk '
        BEGIN {
            expected["fixture_id"] = "phase23-host-session"
            expected["permitted_use"] = "local-only"
            expected["permitted_environment"] = "local-phase23-verification"
            expected["license_status"] = "no-redistribution-license-granted"
            expected["public_ci"] = "false"
            expected["upload"] = "false"
            expected["package"] = "false"
            expected["redistribution"] = "false"
            expected["profile"] = "agb"
            expected["profile_version"] = "16"
            expected["start_mode"] = "direct"
            expected["bios"] = "none"
            expected["cartridge"] = "rom-only"
        }
        $0 == "<!-- PHASE23_AUTHORITY_START -->" {
            starts++
            if (inside) { print "phase23: authority: duplicate authority block"; bad = 1 }
            inside = 1
            next
        }
        $0 == "<!-- PHASE23_AUTHORITY_END -->" {
            ends++
            if (!inside) { print "phase23: authority: closing block without opening block"; bad = 1 }
            inside = 0
            next
        }
        inside {
            if ($0 == "") {
                print "phase23: authority: empty metadata row"
                bad = 1
                next
            }
            separator = index($0, "=")
            if (separator < 2) {
                print "phase23: authority: malformed metadata line"
                bad = 1
                next
            }
            key = substr($0, 1, separator - 1)
            value = substr($0, separator + 1)
            if (!(key in expected)) {
                print "phase23: authority: unexpected field " key
                bad = 1
                next
            }
            if (seen[key]++) {
                print "phase23: authority: duplicate field " key
                bad = 1
                next
            }
            field_count++
            if (value != expected[key]) {
                print "phase23: authority: " key " expected " expected[key] ", got " value
                bad = 1
            }
        }
        END {
            if (starts != 1 || ends != 1 || inside) {
                print "phase23: authority: exactly one delimited metadata block is required"
                bad = 1
            }
            for (key in expected) {
                if (!seen[key]) {
                    print "phase23: authority: missing field " key
                    bad = 1
                }
            }
            if (field_count != 13) {
                print "phase23: authority: exactly 13 approved fields are required"
                bad = 1
            }
            if (bad) {
                exit 1
            }
        }
    ' "$authority_file"
}

check_local_paths()
{
    for candidate in "$REPO_ROOT/build" "$PHASE_ROOT"; do
        [ ! -L "$candidate" ] || fail "refusing symlinked output path: $candidate"
    done
    git -C "$REPO_ROOT" check-ignore -q "$PHASE_ROOT/output-probe" ||
        fail 'build/phase23-local is not ignored'
    if [ -e "$RUN_ROOT" ] || [ -L "$RUN_ROOT" ]; then
        fail "unique invocation path already exists: $RUN_ROOT"
    fi
    if git -C "$REPO_ROOT" ls-files --error-unmatch -- "$RUN_ROOT" >/dev/null 2>&1; then
        fail 'refusing a tracked invocation path'
    fi
}

check_tools()
{
    for tool in cmake ninja shasum awk sed grep git; do
        command -v "$tool" >/dev/null 2>&1 || fail "required local tool unavailable: $tool"
    done
    case "$CC_REQUEST" in
        */*) CC_PATH=$CC_REQUEST ;;
        *) CC_PATH=$(command -v "$CC_REQUEST" || true) ;;
    esac
    [ -n "$CC_PATH" ] && [ -x "$CC_PATH" ] || fail "C compiler unavailable: $CC_REQUEST"
    cmake --version | awk 'NR == 1 { split($3, version, "."); if (version[1] + 0 < 3 || (version[1] + 0 == 3 && version[2] + 0 < 25)) exit 1 }' ||
        fail 'CMake 3.25 or newer is required'
}

preflight()
{
    [ -f "$SOURCE" ] && [ -s "$SOURCE" ] && [ ! -L "$SOURCE" ] ||
        fail 'required Phase 23 generator source is missing, empty, or symlinked'
    [ -f "$NOTICE" ] && [ -s "$NOTICE" ] && [ ! -L "$NOTICE" ] ||
        fail 'required Phase 23 notice is missing, empty, or symlinked'
    authority_check "$NOTICE" || fail 'Phase 23 local authority is not the approved tuple'
    check_tools
    check_local_paths
}

write_fixture_record()
{
    source_digest=$1
    notice_digest=$2
    image_digest=$3
    image_length=$4
    record_file=$5
    revision=$(git -C "$REPO_ROOT" rev-parse HEAD)
    dirty_state=$(if [ -n "$(git -C "$REPO_ROOT" status --porcelain --untracked-files=normal)" ]; then printf dirty; else printf clean; fi)
    compiler_version=$("$CC_PATH" --version 2>&1 | sed -n '1p' | sed 's/\\/\\\\/g; s/"/\\"/g')
    recipe="cc -std=c17 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror"
    printf '{\n  "schema": "gamebloke.phase23-fixture-record/1",\n  "authority": {\n' > "$record_file"
    printf '    "fixture_id": "phase23-host-session",\n    "permitted_use": "local-only",\n    "permitted_environment": "local-phase23-verification",\n    "license_status": "no-redistribution-license-granted",\n    "public_ci": "false",\n    "upload": "false",\n    "package": "false",\n    "redistribution": "false",\n    "profile": "agb",\n    "profile_version": "16",\n    "start_mode": "direct",\n    "bios": "none",\n    "cartridge": "rom-only"\n  },\n' >> "$record_file"
    printf '  "source_path": "tests/fixtures/phase23_host_session.c",\n  "source_sha256": "%s",\n' "$source_digest" >> "$record_file"
    printf '  "notice_path": "tests/fixtures/phase23-host-session-NOTICE.md",\n  "notice_sha256": "%s",\n' "$notice_digest" >> "$record_file"
    printf '  "image_path": "phase23-host-session.gba",\n  "image_sha256": "%s",\n  "image_bytes": %s,\n' "$image_digest" "$image_length" >> "$record_file"
    printf '  "source_revision": "%s",\n  "source_tree_state": "%s",\n' "$revision" "$dirty_state" >> "$record_file"
    printf '  "compiler_identity": "%s",\n  "generation_recipe": "%s",\n' "$compiler_version" "$recipe" >> "$record_file"
    printf '  "expected_guest_behavior": "mailbox PASS 0x23010001; line 6/14/24 KEYINPUT 0x03FE/0x03DF/0x03FF; first frame row 0 black and rows 1-159 BGR555LE 0x56AA; pre-PWM audio left -9472 right 0"\n}\n' >> "$record_file"
}

validate_record()
{
    record_file=$1
    image_file=$2
    staged_source=$3
    notice_file=${4:-$NOTICE}
    [ -f "$record_file" ] && [ -s "$record_file" ] && [ ! -L "$record_file" ] ||
        fail 'fixture-record: missing, empty, or symlinked record'
    authority_check "$notice_file" || fail 'fixture-record: notice authority changed before consumer launch'
    expected_source=$(sha256_file "$staged_source")
    expected_notice=$(sha256_file "$notice_file")
    expected_image=$(sha256_file "$image_file")
    [ "$expected_source" = "$SOURCE_DIGEST" ] ||
        fail 'fixture-record: source digest differs from staged captured digest'
    [ "$expected_notice" = "$NOTICE_DIGEST" ] ||
        fail 'fixture-record: notice digest differs from staged captured digest'
    [ "$expected_image" = "$IMAGE_DIGEST" ] ||
        fail 'fixture-record: image digest differs from staged captured digest'
    awk '
        BEGIN {
            expected["fixture_id"] = "phase23-host-session"
            expected["permitted_use"] = "local-only"
            expected["permitted_environment"] = "local-phase23-verification"
            expected["license_status"] = "no-redistribution-license-granted"
            expected["public_ci"] = "false"
            expected["upload"] = "false"
            expected["package"] = "false"
            expected["redistribution"] = "false"
            expected["profile"] = "agb"
            expected["profile_version"] = "16"
            expected["start_mode"] = "direct"
            expected["bios"] = "none"
            expected["cartridge"] = "rom-only"
        }
        /"authority": \{/ {
            if (inside) { print "phase23: fixture-record: duplicate authority object"; bad = 1 }
            inside = 1
            starts++
            next
        }
        inside && /^[[:space:]]*\}/ {
            inside = 0
            ends++
            next
        }
        inside {
            if (split($0, part, "\"") < 5 || part[2] == "") {
                print "phase23: fixture-record: malformed authority row"
                bad = 1
                next
            }
            key = part[2]
            value = part[4]
            if (!(key in expected)) {
                print "phase23: fixture-record: unexpected authority field " key
                bad = 1
                next
            }
            if (seen[key]++) {
                print "phase23: fixture-record: duplicate authority field " key
                bad = 1
                next
            }
            fields++
            if (value != expected[key]) {
                print "phase23: fixture-record: authority " key " mismatch"
                bad = 1
            }
        }
        END {
            if (starts != 1 || ends != 1 || inside) {
                print "phase23: fixture-record: one complete authority object is required"
                bad = 1
            }
            for (key in expected) {
                if (!seen[key]) {
                    print "phase23: fixture-record: missing authority field " key
                    bad = 1
                }
            }
            if (fields != 13) {
                print "phase23: fixture-record: exactly 13 authority fields are required"
                bad = 1
            }
            if (bad) exit 1
        }
    ' "$record_file" || fail 'fixture-record: authority object mismatch'
    [ "$(grep -Fxc '  "source_path": "tests/fixtures/phase23_host_session.c",' "$record_file" || true)" -eq 1 ] ||
        fail 'fixture-record: source path mismatch'
    [ "$(grep -Fxc '  "notice_path": "tests/fixtures/phase23-host-session-NOTICE.md",' "$record_file" || true)" -eq 1 ] ||
        fail 'fixture-record: notice path mismatch'
    [ "$(grep -Fxc '  "image_path": "phase23-host-session.gba",' "$record_file" || true)" -eq 1 ] ||
        fail 'fixture-record: image path mismatch'
    expected_length=$(wc -c < "$image_file" | tr -d ' ')
    [ "$(grep -Fxc "  \"image_bytes\": $expected_length," "$record_file" || true)" -eq 1 ] ||
        fail 'fixture-record: image_bytes mismatch'
    [ "$(grep -Fxc "  \"source_sha256\": \"$expected_source\"," "$record_file" || true)" -eq 1 ] ||
        fail 'fixture-record: source_sha256 mismatch'
    [ "$(grep -Fxc "  \"notice_sha256\": \"$expected_notice\"," "$record_file" || true)" -eq 1 ] ||
        fail 'fixture-record: notice_sha256 mismatch'
    [ "$(grep -Fxc "  \"image_sha256\": \"$expected_image\"," "$record_file" || true)" -eq 1 ] ||
        fail 'fixture-record: image_sha256 mismatch'
}

run_consumer()
{
    run_record=$1
    run_image=$2
    run_source=$3
    run_output=$4
    shift 4
    validate_record "$run_record" "$run_image" "$run_source"
    (cd "$EXT_ROOT/work" &&
        "$EXT_ROOT/build/gamebloke-phase23-host-session" "$run_image" \
            "$run_output" "$@")
}

cleanup()
{
    if [ -n "$EXT_ROOT" ] && [ -n "$EXT_PHYSICAL" ] && [ -d "$EXT_ROOT" ]; then
        ext_now=$(CDPATH= cd -P "$EXT_ROOT" 2>/dev/null && pwd -P || true)
        if [ "$ext_now" = "$EXT_PHYSICAL" ]; then
            case "$ext_now" in
                "$REPO_ROOT"|"$REPO_ROOT"/*) ;;
                *) rm -rf "$EXT_ROOT" ;;
            esac
        fi
    fi
    if [ -n "$NEG_ROOT" ] && [ -d "$NEG_ROOT" ] &&
       [ -f "$NEG_ROOT/.phase23-owner" ] &&
       [ "$(cat "$NEG_ROOT/.phase23-owner")" = "$START_ID" ] &&
       [ "$(CDPATH= cd -P "$NEG_ROOT" 2>/dev/null && pwd -P || true)" = "$NEG_ROOT" ]; then
        case "$NEG_ROOT" in
            "$PHASE_ROOT"/authority-controls-*) rm -rf "$NEG_ROOT" ;;
        esac
    fi
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

preflight
if [ "$MODE" = preflight ]; then
    printf '%s\n' 'phase23: local preflight verified'
    exit 0
fi
if [ "$MODE" = negative-controls ]; then
    mkdir -p "$PHASE_ROOT"
    NEG_ROOT="$PHASE_ROOT/authority-controls-$START_ID"
    [ ! -e "$NEG_ROOT" ] && [ ! -L "$NEG_ROOT" ] || fail 'authority control path already exists'
    mkdir -m 700 "$NEG_ROOT"
    printf '%s' "$START_ID" > "$NEG_ROOT/.phase23-owner"
    sed '/<!-- PHASE23_AUTHORITY_START -->/,/<!-- PHASE23_AUTHORITY_END -->/d' \
        "$NOTICE" > "$NEG_ROOT/missing-block.md"
    : > "$NEG_ROOT/empty.md"
    sed 's/^public_ci=false$/public_ci=true/' "$NOTICE" > "$NEG_ROOT/broadened-ci.md"
    sed 's/^fixture_id=phase23-host-session$/fixture_id=other-fixture/' \
        "$NOTICE" > "$NEG_ROOT/wrong-id.md"
    for control in \
        'missing-block.md|missing field fixture_id' \
        'empty.md|notice unavailable or empty' \
        'broadened-ci.md|public_ci expected false, got true' \
        'wrong-id.md|fixture_id expected phase23-host-session, got other-fixture'; do
        name=${control%%|*}
        expected=${control#*|}
        if authority_check "$NEG_ROOT/$name" > "$NEG_ROOT/$name.log" 2>&1; then
            fail "authority negative control passed unexpectedly: $name"
        fi
        grep -F "$expected" "$NEG_ROOT/$name.log" >/dev/null || {
            cat "$NEG_ROOT/$name.log" >&2
            fail "authority negative control lacked its named diagnostic: $name"
        }
    done
    [ ! -e "$RUN_ROOT" ] || fail 'authority control created a package or guest run before rejection'
    [ -z "$(find "$NEG_ROOT" -type f -name '*.gba' -print)" ] ||
        fail 'authority control created a fixture image'
    printf '%s\n' 'phase23: authority negative controls verified'
    before_runs=$(find "$PHASE_ROOT" -maxdepth 1 -type d -name 'run-*' -print | sort)
    for ci_case in set empty; do
        if [ "$ci_case" = set ]; then
            if CI=1 sh "$SCRIPT_DIR/check-phase23-host-session.sh" --preflight \
                > "$NEG_ROOT/ci-set.log" 2>&1; then
                ci_status=0
            else
                ci_status=$?
            fi
            ci_log="$NEG_ROOT/ci-set.log"
        else
            if CI= sh "$SCRIPT_DIR/check-phase23-host-session.sh" --preflight \
                > "$NEG_ROOT/ci-empty.log" 2>&1; then
                ci_status=0
            else
                ci_status=$?
            fi
            ci_log="$NEG_ROOT/ci-empty.log"
        fi
        [ "$ci_status" -eq 2 ] || fail "CI=$ci_case refusal did not exit 2"
        grep -Fx 'phase23: refused because CI is set' "$ci_log" >/dev/null ||
            fail "CI=$ci_case refusal omitted its diagnostic"
    done
    after_runs=$(find "$PHASE_ROOT" -maxdepth 1 -type d -name 'run-*' -print | sort)
    [ "$before_runs" = "$after_runs" ] || fail 'CI refusal created a run or scratch path'
fi

umask 077
mkdir -p "$PHASE_ROOT"
phase_physical=$(CDPATH= cd -P "$PHASE_ROOT" && pwd -P)
[ "$phase_physical" = "$PHASE_ROOT" ] || fail 'Phase 23 output resolved outside build/phase23-local'
mkdir -m 700 "$RUN_ROOT"
mkdir -m 700 "$RUN_ROOT/fixture" "$RUN_ROOT/package-build" "$RUN_ROOT/sessions"
git -C "$REPO_ROOT" check-ignore -q "$RUN_ROOT/fixture/phase23-host-session.gba" ||
    fail 'generated Phase 23 fixture is not ignored'
IMAGE="$RUN_ROOT/fixture/phase23-host-session.gba"
STAGED_SOURCE="$RUN_ROOT/fixture/phase23_host_session.c"
GENERATOR="$RUN_ROOT/fixture/phase23-image-generator"
RECORD="$RUN_ROOT/fixture-record.json"
cp "$SOURCE" "$STAGED_SOURCE"
cmp -s "$SOURCE" "$STAGED_SOURCE" || fail 'staged generator source differs from checked source'
SOURCE_DIGEST=$(sha256_file "$STAGED_SOURCE")
NOTICE_DIGEST=$(sha256_file "$NOTICE")
OUTPUT_DEFINE="-DGBL_PHASE23_OUTPUT_PATH=\"$IMAGE\""
"$CC_PATH" -std=c17 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
    -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wcast-qual -Werror \
    "$OUTPUT_DEFINE" "$STAGED_SOURCE" -o "$GENERATOR" || fail 'host generator compile failed'
"$GENERATOR" "$IMAGE" || fail 'bounded local fixture generation failed'
[ -f "$IMAGE" ] && [ ! -L "$IMAGE" ] || fail 'generator did not create its bound image'
IMAGE_LENGTH=$(wc -c < "$IMAGE" | tr -d ' ')
case "$IMAGE_LENGTH" in *[!0-9]*|'') fail 'generated image length is not numeric' ;; esac
[ "$IMAGE_LENGTH" -ge 192 ] && [ "$IMAGE_LENGTH" -le 33554432 ] || fail 'generated image is outside bounded ROM size'
[ $((IMAGE_LENGTH % 2)) -eq 0 ] || fail 'generated image length is not even'
IMAGE_DIGEST=$(sha256_file "$IMAGE")
write_fixture_record "$SOURCE_DIGEST" "$NOTICE_DIGEST" "$IMAGE_DIGEST" \
    "$IMAGE_LENGTH" "$RECORD"
validate_record "$RECORD" "$IMAGE" "$STAGED_SOURCE"

if [ "$MODE" = negative-controls ]; then
    NEGATIVE_ROOT="$RUN_ROOT/negative-controls"
    mkdir -m 700 "$NEGATIVE_ROOT" "$NEGATIVE_ROOT/provenance" \
        "$NEGATIVE_ROOT/missing-package"
    mkdir -m 700 "$NEGATIVE_ROOT/missing-package/source" \
        "$NEGATIVE_ROOT/missing-package/build"
    sed 's/"permitted_environment": "local-phase23-verification"/"permitted_environment": "local-phase23-verification,public-ci"/' \
        "$RECORD" > "$NEGATIVE_ROOT/provenance/broadened-environment.json"
    printf '%s\n' \
        'source_sha256' 'notice_sha256' 'image_sha256' > \
        "$NEGATIVE_ROOT/provenance/digests.txt"
    for digest_field in source_sha256 notice_sha256 image_sha256; do
        awk -v field="$digest_field" '
            index($0, "\"" field "\":") {
                sub(/[\"]([0-9a-f]+)[\"]/, "\"0000000000000000000000000000000000000000000000000000000000000000\"")
            }
            { print }
        ' "$RECORD" > "$NEGATIVE_ROOT/provenance/bad-$digest_field.json"
    done
    for control in \
        'broadened-environment.json|fixture-record: authority permitted_environment mismatch' \
        'bad-source_sha256.json|fixture-record: source_sha256 mismatch' \
        'bad-notice_sha256.json|fixture-record: notice_sha256 mismatch' \
        'bad-image_sha256.json|fixture-record: image_sha256 mismatch'; do
        name=${control%%|*}
        expected=${control#*|}
        if (validate_record "$NEGATIVE_ROOT/provenance/$name" "$IMAGE" \
            "$STAGED_SOURCE") > "$NEGATIVE_ROOT/provenance/$name.log" 2>&1; then
            fail "record negative control passed unexpectedly: $name"
        fi
        grep -F "$expected" "$NEGATIVE_ROOT/provenance/$name.log" >/dev/null || {
            cat "$NEGATIVE_ROOT/provenance/$name.log" >&2
            fail "record negative control lacked its named diagnostic: $name"
        }
    done
    mkdir -m 700 "$NEGATIVE_ROOT/provenance/changed-inputs"
    cp "$STAGED_SOURCE" "$NEGATIVE_ROOT/provenance/changed-inputs/source.c"
    cp "$NOTICE" "$NEGATIVE_ROOT/provenance/changed-inputs/notice.md"
    cp "$IMAGE" "$NEGATIVE_ROOT/provenance/changed-inputs/image.gba"
    printf '\n/* changed after provenance capture */\n' >> \
        "$NEGATIVE_ROOT/provenance/changed-inputs/source.c"
    printf '\nChanged after provenance capture.\n' >> \
        "$NEGATIVE_ROOT/provenance/changed-inputs/notice.md"
    printf '\001' >> "$NEGATIVE_ROOT/provenance/changed-inputs/image.gba"
    for captured_input in source notice image; do
        control_source=$STAGED_SOURCE
        control_notice=$NOTICE
        control_image=$IMAGE
        case "$captured_input" in
            source)
                control_source="$NEGATIVE_ROOT/provenance/changed-inputs/source.c"
                expected_diagnostic='fixture-record: source digest differs from staged captured digest'
                ;;
            notice)
                control_notice="$NEGATIVE_ROOT/provenance/changed-inputs/notice.md"
                expected_diagnostic='fixture-record: notice digest differs from staged captured digest'
                ;;
            image)
                control_image="$NEGATIVE_ROOT/provenance/changed-inputs/image.gba"
                expected_diagnostic='fixture-record: image digest differs from staged captured digest'
                ;;
        esac
        control_record="$NEGATIVE_ROOT/provenance/redefined-$captured_input.json"
        write_fixture_record \
            "$(sha256_file "$control_source")" \
            "$(sha256_file "$control_notice")" \
            "$(sha256_file "$control_image")" \
            "$(wc -c < "$control_image" | tr -d ' ')" \
            "$control_record"
        if (validate_record "$control_record" "$control_image" \
            "$control_source" "$control_notice") \
            > "$NEGATIVE_ROOT/provenance/redefined-$captured_input.log" 2>&1; then
            fail "captured-input negative control passed unexpectedly: $captured_input"
        fi
        grep -F "$expected_diagnostic" \
            "$NEGATIVE_ROOT/provenance/redefined-$captured_input.log" >/dev/null || {
            cat "$NEGATIVE_ROOT/provenance/redefined-$captured_input.log" >&2
            fail "captured-input negative control lacked its named diagnostic: $captured_input"
        }
    done
    [ ! -e "$RUN_ROOT/session-receipt.json" ] ||
        fail 'provenance refusal published an accepted receipt'
    cp "$EXAMPLE/CMakeLists.txt" "$EXAMPLE/consumer.c" \
        "$NEGATIVE_ROOT/missing-package/source/"
    if (
        unset CMAKE_PREFIX_PATH CMAKE_MODULE_PATH gamebloke_DIR
        cmake -S "$NEGATIVE_ROOT/missing-package/source" \
            -B "$NEGATIVE_ROOT/missing-package/build" -G Ninja \
            -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" \
            -DCMAKE_C_COMPILER="$CC_PATH" \
            -DCMAKE_PREFIX_PATH= \
            -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF \
            -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF \
            -DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON \
            -DCMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH=OFF \
            -DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH=OFF
    ) > "$NEGATIVE_ROOT/missing-package/configure.log" 2>&1; then
        fail 'missing-package negative control configured unexpectedly'
    fi
    grep -F 'Could not find a package configuration file provided by "gamebloke"' \
        "$NEGATIVE_ROOT/missing-package/configure.log" >/dev/null || {
        cat "$NEGATIVE_ROOT/missing-package/configure.log" >&2
        fail 'missing-package control did not report unresolved gameblokeConfig.cmake'
    }
    [ ! -e "$RUN_ROOT/session-receipt.json" ] ||
        fail 'missing-package refusal published an accepted receipt'
fi

cmake -S "$REPO_ROOT" -B "$RUN_ROOT/package-build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
    -DGBL_LANE=ci > "$RUN_ROOT/configure.log" 2>&1 || {
        cat "$RUN_ROOT/configure.log" >&2
        fail 'Release package configure failed'
    }
cmake --build "$RUN_ROOT/package-build" --target gamebloke_core \
    > "$RUN_ROOT/build.log" 2>&1 || {
        cat "$RUN_ROOT/build.log" >&2
        fail 'static API package build failed'
    }
INITIAL_PREFIX="$RUN_ROOT/install-before-move"
MOVED_PREFIX="$RUN_ROOT/relocated-prefix"
cmake --install "$RUN_ROOT/package-build" --prefix "$INITIAL_PREFIX" \
    --component GameblokeCoreApi > "$RUN_ROOT/install.log" 2>&1 || {
        cat "$RUN_ROOT/install.log" >&2
        fail 'static API package install failed'
    }
[ -f "$INITIAL_PREFIX/lib/cmake/gamebloke/gameblokeConfig.cmake" ] || fail 'installed CMake package config is missing'
[ -f "$INITIAL_PREFIX/lib/cmake/gamebloke/gameblokeTargets.cmake" ] || fail 'installed CMake target export is missing'
[ -f "$INITIAL_PREFIX/include/gamebloke/gamebloke_v1.h" ] || fail 'installed public v1 header is missing'
if [ "$(find "$INITIAL_PREFIX" -type f -name 'gamebloke_v1.h' | wc -l | tr -d ' ')" -ne 1 ]; then
    fail 'installed inventory does not contain exactly one public v1 header'
fi
if [ "$(find "$INITIAL_PREFIX" -type f -name 'libgamebloke_core.a' | wc -l | tr -d ' ')" -ne 1 ]; then
    fail 'installed inventory does not contain one static core archive'
fi
if [ "$(find "$INITIAL_PREFIX" -type f -name 'gameblokeTargets.cmake' | wc -l | tr -d ' ')" -ne 1 ]; then
    fail 'installed inventory does not contain one static target export'
fi
mv "$INITIAL_PREFIX" "$MOVED_PREFIX"
[ ! -e "$INITIAL_PREFIX" ] && [ -d "$MOVED_PREFIX" ] || fail 'install prefix relocation failed'

EXT_PARENT=${TMPDIR:-/private/tmp}
mkdir -p "$EXT_PARENT"
EXT_ROOT=$(mktemp -d "${EXT_PARENT%/}/gamebloke-phase23-consumer.XXXXXX") || fail 'external consumer scratch creation failed'
EXT_PHYSICAL=$(CDPATH= cd -P "$EXT_ROOT" && pwd -P)
EXT_ROOT=$EXT_PHYSICAL
case "$EXT_PHYSICAL" in "$REPO_ROOT"|"$REPO_ROOT"/*) fail 'external consumer scratch resolved inside checkout' ;; esac
mkdir -m 700 "$EXT_ROOT/source" "$EXT_ROOT/build" "$EXT_ROOT/work" \
    "$RUN_ROOT/sessions/base" "$RUN_ROOT/sessions/replay" \
    "$RUN_ROOT/sessions/refusal"
cp "$EXAMPLE/CMakeLists.txt" "$EXAMPLE/consumer.c" "$EXT_ROOT/source/"
cmp -s "$EXAMPLE/CMakeLists.txt" "$EXT_ROOT/source/CMakeLists.txt" || fail 'external CMake project copy mismatch'
cmp -s "$EXAMPLE/consumer.c" "$EXT_ROOT/source/consumer.c" || fail 'external consumer copy mismatch'
(
    unset CMAKE_PREFIX_PATH CMAKE_MODULE_PATH gamebloke_DIR
    cmake -S "$EXT_ROOT/source" -B "$EXT_ROOT/build" -G Ninja \
        -Dgamebloke_DIR="$MOVED_PREFIX/lib/cmake/gamebloke" \
        -DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF \
        -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF \
        -DCMAKE_FIND_PACKAGE_NO_PACKAGE_REGISTRY=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
        > "$RUN_ROOT/consumer-configure.log" 2>&1
) || {
    cat "$RUN_ROOT/consumer-configure.log" >&2
    fail 'external consumer did not resolve the relocated package'
}
grep -F "$MOVED_PREFIX/include" "$EXT_ROOT/build/compile_commands.json" >/dev/null ||
    fail 'external consumer compile does not resolve the relocated public header'
cmake --build "$EXT_ROOT/build" --verbose > "$RUN_ROOT/consumer-build.log" 2>&1 || {
    cat "$RUN_ROOT/consumer-build.log" >&2
    fail 'external C17 consumer build failed'
}
grep -F "$MOVED_PREFIX/lib/libgamebloke_core.a" "$RUN_ROOT/consumer-build.log" >/dev/null ||
    fail 'external consumer link does not use the relocated static archive'
if [ "$NEGATIVE_ONLY" -eq 1 ]; then
    PRELAUNCH_ROOT="$RUN_ROOT/negative-controls/provenance-before-launch"
    mkdir -p -m 700 "$PRELAUNCH_ROOT/output"
    cp "$IMAGE" "$PRELAUNCH_ROOT/changed-image.gba"
    printf '\001' >> "$PRELAUNCH_ROOT/changed-image.gba"
    PRELAUNCH_RECORD="$PRELAUNCH_ROOT/redefined-record.json"
    write_fixture_record "$SOURCE_DIGEST" "$NOTICE_DIGEST" \
        "$(sha256_file "$PRELAUNCH_ROOT/changed-image.gba")" \
        "$(wc -c < "$PRELAUNCH_ROOT/changed-image.gba" | tr -d ' ')" \
        "$PRELAUNCH_RECORD"
    if (run_consumer "$PRELAUNCH_RECORD" \
        "$PRELAUNCH_ROOT/changed-image.gba" "$STAGED_SOURCE" \
        "$PRELAUNCH_ROOT/output") > "$PRELAUNCH_ROOT/launch.log" 2>&1; then
        fail 'changed-input prelaunch control launched unexpectedly'
    fi
    grep -Fx 'phase23: fixture-record: image digest differs from staged captured digest' \
        "$PRELAUNCH_ROOT/launch.log" >/dev/null || {
        cat "$PRELAUNCH_ROOT/launch.log" >&2
        fail 'changed-input prelaunch control missed the captured-digest diagnostic'
    }
    [ ! -e "$PRELAUNCH_ROOT/output/run.json" ] ||
        fail 'changed-input prelaunch control published consumer evidence'
fi
mkdir -p -m 700 "$RUN_ROOT/negative-controls" \
    "$RUN_ROOT/negative-controls/shift-first-input" \
    "$RUN_ROOT/negative-controls/wrong-video-literal" \
    "$RUN_ROOT/negative-controls/wrong-receipt-left-mask"
if run_consumer "$RECORD" "$IMAGE" "$STAGED_SOURCE" \
    "$RUN_ROOT/negative-controls/shift-first-input" \
    --shift-first-input \
        > "$RUN_ROOT/negative-controls/shift-first-input.log" 2>&1; then
    fail 'shifted-input negative control passed unexpectedly'
fi
grep -F 'phase23: guest-case-mismatch expected PASS case=0x23010001 observed FAIL case=0x2301f001' \
    "$RUN_ROOT/negative-controls/shift-first-input.log" >/dev/null || {
    cat "$RUN_ROOT/negative-controls/shift-first-input.log" >&2
    fail 'shifted-input control lacked the named guest-case mismatch'
}
if run_consumer "$RECORD" "$IMAGE" "$STAGED_SOURCE" \
    "$RUN_ROOT/negative-controls/wrong-video-literal" \
    --wrong-video-literal \
        > "$RUN_ROOT/negative-controls/wrong-video-literal.log" 2>&1; then
    fail 'wrong-video-literal negative control passed unexpectedly'
fi
grep -Fx 'phase23: video-literal-mismatch row=1 pixel=0 offset=480' \
    "$RUN_ROOT/negative-controls/wrong-video-literal.log" >/dev/null || {
    cat "$RUN_ROOT/negative-controls/wrong-video-literal.log" >&2
    fail 'wrong-video-literal control lacked the named row-1 mismatch'
}
if run_consumer "$RECORD" "$IMAGE" "$STAGED_SOURCE" \
    "$RUN_ROOT/negative-controls/wrong-receipt-left-mask" \
    --wrong-receipt-left-mask \
        > "$RUN_ROOT/negative-controls/wrong-receipt-left-mask.log" 2>&1; then
    fail 'wrong-receipt-left-mask negative control passed unexpectedly'
fi
grep -E '^phase23: input receipt does not match call trace checkpoint=2 call=[0-9]+ mask=16 applied=2$' \
    "$RUN_ROOT/negative-controls/wrong-receipt-left-mask.log" >/dev/null || {
    cat "$RUN_ROOT/negative-controls/wrong-receipt-left-mask.log" >&2
    fail 'wrong-receipt-left-mask control lacked its named input-trace mismatch'
}
for negative_dir in \
    "$RUN_ROOT/negative-controls/shift-first-input" \
    "$RUN_ROOT/negative-controls/wrong-video-literal" \
    "$RUN_ROOT/negative-controls/wrong-receipt-left-mask"; do
    for accepted_file in run.json trace.jsonl video.bgr555le audio.s16le refusal.json; do
        [ ! -e "$negative_dir/$accepted_file" ] ||
            fail "negative control published accepted evidence: $accepted_file"
    done
done
printf '%s\n' 'phase23: consumer negative controls verified'
if [ "$NEGATIVE_ONLY" -eq 1 ]; then
    [ ! -e "$RUN_ROOT/session-receipt.json" ] ||
        fail 'negative controls published an accepted receipt'
    printf '%s\n' 'phase23: negative controls verified'
    exit 0
fi
if ! run_consumer "$RECORD" "$IMAGE" "$STAGED_SOURCE" \
      "$RUN_ROOT/sessions/base" > "$RUN_ROOT/session.log" 2>&1; then
    cat "$RUN_ROOT/session.log" >&2
    fail 'installed public API session failed'
fi
grep -Fx 'phase23: installed session verified' "$RUN_ROOT/session.log" >/dev/null ||
    fail 'installed session success marker is missing'
if ! run_consumer "$RECORD" "$IMAGE" "$STAGED_SOURCE" \
      "$RUN_ROOT/sessions/replay" > "$RUN_ROOT/replay.log" 2>&1; then
    cat "$RUN_ROOT/replay.log" >&2
    fail 'fresh public API replay failed'
fi
grep -Fx 'phase23: installed session verified' "$RUN_ROOT/replay.log" >/dev/null ||
    fail 'fresh replay success marker is missing'
for evidence_file in trace.jsonl run.json video.bgr555le audio.s16le; do
    cmp -s "$RUN_ROOT/sessions/base/$evidence_file" \
        "$RUN_ROOT/sessions/replay/$evidence_file" ||
        fail "fresh replay differs in $evidence_file"
done
if ! run_consumer "$RECORD" "$IMAGE" "$STAGED_SOURCE" \
      "$RUN_ROOT/sessions/refusal" --refusal-retry \
        > "$RUN_ROOT/refusal.log" 2>&1; then
    cat "$RUN_ROOT/refusal.log" >&2
    fail 'atomic refusal/retry session failed'
fi
grep -Fx 'phase23: installed session verified' "$RUN_ROOT/refusal.log" >/dev/null ||
    fail 'refusal session success marker is missing'
grep -Fx 'phase23: atomic refusal and retry verified' "$RUN_ROOT/refusal.log" >/dev/null ||
    fail 'refusal session did not report its completed retry proof'
[ -f "$RUN_ROOT/sessions/refusal/refusal.json" ] &&
    [ -s "$RUN_ROOT/sessions/refusal/refusal.json" ] &&
    [ ! -L "$RUN_ROOT/sessions/refusal/refusal.json" ] ||
    fail 'measured refusal receipt is missing or unsafe'
grep -E '^[{]"attempts":\[[{]"status":[0-9]+,"units":512,"budget":512,"result_unchanged":true,"audio_unchanged":true,"observation_unchanged":true[}],[{]"status":[0-9]+,"units":512,"budget":512,"result_unchanged":true,"audio_unchanged":true,"observation_unchanged":true[}]\],"corrected_budget":8192,"retry_matches_fresh_reference":true[}]$' \
    "$RUN_ROOT/sessions/refusal/refusal.json" >/dev/null ||
    fail 'measured refusal receipt omitted expected statuses or postconditions'
for evidence_file in trace.jsonl run.json video.bgr555le audio.s16le; do
    cmp -s "$RUN_ROOT/sessions/base/$evidence_file" \
        "$RUN_ROOT/sessions/refusal/$evidence_file" ||
        fail "refusal/retry final state differs from fresh session in $evidence_file"
done
printf '%s\n' 'phase23: refusal/retry matched fresh session'
VIDEO_DIGEST=$(sha256_file "$RUN_ROOT/sessions/base/video.bgr555le")
AUDIO_DIGEST=$(sha256_file "$RUN_ROOT/sessions/base/audio.s16le")
TRACE_DIGEST=$(sha256_file "$RUN_ROOT/sessions/base/trace.jsonl")
printf '{\n  "fixture_id": "phase23-host-session",\n  "fixture_record": "fixture-record.json",\n  "source_sha256": "%s",\n  "notice_sha256": "%s",\n  "image_sha256": "%s",\n  "baseline": "sessions/base/run.json",\n  "replay": "sessions/replay/run.json",\n  "guest_trace_sha256": "%s",\n  "video_sha256_supplemental": "%s",\n  "audio_sha256_supplemental": "%s",\n  "video_path": "sessions/base/video.bgr555le",\n  "audio_path": "sessions/base/audio.s16le",\n  "host_timing_path": "sessions/base/host-timing.jsonl",\n  "host_presentation": "outside-session-evidence",\n  "permission": "local-phase23-verification-only"\n}\n' \
    "$SOURCE_DIGEST" "$NOTICE_DIGEST" "$IMAGE_DIGEST" \
    "$TRACE_DIGEST" "$VIDEO_DIGEST" "$AUDIO_DIGEST" > \
    "$RUN_ROOT/session-receipt.json"
mv "$RUN_ROOT/session-receipt.json" \
    "$RUN_ROOT/session-baseline-receipt.json"
REFUSAL_RECEIPT=$(cat "$RUN_ROOT/sessions/refusal/refusal.json")
REFUSAL_DIGEST=$(sha256_file "$RUN_ROOT/sessions/refusal/refusal.json")
printf '{\n  "fixture_id": "phase23-host-session",\n  "baseline_receipt": "session-baseline-receipt.json",\n  "refusal_retry": "sessions/refusal/run.json",\n  "refusal_proof": %s,\n  "refusal_proof_sha256": "%s",\n  "final_comparison": "guest trace, run record, full raw video and audio match fresh baseline",\n  "host_presentation": "outside-session-evidence",\n  "permission": "local-phase23-verification-only"\n}\n' \
    "$REFUSAL_RECEIPT" "$REFUSAL_DIGEST" > \
    "$RUN_ROOT/session-receipt.json"
printf 'phase23: installed session, replay, and refusal/retry verified (%s)\n' \
    "$RUN_ROOT"
