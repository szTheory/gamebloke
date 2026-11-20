#!/bin/sh
set -eu

usage() {
    cat >&2 <<'USAGE'
Usage: verify-phase11-plan-paths.sh (--capture|--check) [--group phase10|phase11|all] [--plans NN,NN] [--bin PATH]

Capture stores immutable local preimages for the fixed sixteen-file manifest.
Check compares selected files byte-for-byte with those originals after only the
two approved execution-context path substitutions. --plans is valid with a
single phase group; --group all checks the full fixed manifest.
USAGE
    exit 2
}

fail() {
    printf 'phase11-plan-paths: %s\n' "$*" >&2
    exit 1
}

mode=
group=all
plans=
bin=build/ci

while [ "$#" -gt 0 ]; do
    case "$1" in
        --capture|--check)
            [ -z "$mode" ] || usage
            mode=${1#--}
            ;;
        --group)
            [ "$#" -ge 2 ] || usage
            group=$2
            shift
            ;;
        --plans)
            [ "$#" -ge 2 ] || usage
            plans=$2
            shift
            ;;
        --bin)
            [ "$#" -ge 2 ] || usage
            bin=$2
            shift
            ;;
        *) usage ;;
    esac
    shift
done

[ -n "$mode" ] || usage
case "$group" in phase10|phase11|all) ;; *) usage ;; esac

case "$0" in
    */*) script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd) ;;
    *) script_dir=$(CDPATH= cd scripts && pwd) ;;
esac
root_dir=$(CDPATH= cd "$script_dir/.." && pwd)
case "$bin" in
    /*) bin_dir=$bin ;;
    *) bin_dir=$root_dir/$bin ;;
esac
evidence_dir=$bin_dir/phase11-plan-paths
preimage_dir=$evidence_dir/preimages
manifest=$evidence_dir/manifest.tsv

home_marker='@'
home_separator='/'
home_root='Users'
home_owner='jon'
gsd_root='/.codex/gsd-core'
workflow_source=${home_marker}${home_separator}${home_root}/${home_owner}${gsd_root}/workflows/execute-plan.md
summary_source=${home_marker}${home_separator}${home_root}/${home_owner}${gsd_root}/templates/summary.md
workflow_pattern=$(printf '%s\n' "$workflow_source" | sed 's/\./\\./g')
summary_pattern=$(printf '%s\n' "$summary_source" | sed 's/\./\\./g')
workflow_target='@$HOME/.codex/gsd-core/workflows/execute-plan.md'
summary_target='@$HOME/.codex/gsd-core/templates/summary.md'

phase10_numbers='01 02 03 04 05 06 07 08 09 10 11'
phase11_numbers='01 02 03 04 05'

path_for() {
    case "$1:$2" in
        phase10:*) printf '.planning/milestones/v2.0-phases/10-ppu-timing-and-first-pixels/10-%s-PLAN.md\n' "$2" ;;
        phase11:*) printf '.planning/phases/11-sampled-rows-and-transactional-visibility/11-%s-PLAN.md\n' "$2" ;;
        *) fail "internal path selection error: $1/$2" ;;
    esac
}

validate_original() {
    awk -v workflow="$workflow_source" -v summary="$summary_source" '
        /^<execution_context>[[:space:]]*$/ { in_execution_context = 1; next }
        /^<\/execution_context>[[:space:]]*$/ { in_execution_context = 0; next }
        {
            if ($0 == workflow) {
                workflow_count++
                if (!in_execution_context) outside++
            }
            if ($0 == summary) {
                summary_count++
                if (!in_execution_context) outside++
            }
        }
        END {
            if (workflow_count != 1 || summary_count != 1 || outside != 0) exit 1
        }
    ' "$1" || fail "expected exactly one workflow and one summary source inside execution_context: $2"
}

identity_for() {
    file=$1
    identity=$(cksum < "$file") || fail "could not identify $file"
    set -- $identity
    [ "$#" -eq 2 ] || fail "unexpected cksum output for $file"
    printf '%s\t%s' "$1" "$2"
}

all_paths() {
    for number in $phase10_numbers; do
        path_for phase10 "$number"
    done
    for number in $phase11_numbers; do
        path_for phase11 "$number"
    done
}

if [ "$mode" = capture ]; then
    [ "$group" = all ] || usage
    [ -z "$plans" ] || usage
    [ -d "$bin_dir" ] || fail "build directory does not exist: $bin_dir"
    [ ! -e "$evidence_dir" ] || fail "refusing to overwrite existing preimages: $evidence_dir"

    while IFS= read -r path; do
        [ -f "$root_dir/$path" ] || fail "manifest file is missing: $path"
        validate_original "$root_dir/$path" "$path"
    done <<EOF_PATHS
$(all_paths)
EOF_PATHS

    mkdir -p "$preimage_dir"
    : > "$manifest"
    while IFS= read -r path; do
        source_file=$root_dir/$path
        saved_file=$preimage_dir/$path
        mkdir -p "$(dirname "$saved_file")"
        cp "$source_file" "$saved_file" || fail "could not save preimage: $path"
        identity=$(identity_for "$saved_file")
        printf '%s\t%s\n' "$path" "$identity" >> "$manifest"
    done <<EOF_PATHS
$(all_paths)
EOF_PATHS

    printf 'Captured immutable preimages for 16 documents at %s\n' "$evidence_dir"
    cat "$manifest"
    exit 0
fi

[ -d "$bin_dir" ] || fail "build directory does not exist: $bin_dir"
[ -f "$manifest" ] || fail "preimage manifest is missing: $manifest"

temp_dir=${TMPDIR:-/tmp}/phase11-plan-paths.$$
mkdir "$temp_dir" 2>/dev/null || fail "could not create temporary directory: $temp_dir"
trap 'rm -rf "$temp_dir"' 0 HUP INT TERM

all_paths > "$temp_dir/expected-paths"
cut -f 1 "$manifest" > "$temp_dir/recorded-paths"
cmp -s "$temp_dir/expected-paths" "$temp_dir/recorded-paths" || fail 'preimage manifest does not match the fixed sixteen-file set'
awk -F '\t' 'NF != 3 || $1 == "" || $2 == "" || $3 == "" { bad = 1 } END { if (NR != 16 || bad) exit 1 }' "$manifest" || fail 'preimage manifest is malformed'

tab=$(printf '\t')
while IFS="$tab" read -r path expected_crc expected_size; do
    saved_file=$preimage_dir/$path
    [ -f "$saved_file" ] || fail "saved preimage is missing: $path"
    actual_identity=$(identity_for "$saved_file")
    expected_identity=$(printf '%s\t%s' "$expected_crc" "$expected_size")
    [ "$actual_identity" = "$expected_identity" ] || fail "saved preimage identity changed: $path"
done < "$manifest"

if [ -n "$plans" ]; then
    [ "$group" != all ] || fail '--plans requires --group phase10 or --group phase11'
    case "$plans" in *[!0-9,]*|,*|*,|*,,*) fail 'plans must be comma-separated two-digit plan numbers' ;; esac
    case "$group" in
        phase10) allowed_numbers=$phase10_numbers ;;
        phase11) allowed_numbers=$phase11_numbers ;;
    esac
    old_ifs=$IFS
    IFS=,
    set -f
    # Input is restricted above to digits and commas, so splitting is safe.
    set -- $plans
    IFS=$old_ifs
    selected_numbers=
    for number do
        case "$number" in [0-9][0-9]) ;; *) fail "plan numbers must use two digits: $number" ;; esac
        case " $allowed_numbers " in *" $number "*) ;; *) fail "plan $number is outside group $group" ;; esac
        case " $selected_numbers " in *" $number "*) fail "duplicate plan number: $number" ;; esac
        selected_numbers="$selected_numbers $number"
    done
else
    case "$group" in
        phase10) selected_numbers=$phase10_numbers ;;
        phase11) selected_numbers=$phase11_numbers ;;
        all) selected_numbers=all ;;
    esac
fi

check_one() {
    path=$1
    saved_file=$preimage_dir/$path
    current_file=$root_dir/$path
    expected_file=$temp_dir/expected
    sed \
        -e "s|^${workflow_pattern}\$|${workflow_target}|" \
        -e "s|^${summary_pattern}\$|${summary_target}|" \
        "$saved_file" > "$expected_file"
    cmp -s "$expected_file" "$current_file" || fail "unexpected change beyond the two approved references: $path"
    printf 'PASS %s (2 approved reference lines)\n' "$path"
}

checked=0
if [ "$selected_numbers" = all ]; then
    while IFS= read -r path; do
        check_one "$path"
        checked=$((checked + 1))
    done <<EOF_PATHS
$(all_paths)
EOF_PATHS
else
    for number in $selected_numbers; do
        check_one "$(path_for "$group" "$number")"
        checked=$((checked + 1))
    done
fi

printf 'Checked %s documents; expected replacements: %s\n' "$checked" "$((checked * 2))"
