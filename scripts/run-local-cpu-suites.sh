#!/bin/sh
# Fetch and run the pinned jsmolka CPU suites locally. The fixture allow-list,
# source binding, guest-cycle budget, process timeout, and result validation live
# in tests/gates/jsmolka_case.cmake; this entry point never broadens them.
set -eu

usage() {
  printf '%s\n' 'usage: sh scripts/run-local-cpu-suites.sh [--fetch] [--preflight-only] [--build-dir DIR]' >&2
  exit 2
}

fetch=0
preflight_only=0
build_dir='build/local-cpu-suites'
while [ "$#" -gt 0 ]; do
  case $1 in
    --fetch)
      fetch=1
      shift
      ;;
    --preflight-only)
      preflight_only=1
      shift
      ;;
    --build-dir)
      [ "$#" -ge 2 ] || usage
      build_dir=$2
      [ -n "$build_dir" ] || usage
      shift 2
      ;;
    -*) usage ;;
    *) usage ;;
  esac
done

script_root=$(CDPATH= cd "$(dirname "$0")/.." && pwd -P)
case $build_dir in
  /*) ;;
  *) build_dir="$script_root/$build_dir" ;;
esac

is_enabled() {
  case $1 in
    '' | 0 | false | FALSE | no | NO) return 1 ;;
    *) return 0 ;;
  esac
}

for hosted_flag in "${CI:-}" "${GITHUB_ACTIONS:-}" "${GITLAB_CI:-}" \
    "${CIRCLECI:-}" "${BUILDKITE:-}" "${TF_BUILD:-}" \
    "${CODEBUILD_BUILD_ID:-}" "${JENKINS_URL:-}" \
    "${BITBUCKET_BUILD_NUMBER:-}" "${TEAMCITY_VERSION:-}"; do
  if is_enabled "$hosted_flag"; then
    printf '%s\n' 'local-cpu-suites: public-ci-refused' >&2
    exit 2
  fi
done

roms='jsmolka/arm.gba jsmolka/thumb.gba jsmolka/memory.gba'
if [ "$fetch" -eq 1 ]; then
  cmake "-DSRC=$script_root" "-DBIN=$build_dir" \
    -DCASE=source-binding -DREQUIRE_BINARIES=OFF \
    -P "$script_root/tests/gates/jsmolka_case.cmake"
  for destination in $roms; do
    sh "$script_root/scripts/fetch-pin.sh" \
      --root "$script_root/.fixtures" "$destination"
  done
fi

missing=0
for fixture in 'jsmolka-arm:jsmolka/arm.gba' \
    'jsmolka-thumb:jsmolka/thumb.gba' 'jsmolka-memory:jsmolka/memory.gba'; do
  fixture_id=${fixture%%:*}
  destination=${fixture#*:}
  if [ ! -f "$script_root/.fixtures/$destination" ]; then
    printf 'local-cpu-suites: MISSING %s\n' "$fixture_id"
    missing=1
  fi
done
if [ "$missing" -ne 0 ]; then
  exit 1
fi

cmake "-DROOT=$script_root" -P "$script_root/cmake/CheckFixtureDir.cmake"
python3 "$script_root/scripts/derive-local-jsmolka.py" \
  --input-dir "$script_root/.fixtures/jsmolka" \
  --output-dir "$build_dir/generated/local-cpu-suites"
cmake "-DSRC=$script_root" "-DBIN=$build_dir" \
  -DCASE=source-binding -DREQUIRE_BINARIES=ON \
  -P "$script_root/tests/gates/jsmolka_case.cmake"
cmake "-DSRC=$script_root" "-DBIN=$build_dir" \
  -DCASE=local-controls -P "$script_root/tests/gates/jsmolka_case.cmake"
if [ "$preflight_only" -eq 1 ]; then
  cmake "-DSRC=$script_root" "-DBIN=$build_dir" \
    "-DRUNNER=$build_dir/gamebloke-run" \
    "-DOUTPUT_ROOT=$build_dir/jsmolka" \
    -DCASE=startup-preflight -P "$script_root/tests/gates/jsmolka_case.cmake"
  exit 0
fi
cmake "-DSRC=$script_root" "-DBIN=$build_dir" \
  "-DRUNNER=$build_dir/gamebloke-run" \
  "-DOUTPUT_ROOT=$build_dir/jsmolka" \
  -DCASE=run-local -P "$script_root/tests/gates/jsmolka_case.cmake"
