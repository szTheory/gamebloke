#!/bin/sh
set -eu

MODE=${1:---check}
case "$MODE" in
    --snapshot|--check|--negative-controls|--capture-controls|--phase-resolution-controls) ;;
    *) printf '%s\n' 'usage: sh scripts/check-phase23-playstead-fit.sh [--snapshot|--check|--negative-controls|--capture-controls|--phase-resolution-controls]' >&2; exit 2 ;;
esac

SCRIPT_DIR=$(CDPATH= cd -P "$(dirname "$0")" && pwd -P)
REPO_ROOT=$(CDPATH= cd -P "$SCRIPT_DIR/.." && pwd -P)
PLAYSTEAD_ROOT="$REPO_ROOT/../playstead/playstead-mac"
PHASE_NAME=23-public-api-host-session-and-playstead-fit
PLANNING_ROOT="$REPO_ROOT/.planning"
CITED_PATHS='docs/SUPPORT-MATRIX.md
docs/STATE-SUPPORT.md
Playstead/Adapter/AdapterHost.swift
Playstead/Adapter/AdapterCatalog.swift
Playstead/Adapter/StateSupportMatrix.swift
AdapterRuntime/ManagedMGBA/PROTOCOL.md'
CAPABILITIES='Lifecycle
Timed input
Cycle budget
Raw video and audio
Teardown
Saves'

fail()
{
    printf 'phase23-fit: %s\n' "$*" >&2
    exit 1
}

phase_dir_for()
{
    phase_root=$1
    active_phase_dir="$phase_root/phases/$PHASE_NAME"
    archived_phase_dir="$phase_root/milestones/v6.0-phases/$PHASE_NAME"
    if [ -d "$active_phase_dir" ] && [ -d "$archived_phase_dir" ]; then
        return 2
    fi
    if [ -d "$active_phase_dir" ]; then
        printf '%s\n' "$active_phase_dir"
        return 0
    fi
    if [ -d "$archived_phase_dir" ]; then
        printf '%s\n' "$archived_phase_dir"
        return 0
    fi
    return 1
}

PHASE_DIR=$(phase_dir_for "$PLANNING_ROOT") ||
    fail 'Phase 23 report directory is missing or ambiguous in active and v6.0 archive paths'
REPORT="$PHASE_DIR/23-PLAYSTEAD-FIT.md"

for command_name in git awk sed sort shasum mktemp cmp wc tr grep ln python3; do
    command -v "$command_name" >/dev/null 2>&1 || fail "required command is unavailable: $command_name"
done

[ -d "$PLAYSTEAD_ROOT" ] || fail 'Playstead checkout is missing'
PLAYSTEAD_ROOT=$(CDPATH= cd -P "$PLAYSTEAD_ROOT" && pwd -P) || fail 'Playstead checkout cannot be resolved'
PLAYSTEAD_GIT_ROOT=$(git --no-optional-locks -C "$PLAYSTEAD_ROOT" rev-parse --show-toplevel 2>/dev/null) ||
    fail 'Playstead checkout is not a readable Git worktree'
PLAYSTEAD_GIT_ROOT=$(CDPATH= cd -P "$PLAYSTEAD_GIT_ROOT" && pwd -P) || fail 'Playstead Git root cannot be resolved'
case "$PLAYSTEAD_ROOT" in
    "$PLAYSTEAD_GIT_ROOT"|"$PLAYSTEAD_GIT_ROOT"/*) ;;
    *) fail 'Playstead checkout resolves outside its Git root' ;;
esac
PLAYSTEAD_PREFIX=$(git --no-optional-locks -C "$PLAYSTEAD_ROOT" rev-parse --show-prefix 2>/dev/null) ||
    fail 'Playstead source prefix cannot be resolved'
case "$PLAYSTEAD_PREFIX" in
    /*|*../*) fail 'Playstead source prefix is unsafe' ;;
esac

umask 077
TEMP_BASE=${TMPDIR:-/tmp}
TEMP_ROOT=$(mktemp -d "$TEMP_BASE/gamebloke-phase23-fit.XXXXXX") ||
    fail 'cannot create private checker scratch'
cleanup()
{
    case "$TEMP_ROOT" in "$TEMP_BASE"/gamebloke-phase23-fit.*) rm -rf "$TEMP_ROOT" ;; esac
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
PRESERVE_SNAPSHOT_CAPTURES=0

sha256_file()
{
    shasum -a 256 "$1" | awk '{ print $1 }'
}

capture_file_no_follow()
{
    capture_root=$1
    capture_relative=$2
    capture_output=$3
    python3 - "$capture_root" "$capture_relative" "$capture_output" <<'PY'
import os
import signal
import stat
import sys

root, relative, destination = sys.argv[1:]
if (not hasattr(os, "O_NOFOLLOW") or not hasattr(os, "O_DIRECTORY")
        or not hasattr(os, "O_NONBLOCK") or not hasattr(signal, "SIGALRM")
        or os.open not in os.supports_dir_fd):
    raise SystemExit("safe captured-copy support is unavailable (requires O_NOFOLLOW, O_DIRECTORY, O_NONBLOCK, SIGALRM, and openat)")

parts = relative.split("/")
if not parts or any(part in ("", ".", "..") for part in parts):
    raise SystemExit("unsafe relative source path")

directory_flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | getattr(os, "O_CLOEXEC", 0)
file_flags = os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | getattr(os, "O_CLOEXEC", 0)
source_fd = None
parent_fd = None
destination_fd = None
created = False
try:
    # The caller supplies a physically resolved checkout root. Keep each child
    # directory pinned by fd so a later symlink/path swap cannot redirect openat.
    parent_fd = os.open(root, directory_flags)
    for component in parts[:-1]:
        next_fd = os.open(component, directory_flags, dir_fd=parent_fd)
        os.close(parent_fd)
        parent_fd = next_fd

    # O_NONBLOCK makes a raced FIFO fail promptly instead of waiting for a
    # writer. Keep a short alarm as a backstop if a filesystem open stalls.
    previous_alarm_handler = signal.getsignal(signal.SIGALRM)
    def open_timeout(_signum, _frame):
        raise TimeoutError("source open exceeded three seconds")
    signal.signal(signal.SIGALRM, open_timeout)
    signal.alarm(3)
    try:
        source_fd = os.open(parts[-1], file_flags, dir_fd=parent_fd)
    finally:
        signal.alarm(0)
        signal.signal(signal.SIGALRM, previous_alarm_handler)
    before = os.fstat(source_fd)
    if not stat.S_ISREG(before.st_mode):
        raise OSError("source is not a regular file")

    destination_fd = os.open(
        destination,
        os.O_WRONLY | os.O_CREAT | os.O_EXCL | getattr(os, "O_CLOEXEC", 0),
        0o600,
    )
    created = True
    copied = 0
    while True:
        block = os.read(source_fd, 1024 * 1024)
        if not block:
            break
        view = memoryview(block)
        while view:
            written = os.write(destination_fd, view)
            view = view[written:]
        copied += len(block)

    after = os.fstat(source_fd)
    stable_before = (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns)
    stable_after = (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns)
    if copied != before.st_size or stable_before != stable_after:
        raise OSError("source changed while its captured copy was read")
except Exception as error:
    if created:
        try:
            os.unlink(destination)
        except OSError:
            pass
    print("safe captured copy failed: {}".format(error), file=sys.stderr)
    raise SystemExit(1)
finally:
    for descriptor in (destination_fd, source_fd, parent_fd):
        if descriptor is not None:
            try:
                os.close(descriptor)
            except OSError:
                pass
PY
}

valid_sha256()
{
    digest_value=$1
    [ "${#digest_value}" -eq 64 ] || return 1
    case "$digest_value" in *[!0-9a-f]*) return 1 ;; esac
}

snapshot_equal()
{
    cmp -s "$1" "$2"
}

worktree_snapshot()
{
    snapshot_capture_dir=$(mktemp -d "$TEMP_ROOT/snapshot-captures.XXXXXX") ||
        fail 'cannot create private source-capture directory'
    SNAPSHOT_CAPTURE_DIR=$snapshot_capture_dir
    SNAPSHOT_CAPTURE_MANIFEST="$snapshot_capture_dir/manifest"
    : > "$SNAPSHOT_CAPTURE_MANIFEST"
    snapshot_status_file=$(mktemp "$TEMP_ROOT/status.XXXXXX") || fail 'cannot create status scratch'
    snapshot_head=$(git --no-optional-locks -C "$PLAYSTEAD_ROOT" rev-parse --verify HEAD 2>/dev/null) ||
        fail 'Playstead HEAD cannot be resolved'
    case "$snapshot_head" in
        *[!0-9a-f]*|'') fail 'Playstead HEAD is not a hexadecimal object ID' ;;
    esac
    git --no-optional-locks -C "$PLAYSTEAD_ROOT" status --porcelain=v1 --untracked-files=all \
        > "$snapshot_status_file" 2>/dev/null || fail 'Playstead status cannot be read'
    if [ -s "$snapshot_status_file" ]; then
        snapshot_worktree=dirty
    else
        snapshot_worktree=clean
    fi
    snapshot_status_sha=$(LC_ALL=C sort "$snapshot_status_file" | shasum -a 256 | awk '{ print $1 }')
    valid_sha256 "$snapshot_status_sha" || fail 'Playstead status fingerprint is not SHA-256'
    rm -f "$snapshot_status_file"
    printf 'head=%s\n' "$snapshot_head"
    printf 'source_prefix=%s\n' "$PLAYSTEAD_PREFIX"
    printf 'worktree=%s\n' "$snapshot_worktree"
    printf 'status_sha256=%s\n' "$snapshot_status_sha"
    snapshot_index=0
    for snapshot_path in $CITED_PATHS; do
        git --no-optional-locks -C "$PLAYSTEAD_ROOT" ls-files --error-unmatch -- "$snapshot_path" \
            >/dev/null 2>&1 || fail "cited Playstead path is not tracked: $snapshot_path"
        snapshot_file="$PLAYSTEAD_ROOT/$snapshot_path"
        [ -f "$snapshot_file" ] && [ ! -L "$snapshot_file" ] ||
            fail "cited Playstead path is missing or ambiguous: $snapshot_path"
        snapshot_porcelain=$(git --no-optional-locks -C "$PLAYSTEAD_ROOT" \
            status --porcelain=v1 --untracked-files=all -- "$snapshot_path" 2>/dev/null) ||
            fail "cited Playstead status cannot be read: $snapshot_path"
        if [ -z "$snapshot_porcelain" ]; then
            snapshot_xy=..
        else
            snapshot_xy=$(printf '%s\n' "$snapshot_porcelain" | awk 'NR == 1 { value = substr($0, 1, 2); gsub(/ /, ".", value); print value }')
            [ -n "$snapshot_xy" ] || fail "cited Playstead status is ambiguous: $snapshot_path"
        fi
        snapshot_index=$((snapshot_index + 1))
        snapshot_capture="$snapshot_capture_dir/source.$snapshot_index"
        capture_file_no_follow "$PLAYSTEAD_ROOT" "$snapshot_path" "$snapshot_capture" ||
            fail "cited Playstead path cannot be safely captured: $snapshot_path"
        snapshot_digest=$(sha256_file "$snapshot_capture")
        valid_sha256 "$snapshot_digest" || fail "cited Playstead file digest is not SHA-256: $snapshot_path"
        printf '%s|%s\n' "$snapshot_path" "$snapshot_capture" >> "$SNAPSHOT_CAPTURE_MANIFEST"
        printf 'file=%s|%s|%s\n' "$snapshot_path" "$snapshot_xy" "$snapshot_digest"
    done
    if [ "$PRESERVE_SNAPSHOT_CAPTURES" != 1 ]; then
        rm -rf "$snapshot_capture_dir"
    fi
}

read_snapshot_block()
{
    snapshot_report=$1
    snapshot_out=$2
    [ -f "$snapshot_report" ] && [ ! -L "$snapshot_report" ] || fail 'fit report is missing or ambiguous'
    [ "$(grep -Fxc '<!-- PLAYSTEAD_SNAPSHOT_BEGIN -->' "$snapshot_report" || true)" -eq 1 ] ||
        fail 'fit report must contain one snapshot start marker'
    [ "$(grep -Fxc '<!-- PLAYSTEAD_SNAPSHOT_END -->' "$snapshot_report" || true)" -eq 1 ] ||
        fail 'fit report must contain one snapshot end marker'
    awk '
        /<!-- PLAYSTEAD_SNAPSHOT_BEGIN -->/ { inside = 1; next }
        /<!-- PLAYSTEAD_SNAPSHOT_END -->/ { inside = 0; next }
        inside { print }
    ' "$snapshot_report" > "$snapshot_out"
}

snapshot_value()
{
    snapshot_file=$1
    snapshot_key=$2
    awk -F= -v key="$snapshot_key" '$1 == key { count++; value = substr($0, index($0, "=") + 1) } END { if (count == 1) print value; else exit 1 }' "$snapshot_file"
}

check_snapshot_block()
{
    checked_report=$1
    current_snapshot=$2
    report_snapshot="$TEMP_ROOT/report-snapshot.$$.txt"
    read_snapshot_block "$checked_report" "$report_snapshot"
    expected_head=$(snapshot_value "$current_snapshot" head) || fail 'fit report snapshot structure is invalid (head)'
    report_head=$(snapshot_value "$report_snapshot" head) || fail 'fit report snapshot structure is invalid (head)'
    [ "$report_head" = "$expected_head" ] || fail 'Playstead HEAD mismatch'
    expected_prefix=$(snapshot_value "$current_snapshot" source_prefix) || fail 'current source prefix is invalid'
    report_prefix=$(snapshot_value "$report_snapshot" source_prefix) || fail 'fit report snapshot structure is invalid (source prefix)'
    [ "$report_prefix" = "$expected_prefix" ] || fail 'Playstead source prefix mismatch'
    expected_worktree=$(snapshot_value "$current_snapshot" worktree) || fail 'current worktree state is invalid'
    report_worktree=$(snapshot_value "$report_snapshot" worktree) || fail 'fit report snapshot structure is invalid (worktree)'
    [ "$report_worktree" = "$expected_worktree" ] || fail 'Playstead working-tree state mismatch'
    expected_status_sha=$(snapshot_value "$current_snapshot" status_sha256) || fail 'current status fingerprint is invalid'
    report_status_sha=$(snapshot_value "$report_snapshot" status_sha256) || fail 'fit report snapshot structure is invalid (status fingerprint)'
    [ "$report_status_sha" = "$expected_status_sha" ] || fail 'Playstead status fingerprint mismatch'
    for expected_path in $CITED_PATHS; do
        expected_row=$(awk -F'|' -v path="$expected_path" '$1 == "file=" path { count++; row = $0 } END { if (count == 1) print row; else exit 1 }' "$current_snapshot") ||
            fail "current cited-file identity is invalid: $expected_path"
        report_row=$(awk -F'|' -v path="$expected_path" '$1 == "file=" path { count++; row = $0 } END { if (count == 1) print row; else exit 1 }' "$report_snapshot") ||
            fail "fit report cited-file row is missing or duplicated: $expected_path"
        [ "$report_row" = "$expected_row" ] || fail "cited-file status or content mismatch: $expected_path"
    done
    expected_rows=$(printf '%s\n' "$CITED_PATHS" | wc -l | tr -d ' ')
    report_rows=$(awk -F'|' '$1 ~ /^file=/ { count++ } END { print count + 0 }' "$report_snapshot")
    [ "$report_rows" -eq "$expected_rows" ] || fail 'fit report snapshot has extra or missing cited-file rows'
    snapshot_equal "$report_snapshot" "$current_snapshot" || fail 'fit report snapshot fields are missing, extra, or out of order'
}

line_range_valid()
{
    cited_file=$1
    cited_range=$2
    case "$cited_range" in
        *-*) cited_start=${cited_range%%-*}; cited_end=${cited_range#*-} ;;
        *) return 1 ;;
    esac
    case "$cited_start$cited_end" in *[!0-9]*|'') return 1 ;; esac
    [ "$cited_start" -ge 1 ] && [ "$cited_end" -ge "$cited_start" ] || return 1
    cited_lines=$(wc -l < "$cited_file" | tr -d ' ')
    [ "$cited_end" -le "$cited_lines" ]
}

trim()
{
    printf '%s' "$1" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//'
}

validate_citation_cell()
{
    citation_cell=$1
    citation_tree=$2
    [ -n "$(trim "$citation_cell")" ] || fail 'capability row has an empty citation cell'
    citation_count=0
    old_ifs=$IFS
    IFS=';'
    for citation_item in $citation_cell; do
        citation_item=$(trim "$citation_item")
        case "$citation_item" in *\#*) ;;
            *) fail "invalid citation syntax: $citation_item" ;;
        esac
        citation_path=${citation_item%%#*}
        citation_range=${citation_item#*#}
        case "$citation_path" in
            ''|/*|*..*|*\\*) fail "unsafe citation path: $citation_path" ;;
        esac
        if [ "$citation_tree" = gamebloke ]; then
            case "$citation_path" in
                docs/core-api-v1.md|include/gamebloke/gamebloke_v1.h|scripts/check-phase23-host-session.sh|examples/phase23-host-session/consumer.c) ;;
                *) fail "Gamebloke citation is outside the approved source set: $citation_path" ;;
            esac
            citation_root=$REPO_ROOT
        else
            case "$citation_path" in
                docs/SUPPORT-MATRIX.md|docs/STATE-SUPPORT.md|Playstead/Adapter/AdapterHost.swift|Playstead/Adapter/AdapterCatalog.swift|Playstead/Adapter/StateSupportMatrix.swift|AdapterRuntime/ManagedMGBA/PROTOCOL.md) ;;
                *) fail "Playstead citation is outside the captured source set: $citation_path" ;;
            esac
            snapshot_match=$(awk -F'|' -v path="$citation_path" '$1 == "file=" path { count++; row = $0 } END { if (count == 1) print row; else exit 1 }' "$TEMP_ROOT/validated-snapshot.txt") ||
                fail "Playstead citation is not bound to the snapshot: $citation_path"
            [ -n "$snapshot_match" ] || fail "Playstead citation is not bound to the snapshot: $citation_path"
            citation_capture=$(awk -F'|' -v path="$citation_path" '$1 == path { count++; captured = $2 } END { if (count == 1) print captured; else exit 1 }' "$SNAPSHOT_CAPTURE_MANIFEST") ||
                fail "Playstead citation has no unique captured source: $citation_path"
        fi
        if [ "$citation_tree" = gamebloke ]; then
            citation_capture=$(mktemp "$TEMP_ROOT/citation.XXXXXX") ||
                fail 'cannot reserve private citation capture path'
            rm -f "$citation_capture"
            capture_file_no_follow "$citation_root" "$citation_path" "$citation_capture" ||
                fail "cited path cannot be safely captured: $citation_path"
        fi
        line_range_valid "$citation_capture" "$citation_range" || fail "citation line range invalid: $citation_path#$citation_range"
        if [ "$citation_tree" = gamebloke ]; then
            rm -f "$citation_capture"
        fi
        citation_count=$((citation_count + 1))
    done
    IFS=$old_ifs
    [ "$citation_count" -gt 0 ] || fail 'capability row has an empty citation cell'
}

validate_mapping()
{
    mapping_report=$1
    mapping_snapshot=$2
    for required_section in Scope 'Gamebloke Evidence' 'Playstead Snapshot' 'Capability Mapping' 'Unsupported Behavior and Unknowns'; do
        [ "$(grep -Fxc "## $required_section" "$mapping_report" || true)" -eq 1 ] ||
            fail "fit report is missing or duplicates section: $required_section"
    done
    section_order=$(awk '/^## Scope$/ { a = NR } /^## Gamebloke Evidence$/ { b = NR } /^## Playstead Snapshot$/ { c = NR } /^## Capability Mapping$/ { d = NR } /^## Unsupported Behavior and Unknowns$/ { e = NR } END { if (a < b && b < c && c < d && d < e) print "valid" }' "$mapping_report")
    [ "$section_order" = valid ] || fail 'fit report sections are out of order'
    [ "$(grep -Fxc '<!-- CAPABILITY_MAPPING_BEGIN -->' "$mapping_report" || true)" -eq 1 ] ||
        fail 'fit report must contain one capability mapping start marker'
    [ "$(grep -Fxc '<!-- CAPABILITY_MAPPING_END -->' "$mapping_report" || true)" -eq 1 ] ||
        fail 'fit report must contain one capability mapping end marker'
    awk '/<!-- CAPABILITY_MAPPING_BEGIN -->/ { inside = 1; next } /<!-- CAPABILITY_MAPPING_END -->/ { inside = 0; next } inside { print }' \
        "$mapping_report" > "$TEMP_ROOT/mapping.txt"
    cp "$mapping_snapshot" "$TEMP_ROOT/validated-snapshot.txt"
    actual_labels=$(awk -F'|' '/^\|/ {
        label = $2; gsub(/^[[:space:]]+|[[:space:]]+$/, "", label)
        if (label == "Lifecycle" || label == "Timed input" || label == "Cycle budget" || label == "Raw video and audio" || label == "Teardown" || label == "Saves") print label
    }' "$TEMP_ROOT/mapping.txt")
    expected_labels=$(printf '%s\n' "$CAPABILITIES")
    [ "$actual_labels" = "$expected_labels" ] ||
        fail 'capability rows must occur exactly once in the required order'
    data_rows=$(awk -F'|' '/^\|/ {
        label = $2; gsub(/^[[:space:]]+|[[:space:]]+$/, "", label)
        if (label != "Capability" && label !~ /^[-:]+$/) count++
    } END { print count + 0 }' "$TEMP_ROOT/mapping.txt")
    [ "$data_rows" -eq 6 ] || fail 'capability mapping must contain exactly six rows'
    for capability in Lifecycle 'Timed input' 'Cycle budget' 'Raw video and audio' Teardown Saves; do
        capability_cells=$(awk -F'|' -v target="$capability" '/^\|/ {
            label = $2; gsub(/^[[:space:]]+|[[:space:]]+$/, "", label)
            if (label == target) {
                for (i = 3; i <= 5; i++) { value = $i; gsub(/^[[:space:]]+|[[:space:]]+$/, "", value); print value }
                count++
            }
        } END { if (count != 1) exit 1 }' "$TEMP_ROOT/mapping.txt") ||
            fail "capability row must occur exactly once: $capability"
        fit_cell=$(printf '%s\n' "$capability_cells" | sed -n '1p')
        gbl_cell=$(printf '%s\n' "$capability_cells" | sed -n '2p')
        play_cell=$(printf '%s\n' "$capability_cells" | sed -n '3p')
        case "$(printf '%s' "$fit_cell" | tr '[:upper:]' '[:lower:]')" in
            fit:*|gap:*|partial:*|unknown:*) ;;
            *) fail "capability row does not declare a fit or gap: $capability" ;;
        esac
        [ -n "$gbl_cell" ] && [ -n "$play_cell" ] || fail "capability row has an empty source citation: $capability"
        validate_citation_cell "$gbl_cell" gamebloke
        validate_citation_cell "$play_cell" playstead
    done
    for cited_path in $CITED_PATHS; do
        awk -F'|' -v path="$cited_path" '/^\|/ { if (index($0, path "#") > 0) found = 1 } END { exit !found }' "$TEMP_ROOT/mapping.txt" ||
            fail "Playstead source is not cited by the capability mapping: $cited_path"
    done
}

validate_report_citations()
{
    citations_report=$1
    awk '
        /<!-- PLAYSTEAD_SNAPSHOT_BEGIN -->/ { snapshot = 1; next }
        /<!-- PLAYSTEAD_SNAPSHOT_END -->/ { snapshot = 0; next }
        !snapshot { print }
    ' "$citations_report" > "$TEMP_ROOT/report-without-snapshot.txt"
    citation_tokens=$(grep -Eo '(docs/[A-Za-z0-9._/-]+|include/gamebloke/[A-Za-z0-9._/-]+|scripts/[A-Za-z0-9._/-]+|examples/phase23-host-session/[A-Za-z0-9._/-]+|Playstead/Adapter/[A-Za-z0-9._/-]+|AdapterRuntime/ManagedMGBA/[A-Za-z0-9._/-]+)#[0-9]+-[0-9]+' \
        "$TEMP_ROOT/report-without-snapshot.txt" || true)
    [ -n "$citation_tokens" ] || fail 'fit report contains no precise source citations'
    for citation_token in $citation_tokens; do
        citation_path=${citation_token%%#*}
        case "$citation_path" in
            docs/SUPPORT-MATRIX.md|docs/STATE-SUPPORT.md|Playstead/Adapter/AdapterHost.swift|Playstead/Adapter/AdapterCatalog.swift|Playstead/Adapter/StateSupportMatrix.swift|AdapterRuntime/ManagedMGBA/PROTOCOL.md)
                validate_citation_cell "$citation_token" playstead
                ;;
            *) validate_citation_cell "$citation_token" gamebloke ;;
        esac
    done
}

check_report_file()
{
    checked_path=$1
    before_snapshot="$TEMP_ROOT/snapshot-before.$$.txt"
    after_snapshot="$TEMP_ROOT/snapshot-after.$$.txt"
    PRESERVE_SNAPSHOT_CAPTURES=1
    worktree_snapshot > "$before_snapshot"
    PRESERVE_SNAPSHOT_CAPTURES=0
    validation_capture_dir=$SNAPSHOT_CAPTURE_DIR
    check_snapshot_block "$checked_path" "$before_snapshot"
    validate_mapping "$checked_path" "$before_snapshot"
    validate_report_citations "$checked_path"
    worktree_snapshot > "$after_snapshot"
    snapshot_equal "$before_snapshot" "$after_snapshot" || fail 'stale-snapshot: Playstead changed during source capture or validation'
    rm -rf "$validation_capture_dir"
}

must_reject()
{
    control_report=$1
    expected_diagnostic=$2
    control_name=$3
    if (check_report_file "$control_report") > "$TEMP_ROOT/$control_name.log" 2>&1; then
        fail "negative control unexpectedly passed: $control_name"
    fi
    grep -F "$expected_diagnostic" "$TEMP_ROOT/$control_name.log" >/dev/null || {
        cat "$TEMP_ROOT/$control_name.log" >&2
        fail "negative control missed its diagnostic: $control_name"
    }
}

verify_symlinked_parent_controls()
{
    mkdir -p "$TEMP_ROOT/outside-gamebloke/include/gamebloke" \
        "$TEMP_ROOT/outside-playstead/docs"
    : > "$TEMP_ROOT/outside-gamebloke/include/gamebloke/gamebloke_v1.h"
    : > "$TEMP_ROOT/outside-playstead/docs/SUPPORT-MATRIX.md"
    ln -s "$TEMP_ROOT/outside-gamebloke" "$TEMP_ROOT/gamebloke-parent-link"
    ln -s "$TEMP_ROOT/outside-playstead" "$TEMP_ROOT/playstead-parent-link"
    if capture_file_no_follow "$TEMP_ROOT" \
        "gamebloke-parent-link/include/gamebloke/gamebloke_v1.h" \
        "$TEMP_ROOT/gamebloke-parent-capture"; then
        fail 'Gamebloke symlink-parent control escaped the checkout boundary'
    fi
    if capture_file_no_follow "$TEMP_ROOT" \
        "playstead-parent-link/docs/SUPPORT-MATRIX.md" \
        "$TEMP_ROOT/playstead-parent-capture"; then
        fail 'Playstead symlink-parent control escaped the Git root boundary'
    fi
}

verify_captured_copy_stability()
{
    capture_control_root="$TEMP_ROOT/capture-replacement/source"
    capture_control_outside="$TEMP_ROOT/capture-replacement/outside"
    capture_control_file="$capture_control_root/docs/SUPPORT-MATRIX.md"
    capture_control_copy="$TEMP_ROOT/capture-replacement/captured-source"
    mkdir -p "$capture_control_root/docs" "$capture_control_outside"
    printf 'inside line one\ninside line two\n' > "$capture_control_file"
    printf 'replacement line one\nreplacement line two\nreplacement line three\n' \
        > "$capture_control_outside/SUPPORT-MATRIX.md"
    capture_control_digest=$(sha256_file "$capture_control_file")
    capture_file_no_follow "$capture_control_root" "docs/SUPPORT-MATRIX.md" \
        "$capture_control_copy" || fail 'cannot create captured-copy replacement control'
    mv "$capture_control_root/docs" "$capture_control_root/docs-original"
    ln -s "$capture_control_outside" "$capture_control_root/docs"
    [ "$(sha256_file "$capture_control_copy")" = "$capture_control_digest" ] ||
        fail 'later source-path replacement changed the captured bytes'
    line_range_valid "$capture_control_copy" 1-2 ||
        fail 'captured-copy line-range control rejected the original bytes'
    if line_range_valid "$capture_control_copy" 1-3; then
        fail 'captured-copy line-range control read the replacement source path'
    fi
    if capture_file_no_follow "$capture_control_root" "docs/SUPPORT-MATRIX.md" \
        "$TEMP_ROOT/replacement-capture"; then
        fail 'no-follow capture accepted a later symlink replacement'
    fi

    python3 -c 'import os, sys; os.mkfifo(sys.argv[1])' "$TEMP_ROOT/capture-fifo" ||
        fail 'cannot create captured-copy FIFO control'
    if capture_file_no_follow "$TEMP_ROOT" "capture-fifo" \
        "$TEMP_ROOT/fifo-capture" > "$TEMP_ROOT/fifo-stdout" 2> "$TEMP_ROOT/fifo-stderr"; then
        fail 'captured-copy control accepted a FIFO'
    fi
    grep -q 'source is not a regular file' "$TEMP_ROOT/fifo-stderr" ||
        fail 'captured-copy control did not reject a FIFO promptly as a non-regular file'
    [ ! -e "$TEMP_ROOT/fifo-capture" ] ||
        fail 'captured-copy FIFO refusal left a partial destination'
}

verify_phase_dir_resolution_controls()
{
    resolution_root="$TEMP_ROOT/phase-directory-resolution"
    active_root="$resolution_root/active-only"
    archived_root="$resolution_root/archived-only"
    both_root="$resolution_root/both"
    empty_root="$resolution_root/empty"

    mkdir -p "$active_root/phases/$PHASE_NAME"
    [ "$(phase_dir_for "$active_root")" = "$active_root/phases/$PHASE_NAME" ] ||
        fail 'phase path resolver did not select the active phase directory'

    mkdir -p "$archived_root/milestones/v6.0-phases/$PHASE_NAME"
    [ "$(phase_dir_for "$archived_root")" = "$archived_root/milestones/v6.0-phases/$PHASE_NAME" ] ||
        fail 'phase path resolver did not select the archived phase directory'

    mkdir -p "$both_root/phases/$PHASE_NAME" \
        "$both_root/milestones/v6.0-phases/$PHASE_NAME"
    if phase_dir_for "$both_root"; then
        fail 'phase path resolver accepted ambiguous active and archived directories'
    fi

    mkdir -p "$empty_root"
    if phase_dir_for "$empty_root"; then
        fail 'phase path resolver accepted a missing phase directory'
    fi
}

if [ "$MODE" = --snapshot ]; then
    worktree_snapshot > "$TEMP_ROOT/snapshot.before.txt"
    worktree_snapshot > "$TEMP_ROOT/snapshot.after.txt"
    snapshot_equal "$TEMP_ROOT/snapshot.before.txt" "$TEMP_ROOT/snapshot.after.txt" ||
        fail 'stale-snapshot: Playstead changed during snapshot capture'
    printf '%s\n' '<!-- PLAYSTEAD_SNAPSHOT_BEGIN -->'
    cat "$TEMP_ROOT/snapshot.after.txt"
    printf '%s\n' '<!-- PLAYSTEAD_SNAPSHOT_END -->'
    exit 0
fi

if [ "$MODE" = --check ]; then
    check_report_file "$REPORT"
    printf '%s\n' 'phase23: Playstead fit snapshot verified'
    exit 0
fi

if [ "$MODE" = --capture-controls ]; then
    verify_symlinked_parent_controls
    verify_captured_copy_stability
    printf '%s\n' 'phase23: captured-copy and symlink controls verified'
    exit 0
fi

if [ "$MODE" = --phase-resolution-controls ]; then
    verify_phase_dir_resolution_controls
    printf '%s\n' 'phase23: active/archive phase path controls verified'
    exit 0
fi

[ -f "$REPORT" ] && [ ! -L "$REPORT" ] || fail 'fit report is missing or ambiguous'
check_output_one="$TEMP_ROOT/check-one.log"
check_output_two="$TEMP_ROOT/check-two.log"
sh "$SCRIPT_DIR/check-phase23-playstead-fit.sh" --check > "$check_output_one" || fail 'unchanged fit report did not pass its first check'
sh "$SCRIPT_DIR/check-phase23-playstead-fit.sh" --check > "$check_output_two" || fail 'unchanged fit report did not pass its second check'
snapshot_equal "$check_output_one" "$check_output_two" || fail 'unchanged fit report produced different check results'

cp "$REPORT" "$TEMP_ROOT/stale-head.md"
awk '
    /<!-- PLAYSTEAD_SNAPSHOT_BEGIN -->/ { inside = 1 }
    inside && /^head=/ { print "head=0000000000000000000000000000000000000000"; changed = 1; next }
    { print }
    /<!-- PLAYSTEAD_SNAPSHOT_END -->/ { inside = 0 }
    END { if (!changed) exit 1 }
' "$REPORT" > "$TEMP_ROOT/stale-head.tmp" || fail 'cannot create stale HEAD control'
mv "$TEMP_ROOT/stale-head.tmp" "$TEMP_ROOT/stale-head.md"
must_reject "$TEMP_ROOT/stale-head.md" 'Playstead HEAD mismatch' stale-head

awk -F'|' '
    BEGIN { OFS = "|" }
    /<!-- PLAYSTEAD_SNAPSHOT_BEGIN -->/ { inside = 1 }
    inside && /^file=docs\/SUPPORT-MATRIX\.md\|/ {
        $3 = "0000000000000000000000000000000000000000000000000000000000000000"
        changed = 1
    }
    { print }
    /<!-- PLAYSTEAD_SNAPSHOT_END -->/ { inside = 0 }
    END { if (!changed) exit 1 }
' "$REPORT" > "$TEMP_ROOT/stale-file.tmp" || fail 'cannot create stale cited-file control'
mv "$TEMP_ROOT/stale-file.tmp" "$TEMP_ROOT/stale-file.md"
must_reject "$TEMP_ROOT/stale-file.md" 'cited-file status or content mismatch: docs/SUPPORT-MATRIX.md' stale-file

awk -F'|' '
    BEGIN { OFS = "|" }
    /<!-- CAPABILITY_MAPPING_BEGIN -->/ { inside = 1 }
    inside && /^\| Lifecycle \|/ { $5 = " docs/SUPPORT-MATRIX.md#999999-1000000"; changed = 1 }
    { print }
    /<!-- CAPABILITY_MAPPING_END -->/ { inside = 0 }
    END { if (!changed) exit 1 }
' "$REPORT" > "$TEMP_ROOT/invalid-range.tmp" || fail 'cannot create invalid citation control'
mv "$TEMP_ROOT/invalid-range.tmp" "$TEMP_ROOT/invalid-range.md"
must_reject "$TEMP_ROOT/invalid-range.md" 'citation line range invalid: docs/SUPPORT-MATRIX.md#999999-1000000' invalid-range

awk '
    /<!-- CAPABILITY_MAPPING_BEGIN -->/ { inside = 1 }
    inside && /^\| Saves \|/ { next }
    { print }
    /<!-- CAPABILITY_MAPPING_END -->/ { inside = 0 }
' "$REPORT" > "$TEMP_ROOT/missing-row.md"
must_reject "$TEMP_ROOT/missing-row.md" 'capability rows must occur exactly once in the required order' missing-row

awk '
    /<!-- CAPABILITY_MAPPING_BEGIN -->/ { inside = 1 }
    { print }
    inside && /^\| Lifecycle \|/ { print }
    /<!-- CAPABILITY_MAPPING_END -->/ { inside = 0 }
' "$REPORT" > "$TEMP_ROOT/duplicate-row.md"
must_reject "$TEMP_ROOT/duplicate-row.md" 'capability rows must occur exactly once in the required order' duplicate-row

worktree_snapshot > "$TEMP_ROOT/equality-before.txt"
awk -F'|' '
    BEGIN { OFS = "|" }
    /^file=docs\/SUPPORT-MATRIX\.md\|/ { $3 = "0000000000000000000000000000000000000000000000000000000000000000" }
    { print }
' "$TEMP_ROOT/equality-before.txt" > "$TEMP_ROOT/equality-after.txt"
if snapshot_equal "$TEMP_ROOT/equality-before.txt" "$TEMP_ROOT/equality-after.txt"; then
    fail 'moving-source equality control failed to detect a cited digest change'
fi

verify_symlinked_parent_controls
verify_captured_copy_stability
verify_phase_dir_resolution_controls
check_report_file "$REPORT"
printf '%s\n' 'phase23: Playstead fit and symlink controls verified'
