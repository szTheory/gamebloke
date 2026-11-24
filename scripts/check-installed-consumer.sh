#!/bin/sh
set -eu

if [ -n "${CI:-}" ]; then
    printf '%s\n' 'check-installed-consumer: refused because CI is set' >&2
    exit 2
fi

NEGATIVE_CONTROLS=OFF
CHECK_FIT=OFF
for argument in "$@"; do
    case "$argument" in
        --negative-controls) NEGATIVE_CONTROLS=ON ;;
        --check-fit) CHECK_FIT=ON ;;
        *)
            printf '%s\n' 'usage: sh scripts/check-installed-consumer.sh [--negative-controls] [--check-fit]' >&2
            exit 2
            ;;
    esac
done

SCRIPT_DIR=$(CDPATH= cd -P "$(dirname "$0")" && pwd -P)
REPO_ROOT=$(CDPATH= cd -P "$SCRIPT_DIR/.." && pwd -P)
LIBFUTURO_ROOT=
if [ "$CHECK_FIT" = ON ]; then
    LIBFUTURO_REQUEST=${GBL_LIBFUTURO_CHECKOUT:-"$REPO_ROOT/../libfuturo"}
    if [ ! -d "$LIBFUTURO_REQUEST" ]; then
        printf '%s\n' 'check-installed-consumer: read-only LibFuturo checkout is unavailable; set GBL_LIBFUTURO_CHECKOUT to an equivalent local checkout' >&2
        exit 1
    fi
    LIBFUTURO_ROOT=$(CDPATH= cd -P "$LIBFUTURO_REQUEST" && pwd -P)
fi
PHASE_ROOT="$REPO_ROOT/build/phase21-local"
WORK_PARENT="$PHASE_ROOT/work"
FIXTURE_DIR="$PHASE_ROOT/fixture"
ROM_PATH="$FIXTURE_DIR/phase21-diagnostic.gba"
EVIDENCE_PARENT="$PHASE_ROOT/evidence"
RUN_ID=$(date -u '+%Y%m%dT%H%M%SZ')-$$
WORK_ROOT="$WORK_PARENT/$RUN_ID"
EVIDENCE_ROOT="$EVIDENCE_PARENT/$RUN_ID"
EXT_ROOT=
EXT_PHYSICAL=
WORK_MARKER="$WORK_ROOT/.gamebloke-phase21-owner"
START_SECONDS=$(date +%s)

fail()
{
    printf 'check-installed-consumer: %s\n' "$*" >&2
    exit 1
}

is_within()
{
    case "$1" in
        "$2"|"$2"/*) return 0 ;;
        *) return 1 ;;
    esac
}

canonical_file()
{
    file_path=$1
    [ -f "$file_path" ] && [ ! -L "$file_path" ] || return 1
    file_directory=$(dirname "$file_path")
    file_name=$(basename "$file_path")
    physical_directory=$(CDPATH= cd -P "$file_directory" 2>/dev/null && pwd -P) ||
        return 1
    printf '%s/%s\n' "$physical_directory" "$file_name"
}

cleanup()
{
    if [ -n "$EXT_ROOT" ] && [ -n "$EXT_PHYSICAL" ] && [ -d "$EXT_ROOT" ]; then
        EXT_NOW=$(CDPATH= cd -P "$EXT_ROOT" 2>/dev/null && pwd -P || true)
        if [ "$EXT_NOW" = "$EXT_PHYSICAL" ] &&
           ! is_within "$EXT_NOW" "$REPO_ROOT"; then
            rm -rf "$EXT_ROOT"
        fi
    fi
    if [ -f "$WORK_MARKER" ] &&
       [ "$(cat "$WORK_MARKER")" = "$RUN_ID" ] &&
       [ "$(CDPATH= cd -P "$WORK_ROOT" && pwd -P)" = "$WORK_ROOT" ]; then
        rm -rf "$WORK_ROOT"
    fi
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

if [ "$NEGATIVE_CONTROLS" = ON ]; then
    if CI_OUTPUT=$(CI=1 sh "$SCRIPT_DIR/check-installed-consumer.sh" 2>&1); then
        CI_STATUS=0
    else
        CI_STATUS=$?
    fi
    [ "$CI_STATUS" -eq 2 ] ||
        fail 'CI refusal control did not exit with status 2'
    printf '%s\n' "$CI_OUTPUT" |
        grep -Fx 'check-installed-consumer: refused because CI is set' >/dev/null ||
        fail 'CI refusal control did not report its specific refusal'
    [ ! -e "$WORK_ROOT" ] && [ ! -e "$EVIDENCE_ROOT" ] ||
        fail 'CI refusal created task scratch before rejecting the run'
fi

case "$PHASE_ROOT" in
    "$REPO_ROOT"/build/phase21-local) ;;
    *) fail 'local scratch root escaped build/phase21-local' ;;
esac
if [ -L "$REPO_ROOT/build" ] || [ -L "$PHASE_ROOT" ] ||
   [ -L "$WORK_PARENT" ] || [ -L "$FIXTURE_DIR" ] ||
   [ -L "$EVIDENCE_PARENT" ]; then
    fail 'refusing a symlinked Phase 21 scratch path'
fi
mkdir -p "$PHASE_ROOT" "$WORK_PARENT" "$FIXTURE_DIR" "$EVIDENCE_PARENT"
PHASE_ROOT_PHYSICAL=$(CDPATH= cd -P "$PHASE_ROOT" && pwd -P)
if [ "$PHASE_ROOT_PHYSICAL" != "$PHASE_ROOT" ]; then
    fail 'Phase 21 scratch path is not physically contained in the repository build root'
fi
git -C "$REPO_ROOT" check-ignore -q "$ROM_PATH" ||
    fail 'generated diagnostic path is not ignored'
if git -C "$REPO_ROOT" ls-files --error-unmatch -- "$ROM_PATH" >/dev/null 2>&1; then
    fail 'refusing to write a tracked diagnostic path'
fi
if [ -L "$ROM_PATH" ]; then
    fail 'refusing a symlinked generated diagnostic path'
fi
if [ -e "$WORK_ROOT" ] || [ -e "$EVIDENCE_ROOT" ]; then
    fail 'unique invocation path already exists'
fi
mkdir -m 700 "$WORK_ROOT" "$EVIDENCE_ROOT"
printf '%s' "$RUN_ID" > "$WORK_MARKER"
git -C "$REPO_ROOT" check-ignore -q "$EVIDENCE_ROOT" ||
    fail 'local proof evidence path is not ignored'

CC_REQUEST=${CC:-cc}
case "$CC_REQUEST" in
    */*) CC_PATH=$CC_REQUEST ;;
    *) CC_PATH=$(command -v "$CC_REQUEST" || true) ;;
esac
[ -n "$CC_PATH" ] && [ -x "$CC_PATH" ] ||
    fail "host C compiler is unavailable: $CC_REQUEST"
CC_REAL=$CC_PATH
NINJA_PATH=$(command -v ninja || true)
[ -n "$NINJA_PATH" ] && [ -x "$NINJA_PATH" ] ||
    fail 'Ninja is unavailable'
CC_IDENTITY=$("$CC_REAL" --version | sed -n '1p')
CC_TARGET=$("$CC_REAL" -dumpmachine 2>/dev/null || uname -m)
CMAKE_IDENTITY=$(cmake --version | sed -n '1p')
NINJA_IDENTITY=$(ninja --version)

GENERATOR="$WORK_ROOT/phase21-diagnostic-generator"
GENERATOR_DEFINE="-DGBL_PHASE21_OUTPUT_DIR=\"$FIXTURE_DIR\""
"$CC_REAL" -std=c17 -O2 -Wall -Wextra -Wpedantic -Wconversion \
    -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wundef -Wcast-qual \
    -Werror "$GENERATOR_DEFINE" \
    "$REPO_ROOT/tests/fixtures/phase21_diagnostic.c" -o "$GENERATOR" \
    >"$EVIDENCE_ROOT/generator-build.log" 2>&1 || {
        cat "$EVIDENCE_ROOT/generator-build.log" >&2
        fail 'C17 diagnostic generator did not compile'
    }
if [ -e "$ROM_PATH" ]; then
    [ -f "$ROM_PATH" ] && [ ! -L "$ROM_PATH" ] ||
        fail 'existing local diagnostic is not a regular file'
    printf '%s\n' 'preserved_existing_fixture=yes' \
        >"$EVIDENCE_ROOT/generator.log"
else
    "$GENERATOR" "$FIXTURE_DIR" >"$EVIDENCE_ROOT/generator.log" 2>&1 || {
        cat "$EVIDENCE_ROOT/generator.log" >&2
        fail 'C17 diagnostic generator failed'
    }
fi
[ -f "$ROM_PATH" ] && [ ! -L "$ROM_PATH" ] ||
    fail 'generator did not produce the assigned regular file'
git -C "$REPO_ROOT" check-ignore -q "$ROM_PATH" ||
    fail 'generated diagnostic stopped being ignored'
if git -C "$REPO_ROOT" ls-files --error-unmatch -- "$ROM_PATH" >/dev/null 2>&1; then
    fail 'generated diagnostic was unexpectedly tracked'
fi

cmake \
    -DGBL_STAGE=preflight \
    -DGBL_REPO_ROOT="$REPO_ROOT" \
    -DGBL_MANIFEST="$REPO_ROOT/.planning/phases/21-installed-consumer-proof-and-libfuturo-fit/fixture-manifest.json" \
    -DGBL_SOURCE_PATH="$REPO_ROOT/tests/fixtures/phase21_diagnostic.c" \
    -DGBL_ROM_PATH="$ROM_PATH" \
    -DGBL_SCRATCH="$WORK_ROOT" \
    -DGBL_EVIDENCE_ROOT="$EVIDENCE_ROOT" \
    -DGBL_COMPILER_IDENTITY="$CC_IDENTITY" \
    -DGBL_COMPILER_TARGET="$CC_TARGET" \
    -DGBL_CMAKE_IDENTITY="$CMAKE_IDENTITY" \
    -DGBL_NINJA_IDENTITY="$NINJA_IDENTITY" \
    -DGBL_NEGATIVE_CONTROLS="$NEGATIVE_CONTROLS" \
    -P "$REPO_ROOT/scripts/check-installed-consumer.cmake" \
    >"$EVIDENCE_ROOT/manifest-preflight.log" 2>&1 || {
        cat "$EVIDENCE_ROOT/manifest-preflight.log" >&2
        fail 'fixture provenance, rights, recipe, or digest check failed'
    }

if [ "$NEGATIVE_CONTROLS" = ON ]; then
    if git -C "$REPO_ROOT" grep -n -E \
       'check-installed-consumer|phase21_diagnostic|phase21-diagnostic|gamebloke[.]phase21-consumer|fixture-manifest[.]json|phase21-local' \
       -- CMakeLists.txt cmake .github/workflows tests/inventory \
       >"$EVIDENCE_ROOT/public-registration-search.txt" 2>&1; then
        cat "$EVIDENCE_ROOT/public-registration-search.txt" >&2
        fail 'Phase 21 local fixture or receipt leaked into root/public registrations'
    fi
    if [ -n "$(git -C "$REPO_ROOT" ls-files -- '*.gba' '*.agb' \
            'build/phase21-local/*')" ]; then
        fail 'generated ROM or local receipt is tracked'
    fi
fi

PACKAGE_BUILD="$WORK_ROOT/package-build"
EXT_TMP=${TMPDIR:-/tmp}
EXT_ROOT=$(mktemp -d "${EXT_TMP%/}/gamebloke-phase21-XXXXXX") ||
    fail 'cannot create external scratch directory'
EXT_PHYSICAL=$(CDPATH= cd -P "$EXT_ROOT" && pwd -P)
EXT_ROOT=$EXT_PHYSICAL
is_within "$EXT_PHYSICAL" "$REPO_ROOT" &&
    fail 'external scratch directory resolves inside the repository'
is_within "$EXT_PHYSICAL" "$WORK_ROOT" &&
    fail 'external scratch directory resolves inside the package build root'
INITIAL_PREFIX="$EXT_ROOT/initial-prefix"
MOVED_PREFIX="$EXT_ROOT/moved-prefix"
mkdir -p "$INITIAL_PREFIX"

cmake -S "$REPO_ROOT" -B "$PACKAGE_BUILD" -G Ninja \
    -DCMAKE_C_COMPILER="$CC_REAL" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_COMPILE_WARNING_AS_ERROR=ON -DGBL_LANE=ci \
    >"$EVIDENCE_ROOT/root-configure.log" 2>&1 || {
        cat "$EVIDENCE_ROOT/root-configure.log" >&2
        fail 'root package configure failed; no host dependencies were installed'
    }
cmake --build "$PACKAGE_BUILD" --target gamebloke_core --verbose \
    >"$EVIDENCE_ROOT/root-build.log" 2>&1 || {
        cat "$EVIDENCE_ROOT/root-build.log" >&2
        fail 'building only gamebloke_core failed'
    }
cmake --install "$PACKAGE_BUILD" --prefix "$INITIAL_PREFIX" \
    --component GameblokeCoreApi >"$EVIDENCE_ROOT/root-install.log" 2>&1 || {
        cat "$EVIDENCE_ROOT/root-install.log" >&2
        fail 'GameblokeCoreApi component install failed'
    }
PACKAGE_INVENTORY=$(cd "$INITIAL_PREFIX" && find . -type f -print | sort)
EXPECTED_INVENTORY=$(printf '%s\n' \
    './include/gamebloke/gamebloke_v1.h' \
    './lib/cmake/gamebloke/gameblokeConfig.cmake' \
    './lib/cmake/gamebloke/gameblokeTargets-release.cmake' \
    './lib/cmake/gamebloke/gameblokeTargets.cmake' \
    './lib/libgamebloke_core.a')
if [ "$PACKAGE_INVENTORY" != "$EXPECTED_INVENTORY" ]; then
    printf '%s\n' "$PACKAGE_INVENTORY" > "$EVIDENCE_ROOT/package-inventory.actual.txt"
    printf '%s\n' "$EXPECTED_INVENTORY" > "$EVIDENCE_ROOT/package-inventory.expected.txt"
    fail 'installed GameblokeCoreApi inventory did not match the exact public component'
fi
mv "$INITIAL_PREFIX" "$MOVED_PREFIX"
[ ! -e "$INITIAL_PREFIX" ] || fail 'original package prefix still exists after relocation'

SOURCE_INITIAL="$EXT_ROOT/source-initial"
SOURCE_MOVED="$EXT_ROOT/source-moved"
BUILD_INITIAL="$EXT_ROOT/build-initial"
BUILD_MOVED="$EXT_ROOT/build-moved"
EXECUTION_DIR="$EXT_ROOT/consumer-working-directory"
MISSING_BUILD="$EXT_ROOT/build-package-missing"
mkdir "$SOURCE_INITIAL"
cp "$REPO_ROOT/examples/installed-consumer/CMakeLists.txt" \
   "$REPO_ROOT/examples/installed-consumer/consumer.c" "$SOURCE_INITIAL/"
mv "$SOURCE_INITIAL" "$SOURCE_MOVED"
[ ! -e "$SOURCE_INITIAL" ] || fail 'original consumer staging path still exists'
mkdir "$EXECUTION_DIR"

env -u gamebloke_DIR -u Gamebloke_DIR -u GAMEBLOKE_DIR \
    -u gamebloke_ROOT -u Gamebloke_ROOT -u CMAKE_PREFIX_PATH \
    -u CMAKE_MODULE_PATH -u CMAKE_INCLUDE_PATH -u CMAKE_LIBRARY_PATH \
    -u CMAKE_FRAMEWORK_PATH \
    cmake -S "$SOURCE_MOVED" -B "$BUILD_INITIAL" -G Ninja \
      -DCMAKE_C_COMPILER="$CC_REAL" -DCMAKE_MAKE_PROGRAM="$NINJA_PATH" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -Dgamebloke_DIR:PATH="$MOVED_PREFIX/lib/cmake/gamebloke" \
      -DCMAKE_PREFIX_PATH= -DCMAKE_MODULE_PATH= \
      -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON \
      -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE \
      -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE \
      -DCMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH=FALSE \
      -DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH=FALSE \
      -DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=FALSE \
      -DCMAKE_FIND_USE_INSTALL_PREFIX=FALSE \
      -DCMAKE_FIND_USE_PACKAGE_ROOT_PATH=FALSE \
      --debug-find-pkg=gamebloke >"$EVIDENCE_ROOT/consumer-configure.log" 2>&1 || {
        cat "$EVIDENCE_ROOT/consumer-configure.log" >&2
        fail 'external consumer could not discover the explicit relocated package'
    }
grep -F "$MOVED_PREFIX/lib/cmake/gamebloke/gameblokeConfig.cmake" \
    "$EVIDENCE_ROOT/consumer-configure.log" >/dev/null ||
    fail 'configure log did not identify the moved package config'
grep -F "gamebloke_DIR:PATH=$MOVED_PREFIX/lib/cmake/gamebloke" \
    "$BUILD_INITIAL/CMakeCache.txt" >/dev/null ||
    fail 'CMake cache did not bind the relocated package directory'

[ ! -e "$MISSING_BUILD" ] ||
    fail 'unique missing-package build path already exists'
if env -u gamebloke_DIR -u Gamebloke_DIR -u GAMEBLOKE_DIR \
    -u gamebloke_ROOT -u Gamebloke_ROOT -u CMAKE_PREFIX_PATH \
    -u CMAKE_MODULE_PATH -u CMAKE_INCLUDE_PATH -u CMAKE_LIBRARY_PATH \
    -u CMAKE_FRAMEWORK_PATH \
    cmake -S "$SOURCE_MOVED" -B "$MISSING_BUILD" -G Ninja \
      -DCMAKE_C_COMPILER="$CC_REAL" -DCMAKE_MAKE_PROGRAM="$NINJA_PATH" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DCMAKE_PREFIX_PATH= -DCMAKE_MODULE_PATH= \
      -DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON \
      -DCMAKE_FIND_USE_PACKAGE_REGISTRY=FALSE \
      -DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=FALSE \
      -DCMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH=FALSE \
      -DCMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH=FALSE \
      -DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=FALSE \
      -DCMAKE_FIND_USE_INSTALL_PREFIX=FALSE \
      -DCMAKE_FIND_USE_PACKAGE_ROOT_PATH=FALSE \
      --debug-find-pkg=gamebloke \
      >"$EVIDENCE_ROOT/package-missing.log" 2>&1; then
    fail 'source-only consumer unexpectedly configured without the moved package'
else
    MISSING_STATUS=$?
fi
[ "$MISSING_STATUS" -ne 0 ] ||
    fail 'missing-package negative control unexpectedly succeeded'
grep -F 'Could not find a package configuration file provided by "gamebloke"' \
    "$EVIDENCE_ROOT/package-missing.log" >/dev/null || {
        cat "$EVIDENCE_ROOT/package-missing.log" >&2
        fail 'missing-package control failed without the expected package-specific diagnostic'
    }

MOVED_PREFIX_REAL=$(CDPATH= cd -P "$MOVED_PREFIX" && pwd -P)
MOVED_CONFIG_REAL=$(canonical_file \
    "$MOVED_PREFIX/lib/cmake/gamebloke/gameblokeConfig.cmake") ||
    fail 'relocated package config is missing or symlinked'
MOVED_INCLUDE_REAL=$(CDPATH= cd -P "$MOVED_PREFIX/include" && pwd -P) ||
    fail 'relocated package include directory is missing'
MOVED_ARCHIVE_REAL=$(canonical_file "$MOVED_PREFIX/lib/libgamebloke_core.a") ||
    fail 'relocated package archive is missing or symlinked'
for resolved in "$MOVED_CONFIG_REAL" "$MOVED_INCLUDE_REAL" "$MOVED_ARCHIVE_REAL"; do
    is_within "$resolved" "$MOVED_PREFIX_REAL" ||
        fail "resolved Gamebloke dependency escaped moved prefix: $resolved"
done
cmake --build "$BUILD_INITIAL" --target gamebloke-installed-consumer --verbose \
    >"$EVIDENCE_ROOT/consumer-build.log" 2>&1 || {
        cat "$EVIDENCE_ROOT/consumer-build.log" >&2
        fail 'external C17 consumer build failed'
    }
grep -F "$MOVED_INCLUDE_REAL" "$BUILD_INITIAL/compile_commands.json" >/dev/null ||
    fail 'actual compile command did not include the moved package header directory'
grep -F "$MOVED_ARCHIVE_REAL" "$EVIDENCE_ROOT/consumer-build.log" >/dev/null ||
    fail 'actual link command did not use the moved package archive'
if grep -F "$REPO_ROOT" "$BUILD_INITIAL/compile_commands.json" \
    "$EVIDENCE_ROOT/consumer-build.log" >/dev/null; then
    fail 'repository source or build path leaked into external compile/link inputs'
fi
if grep -F "$INITIAL_PREFIX" "$BUILD_INITIAL/compile_commands.json" \
    "$EVIDENCE_ROOT/consumer-build.log" >/dev/null; then
    fail 'original install prefix leaked into external compile/link inputs'
fi
cp "$BUILD_INITIAL/compile_commands.json" "$EVIDENCE_ROOT/compile_commands.json"
cp "$BUILD_INITIAL/CMakeCache.txt" "$EVIDENCE_ROOT/CMakeCache.txt"
mv "$BUILD_INITIAL" "$BUILD_MOVED"
[ ! -e "$BUILD_INITIAL" ] || fail 'original external consumer build path still exists'

for output_name in run-1 run-2 timing-control expectation-video expectation-audio \
                   refusal-canary-control; do
    mkdir "$EVIDENCE_ROOT/$output_name"
done
(cd "$EXECUTION_DIR" && \
    "$BUILD_MOVED/gamebloke-installed-consumer" "$ROM_PATH" \
      "$EVIDENCE_ROOT/run-1") >"$EVIDENCE_ROOT/run-1.stdout" 2>"$EVIDENCE_ROOT/run-1.stderr" || {
        cat "$EVIDENCE_ROOT/run-1.stderr" >&2
        fail 'first fresh external consumer process failed'
    }
(cd "$EXECUTION_DIR" && \
    "$BUILD_MOVED/gamebloke-installed-consumer" "$ROM_PATH" \
      "$EVIDENCE_ROOT/run-2") >"$EVIDENCE_ROOT/run-2.stdout" 2>"$EVIDENCE_ROOT/run-2.stderr" || {
        cat "$EVIDENCE_ROOT/run-2.stderr" >&2
        fail 'second fresh external consumer process failed'
    }
(cd "$EXECUTION_DIR" && \
    "$BUILD_MOVED/gamebloke-installed-consumer" "$ROM_PATH" \
      "$EVIDENCE_ROOT/timing-control" --timing-control) \
      >"$EVIDENCE_ROOT/timing-control.stdout" \
      2>"$EVIDENCE_ROOT/timing-control.stderr" || {
        cat "$EVIDENCE_ROOT/timing-control.stderr" >&2
        fail 'same-ROM timing control did not produce the expected guest FAIL case'
    }
grep -F '"status": "fail"' "$EVIDENCE_ROOT/timing-control/observations.json" >/dev/null ||
    fail 'timing control evidence did not record the guest FAIL verdict'

if [ "$NEGATIVE_CONTROLS" = ON ]; then
if (cd "$EXECUTION_DIR" && \
    "$BUILD_MOVED/gamebloke-installed-consumer" "$ROM_PATH" \
      "$EVIDENCE_ROOT/refusal-canary-control" \
      --refusal-canary-control) \
      >"$EVIDENCE_ROOT/refusal-canary-control.stdout" \
      2>"$EVIDENCE_ROOT/refusal-canary-control.stderr"; then
    fail 'refusal caller-canary mutation unexpectedly passed'
else
    REFUSAL_CANARY_STATUS=$?
fi
[ "${REFUSAL_CANARY_STATUS:-0}" -ne 0 ] ||
    fail 'refusal caller-canary mutation did not fail'
grep -F 'installed-consumer: refusal-audio-canary-mismatch:' \
    "$EVIDENCE_ROOT/refusal-canary-control.stderr" >/dev/null || {
        cat "$EVIDENCE_ROOT/refusal-canary-control.stderr" >&2
        fail 'refusal caller-canary mutation failed for the wrong reason'
    }
for file in observations.json frame.bgr555le audio.s16le; do
    [ ! -e "$EVIDENCE_ROOT/refusal-canary-control/$file" ] ||
        fail "refusal canary mutation published passing output: $file"
done

if (cd "$EXECUTION_DIR" && \
    "$BUILD_MOVED/gamebloke-installed-consumer" "$ROM_PATH" \
      "$EVIDENCE_ROOT/expectation-video" \
      --expectation-control video) \
      >"$EVIDENCE_ROOT/expectation-video.stdout" \
      2>"$EVIDENCE_ROOT/expectation-video.stderr"; then
    fail 'video expectation mutation unexpectedly passed'
else
    VIDEO_CONTROL_STATUS=$?
fi
[ "$VIDEO_CONTROL_STATUS" -ne 0 ] ||
    fail 'video expectation mutation did not fail'
grep -F 'installed-consumer: expected-video-mismatch:' \
    "$EVIDENCE_ROOT/expectation-video.stderr" >/dev/null || {
        cat "$EVIDENCE_ROOT/expectation-video.stderr" >&2
        fail 'video expectation mutation failed for the wrong reason'
    }
for file in observations.json frame.bgr555le audio.s16le; do
    [ ! -e "$EVIDENCE_ROOT/expectation-video/$file" ] ||
        fail "video mutation published passing output: $file"
done

if (cd "$EXECUTION_DIR" && \
    "$BUILD_MOVED/gamebloke-installed-consumer" "$ROM_PATH" \
      "$EVIDENCE_ROOT/expectation-audio" \
      --expectation-control audio) \
      >"$EVIDENCE_ROOT/expectation-audio.stdout" \
      2>"$EVIDENCE_ROOT/expectation-audio.stderr"; then
    fail 'audio expectation mutation unexpectedly passed'
else
    AUDIO_CONTROL_STATUS=$?
fi
[ "$AUDIO_CONTROL_STATUS" -ne 0 ] ||
    fail 'audio expectation mutation did not fail'
grep -F 'installed-consumer: expected-audio-mismatch:' \
    "$EVIDENCE_ROOT/expectation-audio.stderr" >/dev/null || {
        cat "$EVIDENCE_ROOT/expectation-audio.stderr" >&2
        fail 'audio expectation mutation failed for the wrong reason'
    }
for file in observations.json frame.bgr555le audio.s16le; do
    [ ! -e "$EVIDENCE_ROOT/expectation-audio/$file" ] ||
        fail "audio mutation published passing output: $file"
done
fi

cmake \
    -DGBL_STAGE=evidence \
    -DGBL_REPO_ROOT="$REPO_ROOT" \
    -DGBL_MANIFEST="$REPO_ROOT/.planning/phases/21-installed-consumer-proof-and-libfuturo-fit/fixture-manifest.json" \
    -DGBL_SOURCE_PATH="$REPO_ROOT/tests/fixtures/phase21_diagnostic.c" \
    -DGBL_ROM_PATH="$ROM_PATH" \
    -DGBL_SCRATCH="$WORK_ROOT" \
    -DGBL_EVIDENCE_ROOT="$EVIDENCE_ROOT" \
    -DGBL_COMPILER_IDENTITY="$CC_IDENTITY" \
    -DGBL_COMPILER_TARGET="$CC_TARGET" \
    -DGBL_CMAKE_IDENTITY="$CMAKE_IDENTITY" \
    -DGBL_NINJA_IDENTITY="$NINJA_IDENTITY" \
    -DGBL_RUN1_DIR="$EVIDENCE_ROOT/run-1" \
    -DGBL_RUN2_DIR="$EVIDENCE_ROOT/run-2" \
    -DGBL_TIMING_CONTROL_DIR="$EVIDENCE_ROOT/timing-control" \
    -P "$REPO_ROOT/scripts/check-installed-consumer.cmake" \
    >"$EVIDENCE_ROOT/manifest-evidence.log" 2>&1 || {
        cat "$EVIDENCE_ROOT/manifest-evidence.log" >&2
        fail 'installed consumer evidence did not match the admitted manifest'
    }

for file in observations.json frame.bgr555le audio.s16le; do
    cmp -s "$EVIDENCE_ROOT/run-1/$file" "$EVIDENCE_ROOT/run-2/$file" ||
        fail "fresh process output differed: $file"
done

cat > "$EVIDENCE_ROOT/package-inputs.txt" <<EOF
compiler=$CC_IDENTITY
compiler_path=$CC_REAL
compiler_target=$CC_TARGET
cmake=$CMAKE_IDENTITY
ninja=$NINJA_IDENTITY
toolchain_compiler=$CC_IDENTITY
toolchain_target=$CC_TARGET
toolchain_cmake=$CMAKE_IDENTITY
toolchain_ninja=$NINJA_IDENTITY
platform=$(uname -s)/$(uname -m)
package_config=$MOVED_CONFIG_REAL
package_include=$MOVED_INCLUDE_REAL
package_archive=$MOVED_ARCHIVE_REAL
external_source=$SOURCE_MOVED
external_build=$BUILD_MOVED
external_working_directory=$EXECUTION_DIR
EOF
ROM_SHA256=$(shasum -a 256 "$ROM_PATH" | awk '{print $1}')
SOURCE_SHA256=$(shasum -a 256 "$REPO_ROOT/tests/fixtures/phase21_diagnostic.c" |
    awk '{print $1}')
FRAME_SHA256=$(shasum -a 256 "$EVIDENCE_ROOT/run-1/frame.bgr555le" |
    awk '{print $1}')
AUDIO_SHA256=$(shasum -a 256 "$EVIDENCE_ROOT/run-1/audio.s16le" |
    awk '{print $1}')
cat > "$EVIDENCE_ROOT/identity-and-replay.txt" <<EOF
fixture_path=build/phase21-local/fixture/phase21-diagnostic.gba
fixture_sha256=$ROM_SHA256
source_path=tests/fixtures/phase21_diagnostic.c
source_sha256=$SOURCE_SHA256
frame_sha256=$FRAME_SHA256
audio_sha256=$AUDIO_SHA256
comparison=two fresh processes matched normalized observations and complete A/V bytes
timing_control=same fixture, A event shifted from cycle 1024 to 1536, guest FAIL case 21001
EOF
printf '%s\n' \
    'package_missing=expected gamebloke package-config discovery failure' \
    "timing_control=same ROM, expected guest FAIL case 21001" \
    "video_mutation_status=${VIDEO_CONTROL_STATUS:-not-run}" \
    "audio_mutation_status=${AUDIO_CONTROL_STATUS:-not-run}" \
    "refusal_canary_mutation_status=${REFUSAL_CANARY_STATUS:-not-run}" \
    "ci_refusal=$([ "$NEGATIVE_CONTROLS" = ON ] && printf checked || printf not-run)" \
    "manifest_negative_controls=$([ "$NEGATIVE_CONTROLS" = ON ] && printf checked || printf not-run)" \
    >"$EVIDENCE_ROOT/control-results.txt"
cp "$EVIDENCE_ROOT/run-1/observations.json" "$EVIDENCE_ROOT/proof.json"
END_SECONDS=$(date +%s)
printf 'elapsed_seconds=%s\n' "$((END_SECONDS - START_SECONDS))" \
    > "$EVIDENCE_ROOT/feedback-time.txt"

if [ "$CHECK_FIT" = ON ]; then
    FIT_REPORT="$REPO_ROOT/.planning/phases/21-installed-consumer-proof-and-libfuturo-fit/LIBFUTURO-FIT.md"
    check_fit_report()
    {
        cmake \
            -DGBL_STAGE=fit \
            -DGBL_REPO_ROOT="$REPO_ROOT" \
            -DGBL_MANIFEST="$REPO_ROOT/.planning/phases/21-installed-consumer-proof-and-libfuturo-fit/fixture-manifest.json" \
            -DGBL_SOURCE_PATH="$REPO_ROOT/tests/fixtures/phase21_diagnostic.c" \
            -DGBL_ROM_PATH="$ROM_PATH" \
            -DGBL_SCRATCH="$WORK_ROOT" \
            -DGBL_EVIDENCE_ROOT="$EVIDENCE_ROOT" \
            -DGBL_COMPILER_IDENTITY="$CC_IDENTITY" \
            -DGBL_COMPILER_TARGET="$CC_TARGET" \
            -DGBL_CMAKE_IDENTITY="$CMAKE_IDENTITY" \
            -DGBL_NINJA_IDENTITY="$NINJA_IDENTITY" \
            -DGBL_PROOF_PATH="$EVIDENCE_ROOT/proof.json" \
            -DGBL_PACKAGE_INPUTS="$EVIDENCE_ROOT/package-inputs.txt" \
            -DGBL_FIT_REPORT="$1" \
            -DGBL_LIBFUTURO_CHECKOUT="$LIBFUTURO_ROOT" \
            -P "$REPO_ROOT/scripts/check-installed-consumer.cmake" \
            >"$2" 2>&1
    }
    if ! check_fit_report "$FIT_REPORT" "$EVIDENCE_ROOT/fit-validation.log"; then
        cat "$EVIDENCE_ROOT/fit-validation.log" >&2
        fail 'LibFuturo fit report or refreshed source identity did not match the local proof'
    fi
    if [ "$NEGATIVE_CONTROLS" = ON ]; then
        sed 's/^normalized_proof_sha256=.*/normalized_proof_sha256=0000000000000000000000000000000000000000000000000000000000000000/' \
            "$FIT_REPORT" >"$EVIDENCE_ROOT/fit-stale-digest.md"
        if check_fit_report "$EVIDENCE_ROOT/fit-stale-digest.md" \
             "$EVIDENCE_ROOT/fit-stale-digest.log"; then
            fail 'stale proof digest report control unexpectedly passed'
        fi
        grep -F 'phase21-fit: normalized proof digest mismatch' \
            "$EVIDENCE_ROOT/fit-stale-digest.log" >/dev/null || {
                cat "$EVIDENCE_ROOT/fit-stale-digest.log" >&2
                fail 'stale proof digest report control failed for the wrong reason'
            }
        awk '
            $0 == "## Bounded work" { skip = 1 }
            $0 == "## Output ownership" { skip = 0 }
            !skip { print }
        ' "$FIT_REPORT" >"$EVIDENCE_ROOT/fit-missing-dimension.md"
        if check_fit_report "$EVIDENCE_ROOT/fit-missing-dimension.md" \
             "$EVIDENCE_ROOT/fit-missing-dimension.log"; then
            fail 'missing fit dimension report control unexpectedly passed'
        fi
        grep -F 'phase21-fit: missing required fit dimension: Bounded work' \
            "$EVIDENCE_ROOT/fit-missing-dimension.log" >/dev/null || {
                cat "$EVIDENCE_ROOT/fit-missing-dimension.log" >&2
                fail 'missing fit dimension report control failed for the wrong reason'
            }
        printf '%s\n' \
            'fit_report=pass' \
            'fit_stale_digest=refused' \
            'fit_missing_dimension=refused' \
            'libfuturo_recheck=read-only-pass' \
            >>"$EVIDENCE_ROOT/control-results.txt"
    fi
fi

printf 'check-installed-consumer: pass; evidence=%s\n' \
    "${EVIDENCE_ROOT#"$REPO_ROOT/"}"
printf 'check-installed-consumer: fixture_sha256=%s\n' "$ROM_SHA256"
printf 'check-installed-consumer: frame_sha256=%s audio_sha256=%s\n' \
    "$FRAME_SHA256" "$AUDIO_SHA256"
