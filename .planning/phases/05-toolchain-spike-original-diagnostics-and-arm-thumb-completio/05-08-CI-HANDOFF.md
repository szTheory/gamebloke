# Phase 05 Plan 08: hosted test-fixture leak stop

Status: OWNER-AUTHORIZED CORRECTION VERIFIED LOCALLY; corrected hosted run pending. The owner replied "Yes." to the concrete cleanup, allocation-balance check, matching local Linux sanitizers and one corrected hosted run. The original stop below records the historical failure and diagnosis; that stop was preserved until this approval. Plan 09 has not started.

## Actual result

- Existing draft PR: https://github.com/szTheory/gamebloke/pull/8
- Failed run: https://github.com/szTheory/gamebloke/actions/runs/43167646132 (attempt 1).
- Exact published head: `b0fe8812de9151b1e42f4bac1d6193e8f1ee9564`.
- Linux/GNU regression, macOS regression, both diagnostic hosts, hygiene, title and classifier passed. Sanitizer job `131013212024` failed; CI required failed for that dependency.
- Sanitizer CTest passed 77 of its 78 required checks. Only `cpu.psr` failed. LeakSanitizer reports one leaked allocation of 192 bytes, allocated by `gbl_load_rom`, via the native test helper and `test_psr_mode_reads_and_banks` at `tests/cpu_test.c:2754`.
- The intentional undefined-shift and heap-overflow negative controls passed. Their later sanitizer diagnostics are expected controls, not the cause of this failure.
- No timeout occurred. GNU took 142 seconds, macOS regression 140 seconds, sanitizers 120 seconds (104.11 seconds CTest), hygiene 58 seconds, diagnostics 39 seconds macOS / 64 seconds Linux. All retained 180-second limits. The leak failed its own test in 0.22 seconds; timing changes are not a remedy. No low-risk duplicate setup/build removal was established in this slice.

## Read-only diagnosis

The bank-visibility block in `test_psr_mode_reads_and_banks` creates a zeroed local machine, calls `machine_load(&machine, image)` at line 2754, executes the intended mode/bank transitions and captures the assertion result at line 2768. It then leaves scope at line 2769 without `gbl_machine_dispose`. The local stack object disappears while its owned ROM allocation remains live. The preceding per-mode test loops already dispose their machines.

`machine_load` delegates to `gbl_load_rom`, which correctly allocates the owned ROM copy. `gbl_machine_dispose` is the existing caller-owned release operation; it safely returns for an uninitialized/already disposed machine. The observed defect is missing cleanup in the test fixture, not evidence of a leak in the emulator's loader or CPU implementation.

Concrete proposed insertion, not applied:

```c
bank_visibility = ran;
gbl_machine_dispose(&machine);
```

The verification gap is concrete: this slice had native AppleClang ASan/UBSan and Linux/GCC checks, but did not run the Linux/Clang leak-checking configuration locally before publication. Those earlier checks remain recorded evidence for their actual configurations, not substitutes for the failed hosted configuration.

## Recommended bounded correction

1. Add unconditional disposal at the end of that owned fixture block, preserving its semantic bank assertions and all failure paths.
2. Add a portable allocation-balance regression for this fixture through the existing allocator seam, so missing release is visible without relying on platform-specific leak detection. Keep it scoped; no new CI jobs or dependencies are needed. Use a meaningful live-allocation/byte postcondition rather than merely checking whether a function was called.
3. Verify the focused PSR/resource-lifetime behavior and affected CPU tests, then run the matching reviewed pinned Ubuntu/Clang 21 ASan/UBSan/LeakSanitizer workflow locally, including full inventory and the existing negative controls. Leave leak detection enabled. Use a tracked-source clone to avoid copying ignored builds/private files; the existing Linux helpers already establish the image and package set.
4. Publish one corrected run on the same PR and inspect every result. Keep all required checks, zero sanitizer skips and existing 180-second limits. A future new red hosted result renews the stop again.

This is proposed work only. The owner's preceding approval covered the prior installer correction and its bounded verification; it does not waive the standing new-red-CI stop.

## Preserved state and evidence

- Plans 01–08 have local summaries. Plan 08 has a clean scoped source review after four test-coverage gaps were fixed, but its hosted integration gate is now unresolved. Do not claim the phase complete or merge.
- Plan 09 is the next plan and has NOT started. No Plan 09 summary or implementation agent was created. The bookkeeping pointer was briefly set to executing during the job-status fetch; it is corrected to this stop. No Plan 09 source change occurred.
- CPU implementation remains f862a70; the test-only review follow-up is 673a7dc. Root's production smoke-pair run passed both actual production-index/runner/checker results. All 15 frozen original producer inputs and both smoke hashes/source 4857ddc remain unchanged. 356 ledger rows and phase-wide CPU/DIAG requirements remain pending.
- No private ROM/BIOS access, binary publication, LICENSE, tag, reset, branch deletion, repository setting change or protection bypass occurred. Main was not merged by this work.
- Preserve dirty user AGENTS.md, config.json, state.json, research cache, existing root handoffs and full branch ending `-completion`; the phase directory spelling ends `-completio`.
- Raw complete failure evidence: `build/phase05-plan08/hosted-asan-leak.log`, `hosted-failed-jobs.json`, `hosted-failed-run.json`. Original local evidence and root production smoke log remain in `build/phase05-plan08/` and `build/phase05-review08/`.

## Authorized correction and next actor

The owner approved the bounded correction. Commit `819aac865e472a8d12d55716c2fef864f249db2b` changes only tests/cpu_test.c: unconditional disposal, successful/live allocation and byte accounting, a live-before/zero-after assertion and a refused-allocation count check. The new balance assertion failed before disposal was inserted (saved red control), then passed with disposal. Native normal and AppleClang ASan/UBSan each passed all 29 CPU groups. Independent scoped source re-review has no open findings; full-phase review remains later.

Root ran focused cpu.psr and the entire 78-test ASan/UBSan/LeakSanitizer inventory plus post-inventory check in the pinned Ubuntu 26.04 native arm64 container with Clang 21 at that exact commit. All passed, zero skips; leak detection was not disabled and both intentional sanitizer negative controls passed. Full test time was 153.09 seconds. This local arm64 result does not replace hosted x64 evidence. Logs and the tracked-clone helper are under build/phase05-plan08/leak-fix/.

The agent acts next: publish one corrected run on the existing draft PR #8 and inspect every required result and job timing. Any new red renews the stop. No owner action is needed while that authorized run is pending. Do not begin Plan 09 before this correction is green.
