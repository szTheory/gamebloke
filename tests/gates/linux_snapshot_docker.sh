#!/bin/sh
# Offline Docker witness for linux_snapshot.cmake. It only examines synthetic
# fixture files and signals the helper PID recorded by that test's launcher.
set -eu
fail() { echo "linux-snapshot-stub: $1" >&2; exit 71; }

if [ "$1" = image ]; then
    [ "$#" -eq 3 ] && [ "$2" = inspect ] || fail image-inspect
    [ "$3" = ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c ] || fail image-identity
    exit 0
fi

if [ "$1" = rm ]; then
    [ "$#" -eq 3 ] && [ "$2" = --force ] || fail container-cleanup
    [ "$3" = "$GBL_STUB_CONTAINER_ID" ] || fail wrong-container-cleanup
    printf '%s\n' "$3" > "$GBL_STUB_CONTAINER_CLEANUP"
    exit 0
fi

if [ "$1" = start ]; then
    [ "$#" -eq 3 ] && [ "$2" = --attach ] || fail container-start
    [ "$3" = "$GBL_STUB_CONTAINER_ID" ] || fail wrong-container-start
    case "$GBL_STUB_ACTION" in
      success) exit 0 ;;
      failure) exit 37 ;;
      HUP|INT|TERM)
        helper_pid=$(cat "$GBL_STUB_PID")
        case "$helper_pid" in ''|*[!0-9]*) fail helper-pid ;; esac
        kill -s "$GBL_STUB_ACTION" "$helper_pid" || fail signal
        exit 0 ;;
      *) fail action ;;
    esac
fi

[ "$1" = create ] || fail operation
shift
[ "$1" = --name ] || fail container-name
container_name=$2
case "$container_name" in gamebloke-linux-lane-ci-*) ;; *) fail identifiable-container-name ;; esac
shift 2
[ "$1" = --label ] && [ "$2" = gamebloke.project=gamebloke ] || fail project-label
shift 2
[ "$1" = --label ] && [ "$2" = gamebloke.task=linux-lane-ci ] || fail task-label
shift 2
[ "$1" = --cpus ] && [ "$2" = 4 ] || fail cpu-bound
shift 2
[ "$1" = --memory ] && [ "$2" = 8g ] || fail memory-bound
shift 2
[ "$1" = -e ] && [ "$2" = GBL_PRESET=ci ] || fail preset
shift 2
[ "$1" = -e ] && [ "$2" = GBL_PACKAGES=gcc ] || fail packages
shift 2
[ "$1" = -e ] && [ "$2" = CC=gcc ] || fail compiler
shift 2
[ "$1" = -e ] && [ "$2" = GBL_EXPECT_C_ID=GNU ] || fail compiler-identity
shift 2
[ "$1" = -v ] || fail source-mount
mount=$2
case "$mount" in *:/src:ro) snapshot=${mount%:/src:ro} ;; *) fail readonly-source ;; esac
printf '%s\n' "$snapshot" > "$GBL_STUB_MOUNT"
shift 2
[ "$1" = -w ] && [ "$2" = /work ] || fail working-directory
shift 2
[ "$1" = ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c ] || fail image-identity
shift
[ "$1" = sh ] && [ "$2" = -c ] || fail shell
printf '%s\n' "$3" | grep -F 'cmake --workflow --preset "$GBL_PRESET"' >/dev/null || fail canonical-workflow
if [ "$GBL_STUB_ACTION" = name-collision ]; then
    echo "linux-snapshot-stub: simulated container-name collision" >&2
    exit 125
fi

# Exact argument validation also rejects any --platform override.
[ ! -e "$snapshot/private" ] && [ ! -e "$snapshot/build-old" ] || fail ignored-content
[ ! -e "$snapshot/deleted.txt" ] || fail working-deletion
[ "$(cat "$snapshot/tracked.txt")" = working-edit ] || fail working-edit
[ "$(cat "$snapshot/untracked name.txt")" = unignored-new-file ] || fail unignored-file
[ -x "$snapshot/mode.sh" ] || fail working-mode
[ -L "$snapshot/dangling-owned-link" ] && [ ! -e "$snapshot/dangling-owned-link" ] || fail symlink-dereference
[ "$(git -C "$snapshot" rev-parse HEAD)" = "$GBL_STUB_HEAD" ] || fail head
[ "$(git -C "$snapshot" rev-list --count HEAD)" = 2 ] || fail history-depth
git -C "$snapshot" merge-base --is-ancestor "$GBL_STUB_BASE" HEAD || fail ancestor
[ "$(git -C "$snapshot" show "$GBL_STUB_BASE:ancestor-only.txt")" = original-ancestor-bytes ] || fail ancestor-bytes
printf '%s\n' "$GBL_STUB_CONTAINER_ID"
