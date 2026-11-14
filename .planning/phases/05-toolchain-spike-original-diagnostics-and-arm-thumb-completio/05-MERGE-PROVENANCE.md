# Phase5 merge boundary: preserve producer ancestry

A cheap isolated Git-graph probe found a real conflict between the standing
squash preference and the retained immutable producer ancestor. Squashing PR8
onto main preserves every producer byte/mode but removes85c361e from main's
ancestry. The required source guard would correctly refuse that resulting main.
A merge commit preserves both the exact same tree and producer ancestry.

The probe used a disposable shared clone and synthetic commits only. It changed
no root refs, working tree, remote, settings or actual pull request. Exact graph
results are retained under ignored
`build/phase05-job-budgets/merge-preflight/result.json`.

Read-only GitHub inspection currently shows merge commits permitted; the active
main ruleset requires a pull request and required status checks, has no bypass
actors and no linear-history requirement. No settings change was made.

Recommended owner choice at the existing Phase5 acceptance/merge boundary:
allow a one-time merge commit for PR8, preserving the exact tested branch and
source guard. Later phases can return to squash because the producer will then
already be an ancestor of main. This is an exception to the standing owner
squash preference and is not yet authorized. Do not merge until the owner makes
that boundary decision. Do not weaken ancestry or fabricate a new source pin.

This future merge decision does not block current authorized PR verification,
fixture admission work after a real hosted green, or remaining Phase5 local
gates. No user action is required now; surface the concrete choice with Phase5
acceptance when the implementation is ready. This is the cheap-check-first
lesson applied before an irreversible operation could leave main red.
