#!/bin/sh
set -eu

die() {
  printf 'toolchain: %s\n' "$1" >&2
  exit 1
}

SCRIPT_DIR=$(CDPATH= cd "$(dirname "$0")" && pwd -P) || die script-directory
PROJECT_ROOT=$(CDPATH= cd "$SCRIPT_DIR/.." && pwd -P) || die project-directory
LOCK="$PROJECT_ROOT/toolchain/arm-none-eabi.lock"
ROOT=build/toolchains
REQUESTED_HOST=
CHECK_ONLY=false
CURL_BIN=${CURL:-curl}
TAR_BIN=${TAR:-tar}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --lock)
      [ "$#" -ge 2 ] || die missing-lock-path
      LOCK=$2
      shift 2
      ;;
    --root)
      [ "$#" -ge 2 ] || die missing-install-root
      ROOT=$2
      shift 2
      ;;
    --host)
      [ "$#" -ge 2 ] || die missing-host
      REQUESTED_HOST=$2
      shift 2
      ;;
    --check)
      CHECK_ONLY=true
      shift
      ;;
    *) die unknown-argument ;;
  esac
done

[ -f "$LOCK" ] || die lock-missing

# Lock fields intentionally contain only space-separated portable tokens.
set -f
format_seen=false
release_seen=false
darwin_row=
linux_x64_row=
linux_arm64_row=
while IFS= read -r line || [ -n "$line" ]; do
  [ -n "$line" ] || continue
  case "$line" in
    format=*)
      [ "$format_seen" = false ] || die lock-duplicate-format
      [ "$line" = format=gamebloke.arm-none-eabi/1 ] || die lock-format
      format_seen=true
      ;;
    release=*)
      [ "$release_seen" = false ] || die lock-duplicate-release
      release=${line#release=}
      [ "$release" = 15.2.1-1.1 ] || die lock-release
      release_seen=true
      ;;
    *)
      set -- $line
      [ "$#" -eq 6 ] || die lock-row-fields
      row_host=$1
      row_bytes=$2
      row_sha=$3
      row_url=$4
      row_redirect_host=$5
      row_root=$6
      case "$row_host" in
        darwin-arm64) [ -z "$darwin_row" ] || die lock-duplicate-host; darwin_row=$line ;;
        linux-x64) [ -z "$linux_x64_row" ] || die lock-duplicate-host; linux_x64_row=$line ;;
        linux-arm64) [ -z "$linux_arm64_row" ] || die lock-duplicate-host; linux_arm64_row=$line ;;
        *) die lock-unknown-host ;;
      esac
      case "$row_bytes" in ''|*[!0-9]*) die lock-byte-count ;; esac
      case "$row_sha" in *[!0123456789abcdef]*|'') die lock-sha256 ;; esac
      [ "${#row_sha}" -eq 64 ] || die lock-sha256
      expected_url="https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v15.2.1-1.1/xpack-arm-none-eabi-gcc-15.2.1-1.1-${row_host}.tar.gz"
      [ "$row_url" = "$expected_url" ] || die lock-url
      [ "$row_redirect_host" = release-assets.githubusercontent.com ] || die lock-redirect-host
      [ "$row_root" = xpack-arm-none-eabi-gcc-15.2.1-1.1 ] || die lock-archive-root
      ;;
  esac
done < "$LOCK"
[ "$format_seen" = true ] || die lock-format-missing
[ "$release_seen" = true ] || die lock-release-missing
[ -n "$darwin_row" ] && [ -n "$linux_x64_row" ] && [ -n "$linux_arm64_row" ] || die lock-hosts

SYSTEM=$(uname -s 2>/dev/null || die host-detection)
MACHINE=$(uname -m 2>/dev/null || die host-detection)
case "$SYSTEM:$MACHINE" in
  Darwin:arm64|Darwin:aarch64) NATIVE_HOST=darwin-arm64 ;;
  Linux:x86_64|Linux:amd64) NATIVE_HOST=linux-x64 ;;
  Linux:aarch64|Linux:arm64) NATIVE_HOST=linux-arm64 ;;
  *) die unsupported-native-host ;;
esac
HOST=${REQUESTED_HOST:-$NATIVE_HOST}
[ "$HOST" = "$NATIVE_HOST" ] || die foreign-host

case "$HOST" in
  darwin-arm64) selected_row=$darwin_row ;;
  linux-x64) selected_row=$linux_x64_row ;;
  linux-arm64) selected_row=$linux_arm64_row ;;
  *) die unsupported-host ;;
esac
set -- $selected_row
HOST=$1
ARCHIVE_BYTES=$2
ARCHIVE_SHA=$3
ARCHIVE_URL=$4
REDIRECT_HOST=$5
ARCHIVE_ROOT=$6

[ -n "$ROOT" ] || die empty-install-root
case "$ROOT" in /) die unsafe-install-root ;; esac
case "$ROOT" in /*) ;; *) ROOT="$PROJECT_ROOT/$ROOT" ;; esac
mkdir -p "$ROOT" || die install-root
ROOT=$(CDPATH= cd "$ROOT" && pwd -P) || die install-root
INSTALL_PARENT="$ROOT/arm-none-eabi"
if [ -L "$INSTALL_PARENT" ]; then die install-parent-symlink; fi
mkdir -p "$INSTALL_PARENT" || die install-parent
INSTALL_PARENT=$(CDPATH= cd "$INSTALL_PARENT" && pwd -P) || die install-parent
case "$INSTALL_PARENT" in "$ROOT/arm-none-eabi") ;; *) die install-parent-outside-root ;; esac
INSTALL="$INSTALL_PARENT/$HOST-$release"

verify_tools() {
  bin="$INSTALL/bin"
  [ -x "$bin/arm-none-eabi-as" ] || die assembler-missing
  [ -x "$bin/arm-none-eabi-readelf" ] || die readelf-missing
  [ -x "$bin/arm-none-eabi-gcc" ] || die gcc-missing
  as_version=$("$bin/arm-none-eabi-as" --version 2>/dev/null | sed -n '1p') || die assembler-unusable
  readelf_version=$("$bin/arm-none-eabi-readelf" --version 2>/dev/null | sed -n '1p') || die readelf-unusable
  gcc_version=$("$bin/arm-none-eabi-gcc" --version 2>/dev/null | sed -n '1p') || die gcc-unusable
  case "$gcc_version" in *15.2.1*) ;; *) die gcc-version-mismatch ;; esac
  printf 'toolchain-check PASSED host=%s release=%s archive_sha256=%s\n' "$HOST" "$release" "$ARCHIVE_SHA"
  printf 'assembler: %s\nreadelf: %s\ncompiler: %s\n' "$as_version" "$readelf_version" "$gcc_version"
}

if [ "$CHECK_ONLY" = true ]; then
  [ -d "$INSTALL" ] || die install-missing
  receipt="$INSTALL/install.json"
  [ -f "$receipt" ] || die receipt-missing
  grep -Fq "\"host\": \"$HOST\"" "$receipt" || die receipt-host-mismatch
  grep -Fq "\"release\": \"$release\"" "$receipt" || die receipt-release-mismatch
  grep -Fq "\"archive_bytes\": $ARCHIVE_BYTES" "$receipt" || die receipt-size-mismatch
  grep -Fq "\"archive_sha256\": \"$ARCHIVE_SHA\"" "$receipt" || die receipt-hash-mismatch
  grep -Fq "\"archive_root\": \"$ARCHIVE_ROOT\"" "$receipt" || die receipt-root-mismatch
  verify_tools
  exit 0
fi

if [ -e "$INSTALL" ] || [ -L "$INSTALL" ]; then
  die install-exists-use-check
fi
command -v "$CURL_BIN" >/dev/null 2>&1 || die curl-missing
command -v "$TAR_BIN" >/dev/null 2>&1 || die tar-missing
command -v cmake >/dev/null 2>&1 || die sha-tool-missing

umask 077
WORK=$(mktemp -d "$ROOT/.arm-none-eabi-$HOST.XXXXXX") || die staging-directory
cleanup() {
  if [ -n "${WORK:-}" ] && [ -d "$WORK" ]; then rm -rf "$WORK"; fi
}
trap cleanup 0
trap 'exit 1' HUP INT TERM

archive="$WORK/archive.tar.gz"
redirect_body="$WORK/redirect-body"
curl_error="$WORK/curl-error"
newline='
'
download_start=$(date +%s)
if ! redirect_meta=$("$CURL_BIN" -q --silent --show-error --globoff \
  --proto '=https' --connect-timeout 20 --max-time 600 --max-filesize "$ARCHIVE_BYTES" \
  --write-out '%{http_code}\n%{redirect_url}' --output "$redirect_body" --url "$ARCHIVE_URL" 2>"$curl_error"); then
  die download-failed
fi
redirect_status=${redirect_meta%%"$newline"*}
redirect_url=${redirect_meta#*"$newline"}
[ "$redirect_status" = 302 ] || die redirect-status
case "$redirect_url" in
  "https://$REDIRECT_HOST/"*) ;;
  *) die redirect-host-or-scheme ;;
esac
rm -f "$redirect_body"

if ! final_status=$("$CURL_BIN" -q --silent --show-error --globoff \
  --proto '=https' --connect-timeout 20 --max-time 600 --max-filesize "$ARCHIVE_BYTES" \
  --write-out '%{http_code}' --output "$archive" --url "$redirect_url" 2>/dev/null); then
  die redirected-download-failed
fi
[ "$final_status" = 200 ] || die redirected-status
[ -f "$archive" ] || die empty-response
actual_bytes=$(wc -c < "$archive" | tr -d '[:space:]') || die size-check-failed
[ "$actual_bytes" = "$ARCHIVE_BYTES" ] || die size-mismatch
download_end=$(date +%s)
download_seconds=$((download_end - download_start))

digest_output=$(cmake -E sha256sum "$archive" 2>/dev/null) || die digest-tool-failed
actual_sha=${digest_output%% *}
[ "$actual_sha" = "$ARCHIVE_SHA" ] || die digest-mismatch

members="$WORK/members.txt"
details="$WORK/details.txt"
types="$WORK/types.tsv"
: > "$types"
"$TAR_BIN" -tzf "$archive" > "$members" 2>/dev/null || die corrupt-archive
"$TAR_BIN" -tvzf "$archive" > "$details" 2>/dev/null || die corrupt-archive

safe_archive_path() {
  path=$1
  case "$path" in
    ''|/*|*'//'*|*'/../'*|../*|*/..|*'/./'*|./*) return 1 ;;
  esac
  case "$path" in
    *[!ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._+@/-]*) return 1 ;;
  esac
  case "$path" in
    "$ARCHIVE_ROOT"|"$ARCHIVE_ROOT/"*) return 0 ;;
    *) return 1 ;;
  esac
}

safe_symlink_target() {
  member=$1
  target=$2
  awk -v root="$ARCHIVE_ROOT" -v member="$member" -v target="$target" '
    BEGIN {
      if (target == "" || substr(target, 1, 1) == "/") exit 1
      separator = 0
      for (i = length(member); i > 0; i--) {
        if (substr(member, i, 1) == "/") {
          separator = i
          break
        }
      }
      if (separator == 0) exit 1
      parent = substr(member, 1, separator - 1)
      path = parent "/" target
      count = split(path, pieces, "/")
      depth = 0
      for (i = 1; i <= count; i++) {
        if (pieces[i] == "" || pieces[i] == ".") continue
        if (pieces[i] == "..") {
          if (depth <= 1) exit 1
          depth--
        } else {
          normalized[++depth] = pieces[i]
        }
      }
      if (depth == 0 || normalized[1] != root) exit 1
      exit 0
    }
  ' </dev/null
}

exec 3< "$details"
while IFS= read -r member || [ -n "$member" ]; do
  if ! IFS= read -r detail <&3; then die malformed-archive-listing; fi
  case "$member" in */) member=${member%/} ;; esac
  safe_archive_path "$member" || die unsafe-member-path
  type=${detail%"${detail#?}"}
  case "$type" in
    -|d|l|h) ;;
    *) die unsafe-member-type ;;
  esac
  if awk -F '\t' -v p="$member" '$1 == p { found = 1 } END { exit !found }' "$types"; then
    die duplicate-member
  fi
  printf '%s\t%s\n' "$member" "$type" >> "$types"
  case "$type" in
    l)
      case "$detail" in *' -> '*) target=${detail##* -> } ;; *) die malformed-symlink ;; esac
      case "$target" in *[!ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._+@/-]*) die unsafe-symlink-target ;; esac
      safe_symlink_target "$member" "$target" || die escaping-symlink
      ;;
    h)
      case "$detail" in *' link to '*) target=${detail##* link to } ;; *) die malformed-hardlink ;; esac
      safe_archive_path "$target" || die unsafe-hardlink-target
      awk -F '\t' -v p="$target" '$1 == p && $2 == "-" { found = 1 } END { exit !found }' "$types" || die hardlink-target-missing-or-not-file
      ;;
  esac
done < "$members"
if IFS= read -r extra <&3; then die malformed-archive-listing; fi
exec 3<&-

extract_dir="$WORK/extract"
mkdir "$extract_dir" || die extraction-directory
extract_start=$(date +%s)
"$TAR_BIN" --no-same-owner --no-same-permissions -xzf "$archive" -C "$extract_dir" 2>/dev/null || die extraction-failed
package_root="$extract_dir/$ARCHIVE_ROOT"
[ -d "$package_root" ] || die archive-root-missing
special_nodes=$(find "$package_root" \( -type b -o -type c -o -type p -o -type s \) -print) || die extraction-check-failed
[ -z "$special_nodes" ] || die unsafe-extracted-node
chmod -R u-s,g-s "$package_root" || die permission-normalization-failed
for required in arm-none-eabi-as arm-none-eabi-readelf arm-none-eabi-gcc; do
  [ -x "$package_root/bin/$required" ] || die required-executable-missing
done
"$package_root/bin/arm-none-eabi-as" --version >/dev/null 2>&1 || die assembler-unusable
"$package_root/bin/arm-none-eabi-readelf" --version >/dev/null 2>&1 || die readelf-unusable
gcc_version=$("$package_root/bin/arm-none-eabi-gcc" --version 2>/dev/null | sed -n '1p') || die gcc-unusable
case "$gcc_version" in *15.2.1*) ;; *) die gcc-version-mismatch ;; esac

notice_file="$WORK/notices.txt"
(
  cd "$package_root"
  find . -type f \( -iname 'license*' -o -iname 'copying*' -o -iname 'notice*' \) -print |
    sed 's#^\./##' | LC_ALL=C sort
) > "$notice_file" || die notice-scan-failed
[ -s "$notice_file" ] || die notice-inventory-empty
extract_end=$(date +%s)
extract_seconds=$((extract_end - extract_start))

install_stage="$WORK/install"
mv "$package_root" "$install_stage" || die promotion-staging
receipt="$install_stage/install.json"
{
  printf '{\n'
  printf '  "format": "gamebloke.toolchain-install/1",\n'
  printf '  "host": "%s",\n' "$HOST"
  printf '  "release": "%s",\n' "$release"
  printf '  "archive_bytes": %s,\n' "$actual_bytes"
  printf '  "archive_sha256": "%s",\n' "$actual_sha"
  printf '  "archive_root": "%s",\n' "$ARCHIVE_ROOT"
  printf '  "download_seconds": %s,\n' "$download_seconds"
  printf '  "extract_seconds": %s,\n' "$extract_seconds"
  printf '  "notices": [\n'
  first=true
  while IFS= read -r notice || [ -n "$notice" ]; do
    [ -n "$notice" ] || continue
    if [ "$first" = true ]; then first=false; else printf ',\n'; fi
    printf '    "%s"' "$notice"
  done < "$notice_file"
  printf '\n  ]\n}\n'
} > "$receipt" || die receipt-write-failed

mv "$install_stage" "$INSTALL" || die promotion-failed
cleanup
WORK=
trap - 0 HUP INT TERM
verify_tools
printf 'archive_bytes=%s download_seconds=%s extract_seconds=%s\n' "$actual_bytes" "$download_seconds" "$extract_seconds"
printf 'notice_inventory:\n'
sed 's/^/  /' "$INSTALL/install.json"
