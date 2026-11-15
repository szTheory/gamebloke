# Phase 5 hosted CI stop and resolution

Status: resolved for Plan 03 after the owner-authorized correction and the corrected all-green hosted run. The earlier red run remains in the record as historical evidence; it was not ignored or retried without authority.

## Owner authorization

The owner replied "yes" to: "May I fix that helper and rerun local and hosted checks within the existing limits?" This authorized the narrow test-helper correction, regression controls, local macOS/native Linux verification, and the corrected required CI run on the existing draft PR #8. All existing limits and gates remained in force. A new red hosted result would renew the stop.

## Historical failure

- Draft PR: [#8](https://github.com/szTheory/gamebloke/pull/8).
- Initial run: [43085883559](https://github.com/szTheory/gamebloke/actions/runs/43085883559), source head `444f0344cf268e03048b0292f45b7b1d5dea3d4c`.
- The Linux x64 installer verified and executed the pinned tools and built/observed both original images, but `toolchain.offline` failed with `toolchain-case unsupported-native-host` because CMake script mode left `CMAKE_HOST_SYSTEM_PROCESSOR` empty. The required aggregator reflected that same failed dependency.
- The initial job took 76s of the unchanged 180s limit. Its fetch step took about 69s; receipt transfer/extraction figures of 4s/3s were partial measurements, not total setup time.
- The standing red-run stop was preserved until the owner explicitly authorized the narrow correction. The initial failure has not been reclassified as a successful run.

## Correction and resolved evidence

Commit `2a18d8e1b2f7d24b11b40627e697cfd42b60c48b` queries `uname -s` and `uname -m` in the offline helper, validates command status and nonempty values, and accepts only supported host/architecture pairs. It retains foreign-host refusal behavior and adds actual script-mode controls for empty processor metadata, supported Darwin/Linux mappings, unsupported pairs, failed `uname`, and empty output. The registered offline driver also runs the host controls and the 21-case installer `offline-all` suite, then asserts the controlled failure is propagated.

Corrected run: [43089440697](https://github.com/szTheory/gamebloke/actions/runs/43089440697), checked out at the exact full source head above. Every captured underlying job and `CI required` succeeded.

| Job | Result | Whole job | Fetch step | Diagnostics workflow | 180s margin |
| --- | --- | ---: | ---: | ---: | ---: |
| Diagnostics, Ubuntu 26.04 Linux x64 | success | 92s | 81s | 5s | 88s |
| Diagnostics, macOS 26 Darwin arm64 | success | 145s | 130s | 7s | 35s |
| Ubuntu GCC build | success | 113s | — | — | — |
| macOS build | success | 140s | — | — | — |
| GNU regression | success | 113s | — | — | — |
| ASan/UBSan | success | 100s | — | — | — |
| Hygiene | success | 61s | — | — | — |
| `CI required` | success | 6s | — | — | — |

The narrower transfer/extraction fields were Linux 4s/3s and macOS 3s/3s; they omit validation and staging and must not be called whole setup time. Toolchain setup dominates each diagnostics job, but the jobs do not repeat identical setup within a lane. No speculative caching, dependency, timeout, or bypass change was made.

Both hosts report v4T, ARM ISA and Thumb-1 for all six object/final-ELF records. The positive and broken diagnostics returned the expected semantic pass/fail at case `20481`; the raw unregistered-content case remains `GUEST_STOP` / `unregistered-content`, exit 1. Both workflows ran all four required tests and the post-test inventory with zero skips. Exact output names and unique output rows passed their controls.

Linux x64 hosted hashes are `424a460469dbdbbf98b1eb1991e26a45ae9324a2127936f81572308ba38510d9` (`smoke.gba`) and `76d20735f76c2bb8c67117c98b6488f0ab324039b8b19d768f14aecb64caa9e6` (`smoke-broken.gba`). The captured receipt compares these hashes with independently built local counterparts in ignored `build/phase05-spike/local-counterparts/roms`, whose provenance identifies the source as the existing local macOS arm64 build. Those hashes also match native Linux arm64 outputs. Literal `cmp` exit 0/0 applies only to the local macOS arm64/Linux arm64 pair, not hosted bytes. No hosted binaries were downloaded and no ROM artifacts were uploaded.

Raw run/job metadata and logs remain in ignored `build/phase05-spike/05-03-corrected-hosted-run.json`, `05-03-corrected-hosted-jobs.json`, `05-03-corrected-linux-x64.log`, and `05-03-corrected-macos-arm64.log`. The verified local counterpart metadata is `build/phase05-spike/hosted-linux-x64.json` plus `local-counterparts/LOCAL-PROVENANCE.json`; `05-03-hosted-proof.log` records the actual selector result.

## Current boundary

The initial stop is resolved for this authorized Plan 03 correction. A future red hosted run still requires a fresh stop before any fix or retry. The draft PR remains the phase's single PR. No ROM digest has been admitted; full diagnostic coverage, phase acceptance, merge, CPU expansion, BIOS/game compatibility, licensing and release qualification remain separate gates. No user action is needed for this Plan 03 closeout.

Preserved unrelated user changes remain `AGENTS.md`, `.planning/config.json`, `.planning/state.json`, and `.planning/research/.cache/`. No reset, branch deletion, release, tag, repository-setting change, artifact upload or required-check bypass occurred.
