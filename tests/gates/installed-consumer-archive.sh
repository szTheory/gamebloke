#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -P "$(dirname "$0")/../.." && pwd -P)
ARCHIVE_REL='.planning/milestones/v5.0-phases/21-installed-consumer-proof-and-libfuturo-fit'
ARCHIVE="$ROOT/$ARCHIVE_REL"
TMP_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/gamebloke-phase22-archive.XXXXXX") || {
    printf '%s\n' 'not ok 1 - temporary repository setup' '1..1'
    exit 1
}
trap 'rm -rf "$TMP_ROOT"' 0 HUP INT TERM

TEST_ROOT="$TMP_ROOT/repository"
SENTINEL_BIN="$TMP_ROOT/sentinels"
PROOF_SENTINEL_BIN="$TMP_ROOT/proof-sentinels"
SENTINEL_LOG="$TMP_ROOT/sentinel-calls.log"
OUT="$TMP_ROOT/command-output.txt"
mkdir -p \
    "$TEST_ROOT/examples/installed-consumer" \
    "$TEST_ROOT/scripts" \
    "$TEST_ROOT/cmake" \
    "$TEST_ROOT/$ARCHIVE_REL" \
    "$SENTINEL_BIN" \
    "$PROOF_SENTINEL_BIN"
TEST_ROOT=$(CDPATH= cd -P "$TEST_ROOT" && pwd -P)
cp "$ROOT/scripts/check-installed-consumer.sh" "$TEST_ROOT/scripts/"
cp "$ROOT/scripts/check-installed-consumer.cmake" "$TEST_ROOT/scripts/"
cp "$ROOT/cmake/PathContainment.cmake" "$TEST_ROOT/cmake/"
cp "$ROOT/examples/installed-consumer/README.md" \
    "$TEST_ROOT/examples/installed-consumer/README.md"
cp "$ARCHIVE/fixture-manifest.json" "$TEST_ROOT/$ARCHIVE_REL/"
cp "$ARCHIVE/21-DIAGNOSTIC-NOTICE.md" "$TEST_ROOT/$ARCHIVE_REL/"
cp "$ARCHIVE/21-VERIFICATION.md" "$TEST_ROOT/$ARCHIVE_REL/"

test_number=0
failures=0
record()
{
    test_number=$((test_number + 1))
    if [ "$1" -eq 0 ]; then
        printf 'ok %s - %s\n' "$test_number" "$2"
    else
        printf 'not ok %s - %s\n' "$test_number" "$2"
        failures=$((failures + 1))
    fi
}

make_sentinel()
{
    sentinel_path=$1
    cat > "$sentinel_path" <<'SENTINEL'
#!/bin/sh
printf '%s\n' "${0##*/}" >> "$PHASE22_SENTINEL_LOG"
exit 97
SENTINEL
    chmod +x "$sentinel_path"
}

make_sentinel "$SENTINEL_BIN/cmake"
make_sentinel "$SENTINEL_BIN/shasum"
make_sentinel "$SENTINEL_BIN/phase22-compiler-sentinel"
make_sentinel "$PROOF_SENTINEL_BIN/shasum"
make_sentinel "$PROOF_SENTINEL_BIN/phase22-compiler-sentinel"

run_copy()
{
    run_path=$1
    shift
    if CI= \
       GBL_LIBFUTURO_CHECKOUT="$TMP_ROOT/absent-libfuturo" \
       PHASE22_SENTINEL_LOG="$SENTINEL_LOG" \
       CC="$SENTINEL_BIN/phase22-compiler-sentinel" \
       PATH="$SENTINEL_BIN:$PATH" \
       sh "$run_path" "$@" > "$OUT" 2>&1; then
        RUN_STATUS=0
    else
        RUN_STATUS=$?
    fi
}

run_copy_with_real_tools()
{
    run_path=$1
    shift
    if CI= \
       GBL_LIBFUTURO_CHECKOUT="$TMP_ROOT/absent-libfuturo" \
       sh "$run_path" "$@" > "$OUT" 2>&1; then
        RUN_STATUS=0
    else
        RUN_STATUS=$?
    fi
}

run_copy_with_guarded_proof_tools()
{
    run_path=$1
    shift
    if CI= \
       GBL_LIBFUTURO_CHECKOUT="$TMP_ROOT/absent-libfuturo" \
       PHASE22_SENTINEL_LOG="$SENTINEL_LOG" \
       CC="$PROOF_SENTINEL_BIN/phase22-compiler-sentinel" \
       PATH="$PROOF_SENTINEL_BIN:$PATH" \
       sh "$run_path" "$@" > "$OUT" 2>&1; then
        RUN_STATUS=0
    else
        RUN_STATUS=$?
    fi
}

no_proof_outputs()
{
    [ ! -e "$TEST_ROOT/build/phase21-local" ] &&
    [ ! -e "$TEST_ROOT/tests/fixtures/phase21_diagnostic.c" ] &&
    [ ! -e "$TEST_ROOT/build/phase21-local/fixture/phase21-diagnostic.gba" ] &&
    [ ! -e "$TEST_ROOT/build/phase21-local/evidence" ]
}

expect_missing_record()
{
    record_name=$1
    mode=$2
    case "$record_name" in
        manifest)
            record_path="$TEST_ROOT/$ARCHIVE_REL/fixture-manifest.json"
            expected_prefix='phase21-archive: manifest unavailable:'
            ;;
        notice)
            record_path="$TEST_ROOT/$ARCHIVE_REL/21-DIAGNOSTIC-NOTICE.md"
            expected_prefix='phase21-archive: notice unavailable:'
            ;;
    esac
    saved_path="$TMP_ROOT/saved-$record_name"
    mv "$record_path" "$saved_path"
    : > "$SENTINEL_LOG"
    case "$mode" in
        explicit)
            run_copy_with_guarded_proof_tools \
                "$TEST_ROOT/scripts/check-installed-consumer.sh" \
                --archive-preflight
            ;;
        ordinary)
            run_copy_with_guarded_proof_tools \
                "$TEST_ROOT/scripts/check-installed-consumer.sh"
            ;;
    esac
    failed=0
    if [ "$RUN_STATUS" -eq 0 ] ||
       ! grep -F "$expected_prefix" "$OUT" >/dev/null ||
       ! grep -F "$record_path" "$OUT" >/dev/null; then
        failed=1
        printf '# missing-record refusal assertion failed: %s (%s mode), status=%s\n' \
            "$record_name" "$mode" "$RUN_STATUS"
        printf '# expected prefix: %s\n# expected path: %s\n' \
            "$expected_prefix" "$record_path"
        if grep -F "$expected_prefix" "$OUT" >/dev/null; then
            printf '%s\n' '# prefix found'
        else
            printf '%s\n' '# prefix missing'
        fi
        if grep -F "$record_path" "$OUT" >/dev/null; then
            printf '%s\n' '# path found'
        else
            printf '%s\n' '# path missing'
        fi
    fi
    if [ -s "$SENTINEL_LOG" ]; then
        failed=1
        printf '# proof tool sentinel was called: %s (%s mode)\n' \
            "$record_name" "$mode"
        sed 's/^/# sentinel: /' "$SENTINEL_LOG"
    fi
    if ! no_proof_outputs; then
        failed=1
        printf '# proof output appeared during refusal: %s (%s mode)\n' \
            "$record_name" "$mode"
    fi
    mv "$saved_path" "$record_path"
    record "$failed" "missing $record_name is named before proof setup in $mode mode"
    if [ "$failed" -ne 0 ]; then
        sed 's/^/# command output: /' "$OUT"
    fi
}

logical_lines()
{
    awk '
        function emit() {
            line = logical
            sub(/^[[:space:]]+/, "", line)
            sub(/[[:space:]]+$/, "", line)
            gsub(/[[:space:]]+/, " ", line)
            if (line != "" && line !~ /^#/) print line
            logical = ""
        }
        {
            part = $0
            sub(/^[[:space:]]+/, "", part)
            if (part ~ /^#/) next
            if (logical == "") logical = part
            else logical = logical " " part
            if (part ~ /\\[[:space:]]*$/) {
                sub(/\\[[:space:]]*$/, "", logical)
                next
            }
            emit()
        }
        END { if (logical != "") emit() }
    ' "$1"
}

assert_routing()
{
    source=$1
    flattened="$TMP_ROOT/flattened-runner.sh"
    logical_lines "$source" > "$flattened"
    expected_root="ARCHIVED_PHASE21_ROOT=\"\$REPO_ROOT/$ARCHIVE_REL\""
    expected_manifest='ARCHIVED_PHASE21_MANIFEST="$ARCHIVED_PHASE21_ROOT/fixture-manifest.json"'
    root_count=$(awk -v expected="$expected_root" \
        '$0 ~ /^ARCHIVED_PHASE21_ROOT=/ { count++; if ($0 == expected) exact++ } END { print count + 0 ":" exact + 0 }' \
        "$flattened")
    manifest_count=$(awk -v expected="$expected_manifest" \
        '$0 ~ /^ARCHIVED_PHASE21_MANIFEST=/ { count++; if ($0 == expected) exact++ } END { print count + 0 ":" exact + 0 }' \
        "$flattened")
    [ "$root_count" = '1:1' ] && [ "$manifest_count" = '1:1' ] || return 1

    for stage in preflight evidence fit; do
        stage_result=$(awk -v wanted="$stage" '
            /^cmake / && index($0, "-P \"$REPO_ROOT/scripts/check-installed-consumer.cmake\"") {
                stage_count = 0
                manifest_count = 0
                manifest_value = ""
                value = ""
                words = split($0, args, /[[:space:]]+/)
                for (i = 1; i <= words; i++) {
                    if (args[i] ~ /^-DGBL_STAGE=/) {
                        stage_count++
                        value = substr(args[i], length("-DGBL_STAGE=") + 1)
                    }
                    if (args[i] ~ /^-DGBL_MANIFEST=/) {
                        manifest_count++
                        manifest_value = args[i]
                    }
                }
                if (stage_count == 1 && value == wanted) {
                    matches++
                    if (manifest_count == 1 &&
                        manifest_value == "-DGBL_MANIFEST=\"$ARCHIVED_PHASE21_MANIFEST\"")
                        exact++
                }
            }
            END { print matches + 0 ":" exact + 0 }
        ' "$flattened")
        if [ "$stage_result" != '1:1' ]; then
            printf '# archived manifest binding mismatch for checker stage: %s\n' "$stage"
            return 1
        fi
    done

    grep -Fx 'FIT_REPORT="$ARCHIVED_PHASE21_ROOT/LIBFUTURO-FIT.md"' \
        "$flattened" >/dev/null || {
            printf '%s\n' '# fit report root must derive from ARCHIVED_PHASE21_ROOT'
            return 1
        }
    grep -F 'check_fit_report "$FIT_REPORT"' "$flattened" >/dev/null || {
        printf '%s\n' '# ordinary fit validation must use FIT_REPORT'
        return 1
    }
    grep -F -- '-DGBL_FIT_REPORT="$1"' "$flattened" >/dev/null || {
        printf '%s\n' '# fit checker must receive the helper report argument'
        return 1
    }
}

make_stage_mutation()
{
    source=$1
    wanted_stage=$2
    destination=$3
    logical_lines "$source" | awk -v wanted="$wanted_stage" -v archive="$ARCHIVE_REL" '
        /^cmake / && index($0, "-P \"$REPO_ROOT/scripts/check-installed-consumer.cmake\"") {
            words = split($0, args, /[[:space:]]+/)
            current_stage = ""
            for (i = 1; i <= words; i++) {
                if (args[i] ~ /^-DGBL_STAGE=/)
                    current_stage = substr(args[i], length("-DGBL_STAGE=") + 1)
            }
            if (current_stage == wanted) {
                old = "-DGBL_MANIFEST=\"$ARCHIVED_PHASE21_MANIFEST\""
                new = "-DGBL_MANIFEST=\"$REPO_ROOT/.planning/phases/21-installed-consumer-proof-and-libfuturo-fit/fixture-manifest.json\""
                position = index($0, old)
                if (position > 0)
                    $0 = substr($0, 1, position - 1) new substr($0, position + length(old))
            }
        }
        { print }
    ' > "$destination"
}

make_fit_mutation()
{
    source=$1
    destination=$2
    logical_lines "$source" | awk '
        $0 ~ /^FIT_REPORT=/ {
            old = "$ARCHIVED_PHASE21_ROOT"
            new = "$REPO_ROOT/.planning/milestones/v5.0-phases/21-installed-consumer-proof-and-libfuturo-fit"
            position = index($0, old)
            if (position > 0)
                $0 = substr($0, 1, position - 1) new substr($0, position + length(old))
        }
        { print }
    ' > "$destination"
}

printf '%s\n' 'TAP version 13'

run_path="$TEST_ROOT/scripts/check-installed-consumer.sh"
run_copy_with_real_tools "$run_path" --archive-preflight
if [ "$RUN_STATUS" -eq 0 ] &&
   grep -Fx 'phase21-archive: references verified' "$OUT" >/dev/null; then
    record 0 'archived metadata-only preflight accepts the resolved records'
else
    record 1 'archived metadata-only preflight accepts the resolved records'
    sed 's/^/# command output: /' "$OUT"
fi

: > "$SENTINEL_LOG"
run_copy "$run_path" --archive-preflight --check-fit
if [ "$RUN_STATUS" -eq 2 ] &&
   grep -F 'usage:' "$OUT" >/dev/null &&
   grep -Fx 'phase21-archive: --archive-preflight must be used alone' "$OUT" >/dev/null &&
   [ ! -s "$SENTINEL_LOG" ] &&
   [ ! -e "$TEST_ROOT/build/phase21-local" ]; then
    record 0 'preflight combined with fit refuses before proof tools or output'
else
    record 1 'preflight combined with fit refuses before proof tools or output'
    sed 's/^/# command output: /' "$OUT"
fi

: > "$SENTINEL_LOG"
run_copy "$run_path" --negative-controls --archive-preflight
if [ "$RUN_STATUS" -eq 2 ] &&
   grep -F 'usage:' "$OUT" >/dev/null &&
   grep -Fx 'phase21-archive: --archive-preflight must be used alone' "$OUT" >/dev/null &&
   [ ! -s "$SENTINEL_LOG" ] &&
   [ ! -e "$TEST_ROOT/build/phase21-local" ]; then
    record 0 'preflight combined with negative controls refuses before proof tools or output'
else
    record 1 'preflight combined with negative controls refuses before proof tools or output'
    sed 's/^/# command output: /' "$OUT"
fi

if assert_routing "$ROOT/scripts/check-installed-consumer.sh"; then
    record 0 'ordinary proof stages and fit report resolve from one archived root'
else
    record 1 'ordinary proof stages and fit report resolve from one archived root'
fi

mutation_failed=0
for stage in preflight evidence fit; do
    mutated="$TMP_ROOT/mutated-$stage.sh"
    make_stage_mutation "$ROOT/scripts/check-installed-consumer.sh" "$stage" "$mutated"
    if assert_routing "$mutated"; then
        mutation_failed=1
        printf '# routing mutation unexpectedly passed: %s manifest\n' "$stage"
    fi
done
mutated="$TMP_ROOT/mutated-fit-report.sh"
make_fit_mutation "$ROOT/scripts/check-installed-consumer.sh" "$mutated"
if assert_routing "$mutated"; then
    mutation_failed=1
    printf '%s\n' '# routing mutation unexpectedly passed: fit report root'
fi
record "$mutation_failed" 'independent stage and fit report mutations are detected'

[ ! -e "$TEST_ROOT/tests/fixtures" ] &&
[ ! -e "$TEST_ROOT/build/phase21-local" ] || {
    printf '%s\n' '# fixture-free preflight copy acquired proof inputs or output'
    failures=$((failures + 1))
}

if [ ! -e "$TEST_ROOT/.planning/phases/21-installed-consumer-proof-and-libfuturo-fit" ] &&
   [ ! -e "$TEST_ROOT/tests/fixtures/phase21_diagnostic.c" ] &&
   no_proof_outputs; then
    record 0 'metadata-only copy has no active phase, generator, fixture, or proof output'
else
    record 1 'metadata-only copy has no active phase, generator, fixture, or proof output'
fi

expect_missing_record manifest explicit
expect_missing_record manifest ordinary
expect_missing_record notice explicit
expect_missing_record notice ordinary

readme_path="$TEST_ROOT/examples/installed-consumer/README.md"
saved_readme="$TMP_ROOT/README-original.md"
cp "$readme_path" "$saved_readme"

expect_readme_refusal()
{
    description=$1
    expected_detail=$2
    : > "$SENTINEL_LOG"
    run_copy_with_real_tools "$run_path" --archive-preflight
    failed=0
    if [ "$RUN_STATUS" -eq 0 ] ||
       ! grep -F 'phase21-archive: README mismatch:' "$OUT" >/dev/null ||
       ! grep -F "$expected_detail" "$OUT" >/dev/null; then
        failed=1
        printf '# README refusal assertion failed: %s (status=%s)\n' \
            "$description" "$RUN_STATUS"
    fi
    if [ -s "$SENTINEL_LOG" ] || ! no_proof_outputs; then
        failed=1
        printf '# README refusal reached a proof sentinel or created proof output: %s\n' \
            "$description"
    fi
    record "$failed" "$description"
    if [ "$failed" -ne 0 ]; then
        sed 's/^/# command output: /' "$OUT"
    fi
}

for hidden_mode in fence comment inline; do
    cp "$saved_readme" "$readme_path"
    awk -v mode="$hidden_mode" '
        /^## Archived Phase 21 records$/ { in_archive = 1 }
        in_archive && /^## / && $0 != "## Archived Phase 21 records" { in_archive = 0 }
        in_archive && index($0, "[archived fixture manifest](") {
            if (mode == "fence") { print "```md"; print; print "```" }
            else if (mode == "comment") { print "<!--"; print; print "-->" }
            else print "- `" $0 "`"
            next
        }
        in_archive && index($0, "[archived diagnostic notice](") {
            if (mode == "fence") { print "```md"; print; print "```" }
            else if (mode == "comment") { print "<!--"; print; print "-->" }
            else print "- `" $0 "`"
            next
        }
        in_archive && index($0, "[archived verification record](") {
            if (mode == "fence") { print "```md"; print; print "```" }
            else if (mode == "comment") { print "<!--"; print; print "-->" }
            else print "- `" $0 "`"
            next
        }
        { print }
    ' "$saved_readme" > "$readme_path"
    expect_readme_refusal \
        "archive preflight ignores links hidden in $hidden_mode blocks" \
        'link count'
done

cp "$saved_readme" "$readme_path"
awk '
    /^## Archived Phase 21 records$/ { print; print "<!-- -->```md"; next }
    { print }
' "$saved_readme" > "$readme_path"
expect_readme_refusal \
    'archive preflight refuses a comment that could change code-fence parsing' \
    'inline HTML comment is unsupported'

cp "$saved_readme" "$readme_path"
sed 's|\[archived fixture manifest\](|[archived fixture manifest] (|' \
    "$saved_readme" > "$readme_path"
expect_readme_refusal \
    'archive preflight requires an immediate opening parenthesis after each label' \
    'archived fixture manifest link count'

cp "$saved_readme" "$readme_path"
awk '
    index($0, "[archived fixture manifest](") { print "    " $0; next }
    { print }
' "$saved_readme" > "$readme_path"
expect_readme_refusal \
    'archive preflight fails closed on indented code in the archive section' \
    'indented code'

cp "$saved_readme" "$readme_path"
awk '
    index($0, "[archived fixture manifest](") {
        print "<div>"
        print
        print "</div>"
        next
    }
    { print }
' "$saved_readme" > "$readme_path"
expect_readme_refusal \
    'archive preflight fails closed on raw HTML blocks in the archive section' \
    'raw HTML block'

cp "$saved_readme" "$readme_path"
awk '
    index($0, "[archived fixture manifest](") {
        print "`"
        print
        print "`"
        next
    }
    { print }
' "$saved_readme" > "$readme_path"
expect_readme_refusal \
    'archive preflight fails closed on multi-line code spans' \
    'multi-line code span is unsupported'

cp "$saved_readme" "$readme_path"
awk '
    index($0, "[archived fixture manifest](") {
        print ">     " $0
        next
    }
    { print }
' "$saved_readme" > "$readme_path"
expect_readme_refusal \
    'archive preflight fails closed on blockquote-indented code links' \
    'blockquote content'

cp "$saved_readme" "$readme_path"
awk '
    index($0, "sh scripts/check-installed-consumer.sh --archive-preflight") { next }
    /^## Requirements and evidence$/ {
        print "<!-- sh scripts/check-installed-consumer.sh --archive-preflight -->"
    }
    { print }
' "$saved_readme" > "$readme_path"
expect_readme_refusal \
    'archive preflight requires its command in a visible fenced code block' \
    'archive-preflight command in visible code block'

cp "$saved_readme" "$readme_path"
awk '
    /^## Archived Phase 21 records$/ { print "<div>"; in_archive = 1 }
    in_archive && /^## Requirements and evidence$/ {
        print "</div>"
        in_archive = 0
    }
    { print }
' "$saved_readme" > "$readme_path"
expect_readme_refusal \
    'archive preflight refuses a raw HTML block opened before the archive section' \
    'raw HTML block'

cp "$saved_readme" "$readme_path"
awk '
    /^## Archived Phase 21 records$/ {
        print
        print "`"
        print "```sh"
        print "`"
        print "```"
        print "<pre>"
        print "</pre>"
        print "`"
        next
    }
    { print }
' "$saved_readme" > "$readme_path"
expect_readme_refusal \
    'archive preflight does not pair code-span delimiters across a fenced block' \
    'multi-line code span is unsupported'

cp "$saved_readme" "$readme_path"
awk '
    index($0, "[archived fixture manifest](") {
        print "- <pre>"
        print "  " $0
        print "  </pre>"
        next
    }
    { print }
' "$saved_readme" > "$readme_path"
expect_readme_refusal \
    'archive preflight refuses raw HTML tags inside list items' \
    'raw HTML block'

cp "$saved_readme" "$readme_path"
awk '
    /^## Archived Phase 21 records$/ { print "`"; print; next }
    /^## Requirements and evidence$/ { print "`"; print; next }
    { print }
' "$saved_readme" > "$readme_path"
expect_readme_refusal \
    'archive preflight fails closed on a multiline code span opened before the archive section' \
    'multi-line code span is unsupported'

cp "$saved_readme" "$readme_path"
awk '
    index($0, "[archived diagnostic notice](") { next }
    { print }
' "$saved_readme" > "$readme_path"
expect_readme_refusal \
    'archive preflight rejects a missing rendered archive link' \
    'archived diagnostic notice link count'

cp "$saved_readme" "$readme_path"
awk '
    { print }
    index($0, "[archived fixture manifest](") { print }
' "$saved_readme" > "$readme_path"
expect_readme_refusal \
    'archive preflight rejects duplicate rendered archive links' \
    'archived fixture manifest link count'

cp "$saved_readme" "$readme_path"
sed 's|../../.planning/milestones/v5.0-phases/21-installed-consumer-proof-and-libfuturo-fit/fixture-manifest.json|../../.planning/milestones/v5.0-phases/21-installed-consumer-proof-and-libfuturo-fit/21-DIAGNOSTIC-NOTICE.md|' \
    "$saved_readme" > "$readme_path"
printf '%s\n' \
    '<!-- [archived fixture manifest](../../.planning/milestones/v5.0-phases/21-installed-consumer-proof-and-libfuturo-fit/fixture-manifest.json) -->' \
    >> "$readme_path"
expect_readme_refusal \
    'archive preflight resolves the captured rendered link destination' \
    '(manifest link)'

restore_readme_parent()
{
    rm "$TEST_ROOT/examples/installed-consumer"
    mv "$TMP_ROOT/installed-consumer-original" \
        "$TEST_ROOT/examples/installed-consumer"
}

mv "$TEST_ROOT/examples/installed-consumer" \
    "$TMP_ROOT/installed-consumer-original"
mkdir -p "$TEST_ROOT/deep/really/consumer"
cp "$saved_readme" "$TEST_ROOT/deep/really/consumer/README.md"
ln -s "$TEST_ROOT/deep/really/consumer" \
    "$TEST_ROOT/examples/installed-consumer"
expect_readme_refusal \
    'archive preflight resolves links from an internal symlinked README parent' \
    '(manifest link)'
restore_readme_parent

mv "$TEST_ROOT/examples/installed-consumer" \
    "$TMP_ROOT/installed-consumer-original"
mkdir -p "$TMP_ROOT/external-consumer"
cp "$saved_readme" "$TMP_ROOT/external-consumer/README.md"
ln -s "$TMP_ROOT/external-consumer" \
    "$TEST_ROOT/examples/installed-consumer"
expect_readme_refusal \
    'archive preflight rejects a README parent outside the repository' \
    '(unavailable)'
restore_readme_parent

cp "$saved_readme" "$readme_path"
printf '%s\n' \
    'Old record: .planning/phases/21-installed-consumer-proof-and-libfuturo-fit/fixture-manifest.json' \
    >> "$readme_path"
: > "$SENTINEL_LOG"
run_copy_with_real_tools "$run_path" --archive-preflight
stale_failed=0
if [ "$RUN_STATUS" -eq 0 ] ||
   ! grep -F 'phase21-archive: stale README reference:' "$OUT" >/dev/null ||
   ! grep -F "$readme_path" "$OUT" >/dev/null; then
    stale_failed=1
    printf '%s\n' '# stale README refusal assertion failed'
fi
if [ -s "$SENTINEL_LOG" ] || ! no_proof_outputs; then
    stale_failed=1
    printf '%s\n' '# stale README refusal reached a sentinel or created proof output'
fi
record "$stale_failed" 'preflight rejects a stale active-phase README reference'
if [ "$stale_failed" -ne 0 ]; then
    sed 's/^/# command output: /' "$OUT"
fi

printf '1..%s\n' "$test_number"
[ "$failures" -eq 0 ]
