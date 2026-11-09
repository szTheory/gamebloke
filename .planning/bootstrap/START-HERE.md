# Gamebloke bootstrap

Prepared 2026-11-09 as pre-GSD setup for a new Game Boy Advance emulator.
The target contained only .planning/config.json at inspection time. It had no
source code, Git repository, root AGENTS.md, bootstrap pack, or generated GSD
project, requirements, roadmap, or state files. Preserve the existing
configuration and any new user work.

## Read before kickoff

1. Root AGENTS.md for the project's standing engineering and GSD preferences.
2. .planning/bootstrap/PROJECT-SEED.md for owner direction and open project
   decisions.
3. .planning/bootstrap/EMULATOR-DNA.md for transferable emulator lessons, GBA
   research, source links, test strategy, and evidence limits.
4. The current .planning/config.json before changing GSD settings.

These bootstrap files do not replace generated OpenGSD artifacts. Do not hand
write .planning/PROJECT.md, REQUIREMENTS.md, ROADMAP.md, or STATE.md. Let the
installed GSD workflow create and validate them.

## Initialize the project

In a fresh LLM session with this repository as the working directory, run:

    $gsd-new-project --auto @.planning/bootstrap/PROJECT-SEED.md

Use the exact syntax supported by that session's installed GSD version if it
differs. The owner requested autonomous, source-grounded project setup, so use
the automatic path with the existing research, plan-check, verifier, Nyquist,
code-review, and pattern-mapping settings.

Complete project initialization and return the generated artifacts, the key
decisions and unresolved choices, and the next useful GSD command. Do not start
$gsd-plan-phase 1, implementation, builds, tests, Git initialization, or release
work in this bootstrap handoff. This target currently has no Git repository;
repo setup belongs to the project session only if its GSD discussion selects
and authorizes it.

## Configuration

The existing config already enables research, plan-check, verifier, Nyquist
validation, security checks, code review, pattern mapping, parallelization,
subagent research, and end-of-phase human verification. It had
workflow.auto_advance=false. This setup changes only that setting to true to
match the owner's standing preference for routine OpenGSD work to proceed
autonomously. All other existing settings are preserved.

Auto-advance covers ordinary reversible work. It does not accept new scope,
license or fixture rights, external publication, physical qualification, or an
irreversible decision. Batch any unavoidable human judgment at phase end where
possible; do not ask for routine acknowledgments.

## Accepted project direction and boundary

The owner selected the working name Gamebloke and the Game Boy Advance domain.
A small older Apple IIe emulator repository uses the same name; the owner
reviewed it and accepts reuse because it has no meaningful traction. The name
is not trademark clearance. GSD should use Gamebloke and should not reopen this
same collision absent materially new evidence.

This is a setup pack only. It does not select an implementation language,
public API/ABI, hardware/BIOS profile, first game or diagnostic, host platform,
LibFuturo adoption, project license, repository visibility, or release plan.
The owner is next: run the command in a fresh session from the Gamebloke
directory. No implementation or GBA behavior has been verified here.
