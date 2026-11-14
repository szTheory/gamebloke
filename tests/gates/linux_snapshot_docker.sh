#!/bin/sh
# Offline Docker witness for linux_snapshot.cmake. It only examines synthetic
# fixture files and signals the helper PID recorded by that test's launcher.
set -eu
fail() { echo "linux-snapshot-stub: $1" >&2; exit 71; }
[ "$#" -eq 22 ] || fail argument-count
[ "$1" = run ] && [ "$2" = --rm ] || fail lifecycle
[ "$3" = --cpus ] && [ "$4" = 4 ] || fail cpu-bound
[ "$5" = --memory ] && [ "$6" = 8g ] || fail memory-bound
[ "$7" = -e ] && [ "$8" = GBL_PRESET=ci ] || fail preset
[ "$9" = -e ] || fail package-option
shift 9
[ "$1" = GBL_PACKAGES=gcc ] && [ "$2" = -e ] && [ "$3" = CC=gcc ] || fail compiler
[ "$4" = -e ] && [ "$5" = GBL_EXPECT_C_ID=GNU ] || fail compiler-identity
[ "$6" = -v ] || fail source-mount
mount=$7
case "$mount" in *:/src:ro) snapshot=${mount%:/src:ro} ;; *) fail readonly-source ;; esac
printf '%s\n' "$snapshot" > "$GBL_STUB_MOUNT"
[ "$8" = -w ] && [ "$9" = /work ] || fail working-directory
shift 9
[ "$1" = ubuntu:26.04@sha256:6a06111c3b0a8600feadb270aec771dc239bf407b25625f93f44ab00e11f4d3c ] || fail image-identity
[ "$2" = sh ] && [ "$3" = -c ] || fail shell
printf '%s\n' "$4" | grep -F 'cmake --workflow --preset "$GBL_PRESET"' >/dev/null || fail canonical-workflow
# Exact argument positions also reject any --platform override.
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
