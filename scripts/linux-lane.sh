#!/bin/sh
# Local Linux runner for any workflow preset (developer tool; CI does not call
# it). Runs `cmake --workflow --preset <preset>` in a digest-pinned ubuntu:26.04
# container so a Linux-only failure (GCC-only warnings, sanitizer runtime
# differences) shows up before a push.
#
#   sh scripts/linux-lane.sh <workflow-preset> [gcc|clang]
#
# The default compiler is clang for the asan preset, gcc otherwise. The working
# tree is mounted read-only at /src and copied to /work inside the container, so
# a run never writes into the source tree or into the macOS build directories.
# The container runs on the host's native architecture (no --platform): an
# emulated sanitizer run is slow and unrepresentative. The x86_64 Linux proof is
# the ubuntu-26.04 CI runner.
#
# Environment:
#   GBL_LINUX_IMAGE  override the pinned image reference
#
# Exit codes: the container's status; 2 for a usage error (before Docker runs).

# Pinned 2026-11-10: multi-architecture index digest of ubuntu:26.04.
GBL_LINUX_IMAGE=${GBL_LINUX_IMAGE:-ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c}

usage() {
    echo "linux-lane: usage: linux-lane.sh <workflow-preset> [gcc|clang]" >&2
    exit 2
}

[ "$#" -ge 1 ] && [ "$#" -le 2 ] || usage
preset=$1
case "$preset" in
    '' | *[!a-z0-9_-]*) usage ;;
esac
if [ "$#" -eq 2 ]; then
    compiler=$2
elif [ "$preset" = asan ]; then
    compiler=clang
else
    compiler=gcc
fi
case "$compiler" in
    gcc) packages="gcc"; cc_name=gcc; cc_id=GNU ;;
    clang) packages="clang-21 libclang-rt-21-dev"; cc_name=clang-21; cc_id=Clang ;;
    *) usage ;;
esac

root=$(cd "$(dirname "$0")/.." && pwd) || exit 1

exec docker run --rm \
    -e "GBL_PRESET=$preset" \
    -e "GBL_PACKAGES=$packages" \
    -e "CC=$cc_name" \
    -e "GBL_EXPECT_C_ID=$cc_id" \
    -v "$root":/src:ro -w /work \
    "$GBL_LINUX_IMAGE" \
    sh -c '
set -eu
export DEBIAN_FRONTEND=noninteractive
echo "linux-lane: preset ${GBL_PRESET}, compiler ${CC}, arch $(uname -m)"
apt-get update -qq
apt-get install -y -qq --no-install-recommends ca-certificates cmake ninja-build git libc6-dev $GBL_PACKAGES >/dev/null
mkdir -p /work
cp -R /src/. /work
rm -rf /work/build
git config --global --add safe.directory /work
cmake --version | head -n 1
"$CC" --version | head -n 1
cd /work
cmake --workflow --preset "$GBL_PRESET"
'
