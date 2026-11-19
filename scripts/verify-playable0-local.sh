#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${CI:-}" ]]; then
  printf '%s\n' 'verify-playable0-local: refused because CI is set' >&2
  exit 2
fi

source_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${source_dir}/build/local-playable0"
cmake_bin="${CMAKE:-cmake}"

sh "${source_dir}/scripts/verify-phase10-fast.sh" --bin "${source_dir}/build/ci"
"${cmake_bin}" --preset local-playable0
"${cmake_bin}" --build --preset local-playable0 --target \
  gamebloke-run gamebloke-run-control runner_test

for case in admission verify control; do
  "${cmake_bin}" "-DSRC=${source_dir}" "-DBIN=${build_dir}" \
    "-DCASE=${case}" "-DGENERATOR=${build_dir}/gbl_instruction_image" \
    -P "${source_dir}/tests/gates/playable0_guest.cmake"
done
