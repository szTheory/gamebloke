# Plan09 hosted timeout stop

Status: RESOLVED. The one owner-approved corrected hosted run passed every required check. The owner replied "yes follow ur recs" to the concrete duplicate-compilation reduction, measurements, full verification and one corrected hosted run. The original stop below records the historical failure and diagnosis; it was preserved until approval. Plan10 has not started.

## Actual evidence

Existing draft PR https://github.com/szTheory/gamebloke/pull/8. Run https://github.com/szTheory/gamebloke/actions/runs/43240119634 attempt1 is on exact current published head967d7ef27ad0a95fa3a12f3cf2eeea21b5d3c85f. The Linux GNU job131258036917 was cancelled after181 seconds. Its check annotation explicitly states: "The job has exceeded the maximum execution time of 3m0s". This is a real new red gate, not a superseded run or expected mutant failure. The preceding owner-approved Plan08 leak correction passed required CI in run43228795941; that old stop remains resolved and does not authorize fixes/retries for this new failure.

The GNU job actually passed the full CTest workflow (336 pass, one existing allowed owner-local signature skip, post-inventory pass). CTest took168.90 seconds and the workflow step about175 seconds, leaving insufficient job/cleanup margin under the fixed180-second limit. No semantic test failed in that job. Hosted Linux Clang ASan/UBSan/LeakSanitizer passed80/80 with zero skips in106 seconds (CTest91.06s). Hygiene59s and both diagnostics hosts (Linux24s/macOS49s) passed. The macOS regression was still running at initial stop; final observation will be appended below. CI required cannot pass with the cancelled GNU dependency.

All prior local/native and pinnedLinux GCC/Clang results, TDD evidence and independent source review remain true for their stated configurations. Plan09 is locally complete,9/25 plans; phase-wide CPU/DIAG requirements remain pending. Plan10 has NOT started and no executor/summary exists for it.

## Read-only critical-path diagnosis

The eight CPU mutation checks took6.54–6.69 seconds each, about52.8 seconds in aggregate. tests/gates/core_mutation.cmake:gbl__run_cpu_pair invokes gbl__compile_cpu separately for mutant and clean. Each invocation recompiles version.c, machine.c, bus.c, loader.c, cpu.c, inspect.c and the entire large cpu_test.c. For six CPU-source-only mutations the test harness and several other translation units are byte-identical across the pair. They are compiled twice even though separate mutant and clean executables only need the changed CPU object to differ. This is concrete repeated compilation in the measured critical path; no speedup has been measured yet.

Header mutation shift_carry changes cpu_helpers.h used by both CPU and the test source. peek_effect changes inspect.c plus public header declarations. Those cases must rebuild all affected translation units and must never reuse an object built against the original header. Unknown/new cases should default to the safe full-compilation path unless their dependency identity is proven.

## Recommended bounded remedy (not implemented)

1. Reduce duplicate compilation within each CPU mutation check: compile demonstrably unchanged translation units once for that invocation and link them into two separate mutant/clean executables; rebuild the changed CPU translation unit separately for each. Preserve exact compiler/config/warning/hardening/sanitizer/link flags and source/header identity. Keep isolated source copies and all exact semantic negative/clean regex checks.
2. Keep header-affecting and unknown mutations on the existing safe path, or explicitly rebuild every affected unit. Add meaningful harness controls that prove the intended mutant still fails for its semantic cause, its clean companion passes, and compiler/link errors cannot masquerade as semantic evidence. Do not share artifacts across runs or introduce a persistent cache, dependency, concurrency or new CI job.
3. Measure before/after timings of the affected mutation matrix on native macOS and pinnedLinux GCC/Clang, then run the full canonical CI and full matching Linux sanitizer inventory/postcheck. Keep all required checks, the existing allowed owner-local normal skip, zero sanitizer skips and the180-second hosted limit. No emulator semantic changes are proposed.
4. Publish one corrected run on the same PR and inspect all results and critical-path timings. A new red renews the stop again. Do not simply retry the unchanged near-limit run or increase time limits.

The concrete risk is accidentally sharing an object whose headers or compile flags differ. Retaining full rebuilds for those cases and proving the semantic mutant/clean controls is the preferred low-disruption remedy. The speedup remains unmeasured and must be established before publication.

## Evidence and next actor

Full logs and API snapshots: build/phase05-plan09/hosted-job-131258036917.log, hosted-job-131258037120.log, hosted-stop-run.json, hosted-stop-jobs.json, hosted-gnu-timeout-annotations.json. The GitHub connector did not expose check-run annotations; read-only gh api confirmed the timeout. No tests or retries were launched after the stop, and source remains exactly967d7ef.

The owner decides whether to authorize this bounded build-work reduction, verification/timing measurement and one corrected hosted run. The agent is waiting for that decision. Keep user-dirty AGENTS/config/state.json/researchcache and root untracked handoffs unchanged; snapshot exact state.json bytes before GSD handlers and restore after. No private ROM/BIOS, binary upload, LICENSE/tag, settings/protection change, merge or Phase6 work is authorized by this stop handoff.


## Final hosted observation

The existing run finished cancelled because GNU exceeded3m; CI required finished failure. macOS regression passed in156s. All other substantive lanes passed (Linux sanitizer106s, diagnostics Linux24s/macOS49s, hygiene59s); the title check reports success with a stale status field in the jobs API. There is no semantic regression failure in the test logs. No new run, fix or local retest was launched; head remains967d7ef. The agent is waiting for the owner's bounded remedy decision.


## Owner-approved correction — current state

The owner approved the recommended scoped remedy. Commitf6c7de5f894a66476cd0f037e290518b0de7f477 changes only tests/gates/core_mutation.cmake. Six explicitly allowlisted CPU-source-only mutations compile six identical translation units once per pair; changed CPU objects and executables remain separate. Every copied common input is hash-checked, changed CPU sources/objects must differ, and compile/link failures remain fatal before exact semantic mutant and clean assertions. Header-affecting shift_carry/peek_effect and unknown cases retain full rebuilds. No persistent/cross-case cache, dependency, concurrency, inventory, required check, skip policy, sanitizer setting, emulator semantic or timeout change was made.

Controlled observed before/after eight-check timings: macOS Release30.24->21.15s; macOSASan33.05->25.33s; pinnedLinuxarm64GCC48.72->39.05s; Clang21ASan48.12->36.00s. Compiler/config flags match within each pair. Initial noisy native runs are excluded from the comparison. The benefit is measured locally, not a guarantee of hosted deadline fit. Exact receipts and caveats:05-09-CI-FIX.md and build/phase05-plan09/mutation-build-fix/.

Native full canonical CI passed336 checks with the existing allowed owner-local skip and post inventory. Root final full pinnedUbuntu26.04arm64GCC also passed336+1allowedskip/post, and pinnedClang21ASan/UBSan/LeakSanitizer passed80/80zero skips/post, all at exactf6c7de5. Independent scoped05-09-CI-REVIEW.md found no actionable issue. Guard-hash mismatch and forced compiler/link failure paths were statically reviewed, not force-injected; semantic negative/clean matrix ran on all four configurations. Full-phase review remains later.

The agent acts next: publish one corrected run on existing draft PR8 and inspect all required results and timings. Any new red renews the stop. No owner action is needed while that authorized run is pending; do not start Plan10 before this correction is green.


## Corrected hosted gate — resolved

Run https://github.com/szTheory/gamebloke/actions/runs/43249533205 attempt1 completed success at exactpublishedheada16dad4dd10f0585507a6a6227a4558deec629bc. Every required dependency and CI required passed. GNU/macOS regression inventories each ran336 with the existingoneallowedownerlocalskip/post; sanitizer80/80zero skip/post; both diagnostics and hygiene passed. No retry, suppression, timeout increase or check removal occurred.

Measured jobs: GNU175s, macOS154s, sanitizer146s, diagnostics22sLinux/23smacOS, hygiene56s, required5s. Every job remained under180s. GNUCTest147.88s (prior168.90s), macOS130.27s, sanitizer124.96s. The eight GNUCPU mutation checks total35.19s (prior about52.8s); source-only cases about3.76–3.82s while header-affecting full rebuilds6.23/6.28s. Thus reduced duplicate work is directly visible in hosted control timings, separate from host/build/load noise. GNU overall margin remains only5s; keep measured critical-path costs visible during subsequent plans, without claiming broad performance headroom or raising limits. Raw run/jobs and all three heavyjoblogs: build/phase05-plan09/mutation-build-fix/hosted-*.

The original Plan09 timeout stop is resolved. Resume authorized Plan10. Fullphasevalidation/review/acceptance remain later, and another new red renews stop. No owner action is required to continue.
