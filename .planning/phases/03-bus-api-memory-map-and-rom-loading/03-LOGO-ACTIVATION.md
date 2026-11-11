# Owner-authorized logo authority activation

## Authorization and provenance

On 2026-11-10 the owner supplied two existing local GBA file paths, explicitly
authorized agent-managed ignored copies, and asked the agent to perform the
digest extraction and required follow-through. This supersedes the earlier
owner-must-run-the-helper procedure for these inputs; it does not grant rights
to publish either file or include them in public CI.

Both inputs were copied to the existing ignored `/private/owner-roms/` folder.
`git check-ignore` accepts each destination and `git ls-files -- private` is
empty. Their original provenance and redistribution license are unverified.
Private per-file digests and provenance records remain inside that ignored
folder. No game, BIOS or logo bytes are added to source control or public CI.

The existing helper computed the same 156-byte-window SHA-256 from both local
files. Only that resulting authority row is added to
`tests/signatures/logo.sha256`. Identity of two supplied windows does not
establish original-cartridge provenance, redistribution rights or game accuracy.

## Implemented change

- Profile version increases from 2 to 3 because the configured production
  diagnostic changes from unchecked to checked against the authorized row.
  The profile schema, boot mode, cartridge scope and machine policies are unchanged.
- Profile narrative, API and verification docs describe checked match/mismatch,
  advisory load behavior, and the distinction between private evidence and CI.
- Profile mutation controls use version 3 as the current anchor and version 4
  as the deliberately changed version; their rejection/positive intent is unchanged.
- The production-mode synthetic loader tests now require checked mismatch,
  rather than accepting either checked result. No copyrighted window is embedded.
- The original owned-pattern synthetic core still proves the general match and
  mismatch mechanism. Pending-authority parsing and tests remain supported.

## Measured local evidence

- Canonical CI workflow passes: 295 checks run, one permitted owner-local skip;
  inventory reconciliation passes. Test time is 63.57 seconds.
- Fresh installed-AppleClang ASan/UBSan configure, build and current inventory
  pass: 42 checks run, zero skipped; inventory reconciliation passes. Test time
  is 31.84 seconds.
- Both owner-provided files pass the existing local digest-authority cross-check.
- A local host harness links the actual configured production library and reads
  only each file's minimum header into memory. Both report checked match. In-memory
  changes to the first and last logo-window bytes, and an all-zero window,
  report checked mismatch while still loading. Changes just outside the window
  retain checked match. Reset and refused short replacement preserve diagnostics.
  The same witnesses pass against the freshly instrumented production library.
- Scanning the private directory deliberately refuses both inputs with the real
  `logo-window` rule. This confirms detection without staging any game bytes.

Logs and the byte-free local harness source are retained under
`build/phase03-logo-activation/`: `ci.log`, `asan.log`, the lane JUnit snapshots,
`owner-header.log`, `owner-header-asan.log`, `private-scan.log` and
`owner_header_check.c`. Private input contents are not included in this report.
The harness reads supplied bytes at runtime; its source contains no logo data.

These observations establish the configured header comparison and refusal/reset
postconditions. They are not a game boot, CPU execution, BIOS, save-device,
hardware-accuracy or broad-compatibility result.

## Acceptance boundary

The owner-data prerequisite is now supplied and exercised. Prior pending-only
reports remain historical evidence. Refresh the phase verification against this
activation. Keep any judgment-tier prohibition flags visible rather than
silently treating an automated review as owner acceptance. ROM-tail hardware
evidence remains a prerequisite for a future hardware claim, not for accepting
the current explicitly unverified software policy.
