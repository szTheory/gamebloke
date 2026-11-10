#!/bin/sh
# Conventional Commits pull request title gate (D-14).
#
#   check-title.sh <title>
#
# Accepts `type(scope)!: subject` with type one of feat, fix, docs, test, ci,
# build, refactor, perf, chore, revert, style. The scope and the `!` are
# optional; the subject starts right after one space.
#
# The title is attacker-controlled text. It arrives as one quoted argument, is
# matched with grep -E, and is never evaluated or interpolated into a command.
#
# Reason codes (standard error, except `ok`) and exit codes:
#   title: ok            exit 0
#   title: empty         exit 1
#   title: too-long      exit 1  (over 100 characters)
#   title: not-conventional  exit 1
#   title: usage         exit 2  (not exactly one argument)
set -eu
LC_ALL=C
export LC_ALL

if [ "$#" -ne 1 ]; then
  echo "title: usage: check-title.sh <title>" >&2
  exit 2
fi
title=$1

if [ -z "$title" ]; then
  echo "title: empty" >&2
  exit 1
fi
if [ "${#title}" -gt 100 ]; then
  echo "title: too-long (${#title} characters, limit 100)" >&2
  exit 1
fi
# A title is one line. grep matches per line, so refuse a second line outright.
case $title in
  *'
'*)
    echo "title: not-conventional (more than one line)" >&2
    exit 1
    ;;
esac
if ! printf '%s\n' "$title" |
    grep -Eq '^(feat|fix|docs|test|ci|build|refactor|perf|chore|revert|style)(\([a-z0-9._/-]+\))?!?: [^ ]'; then
  echo "title: not-conventional (want 'type(scope)!: subject')" >&2
  exit 1
fi
echo "title: ok"
