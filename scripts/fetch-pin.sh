#!/bin/sh
# Pinned fetch with verify-before-place (EVID-03, D-15, D-16).
#
#   fetch-pin.sh [--pins <file>] [--root <dir>] <dest>
#
# Selects the pins file row `<sha256> <url> <dest>` whose third field is <dest>,
# downloads the URL into a temporary directory created inside the root (the same
# filesystem as the destination), hashes the payload with `cmake -E sha256sum`,
# and moves it to <root>/<dest> only when the digest equals the pin. A mismatch
# leaves nothing at the destination and no temporary directory behind. No
# archive is extracted; the verified file is placed as is.
#
# Defaults: --pins is pins.txt at the repository root, --root is .fixtures/ at the
# repository root (ignored by git; nothing under it is ever committed).
#
# Environment:
#   CURL                  curl program to run (default: curl)
#   CMAKE                 cmake program used for the digest (default: cmake)
#   GBL_FETCH_ALLOW_FILE  exactly 1 lets a row use a file:// URL (tests only; the
#                         committed pins.txt is https only)
#
# Row text is only ever used as a quoted argument or a case pattern, never
# evaluated. curl is limited to the row's single scheme for the request and for
# redirects, fails on an HTTP error, and has a redirect cap, timeouts, and a size
# cap. Certificate verification is never disabled.
#
# Reason codes (each prints `fetch-pin: <code>: <detail>` to stderr):
#   usage            unknown option, missing value, or not exactly one <dest>    (exit 2)
#   missing-pins     the pins file is absent or unreadable                       (exit 2)
#   no-curl          the curl program is absent                                  (exit 2)
#   bad-dest         <dest> (requested or in the row) fails the allow-list
#   no-row           no pins row has <dest> as its third field
#   bad-row          a row without exactly three fields, or a duplicate <dest>
#   bad-digest       the row digest is not 64 lowercase hex characters
#   bad-url          scheme not https (or file:// without the test variable), or an unsafe character
#   download-failed  curl failed or the file was refused
#   digest-mismatch  the downloaded bytes do not match the pin
#   symlink          the destination parent leaves the root, or the destination is a symlink
#   dest-is-dir      a directory sits at the destination
# Success prints `fetch-pin: ok: <dest>` and exits 0. Every other failure exits 1.
set -eu
set -f
LC_ALL=C
export LC_ALL

die() {
  printf 'fetch-pin: %s: %s\n' "$1" "$2" >&2
  exit "${3:-1}"
}

usage() {
  die usage 'fetch-pin.sh [--pins <file>] [--root <dir>] <dest>' 2
}

# check_dest <path>: allow-listed characters, no leading slash, no empty, . or .. part.
check_dest() {
  case $1 in
    '' | /* | */ | *//*) return 1 ;;
    *[!A-Za-z0-9._/-]*) return 1 ;;
  esac
  oldifs=$IFS
  IFS=/
  for part in $1; do
    case $part in
      '' | . | ..)
        IFS=$oldifs
        return 1
        ;;
    esac
  done
  IFS=$oldifs
  return 0
}

here=$(cd "$(dirname "$0")/.." && pwd -P)
pins=$here/pins.txt
root=$here/.fixtures
dest=''
have_dest=0

while [ "$#" -gt 0 ]; do
  case $1 in
    --pins)
      [ "$#" -ge 2 ] || usage
      pins=$2
      shift 2
      ;;
    --root)
      [ "$#" -ge 2 ] || usage
      root=$2
      shift 2
      ;;
    -*) usage ;;
    *)
      [ "$have_dest" -eq 0 ] || usage
      dest=$1
      have_dest=1
      shift
      ;;
  esac
done
[ "$have_dest" -eq 1 ] || usage

check_dest "$dest" || die bad-dest "$dest"
[ -f "$pins" ] && [ -r "$pins" ] || die missing-pins "$pins" 2

# Read every row; validate its shape; select the rows for <dest>.
want=''
url=''
matches=0
lineno=0
while IFS= read -r line || [ -n "$line" ]; do
  lineno=$((lineno + 1))
  case $line in
    '' | '#'*) continue ;;
  esac
  # shellcheck disable=SC2086
  set -- $line
  [ "$#" -ge 1 ] || continue
  case $1 in '#'*) continue ;; esac
  [ "$#" -eq 3 ] || die bad-row "line $lineno"
  if [ "$3" = "$dest" ]; then
    matches=$((matches + 1))
    [ "$matches" -eq 1 ] || die bad-row "duplicate dest: $dest"
    want=$1
    url=$2
  fi
done <"$pins"
[ "$matches" -eq 1 ] || die no-row "$dest"

case $want in
  *[!0-9a-f]*) die bad-digest "$dest" ;;
esac
[ "${#want}" -eq 64 ] || die bad-digest "$dest"

case $url in
  https://*) proto='=https' ;;
  file://*)
    [ "${GBL_FETCH_ALLOW_FILE:-}" = 1 ] || die bad-url "file:// refused: $dest"
    proto='=file'
    ;;
  *) die bad-url "scheme not allowed: $dest" ;;
esac
case $url in
  *[[:space:]\'\"\\]*) die bad-url "unsafe character: $dest" ;;
esac

curl_prog=${CURL:-curl}
command -v "$curl_prog" >/dev/null 2>&1 || die no-curl "curl not found" 2

# The temporary directory sits inside the root so the final rename never crosses
# a filesystem. EXIT removes it; each signal ends the script (a signal trap that
# only cleans up would let POSIX sh resume).
mkdir -p "$root"
tmp=$(mktemp -d "$root/.fetch.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

"$curl_prog" --fail --silent --show-error --globoff \
  --proto "$proto" --proto-redir "$proto" --max-redirs 5 \
  --connect-timeout 20 --max-time 600 --max-filesize 268435456 \
  --output "$tmp/payload" --url "$url" || die download-failed "$dest"

got=$("${CMAKE:-cmake}" -E sha256sum "$tmp/payload")
got=${got%% *}
[ "$got" = "$want" ] || die digest-mismatch "$dest: pinned $want, got $got"

case $dest in
  */*) parent=$root/${dest%/*} ;;
  *) parent=$root ;;
esac
mkdir -p "$parent" || die bad-dest "parent not creatable: $dest"
real_parent=$(cd "$parent" && pwd -P)
real_root=$(cd "$root" && pwd -P)
case $real_parent in
  "$real_root" | "$real_root"/*) ;;
  *) die symlink "$dest: parent resolves outside the root" ;;
esac
[ ! -L "$root/$dest" ] || die symlink "$dest: destination is a symlink"
[ ! -d "$root/$dest" ] || die dest-is-dir "$dest"

mv -f "$tmp/payload" "$root/$dest"
printf 'fetch-pin: ok: %s\n' "$dest"
