# Phase 05 capability and assumption coverage

No external API integration: this phase extends the existing local C17 interpreter, original assembly diagnostics and CMake validators; the release asset is a narrowly pinned developer-tool fetch, not broad GitHub API/service adoption. The public CPU exception API is existing project code, not an external SDK capability. No external API surface is silently partially integrated.

The full plan-scope detector reported detected=true from the existing CPU API surface. This is a reasoned false-positive declaration under the capability contribution, not an ignored or empty probe result. The roadmap-only detector reported detected=false.

## Assumption-delta decision

The installed detector reports detected=true from the roadmap's fallback cue. The actual generalization is source identity versus emitted artifact identity. Promote output-byte identity to the primary identity for execution; retain git revision as the reviewed source identity and exact host tool archive as build identity. This is not two competing required ROM identities. Optional build.artifact.path/sha256 populates the existing immutable rom_sha256 slot; URL/in-tree retain their existing binding and legacy git without artifact retains explicit digest-unavailable refusal. result/1 stays unchanged.

<assumption_delta_decision>
Noun: loaded ROM artifact bytes.
Decision: promote.
Rationale: a source commit cannot identify generated output bytes; the optional generalized artifact representation supplies runtime identity while preserving source provenance and all legacy variants.
Invariant: an admitted source-owned artifact round-trips through immutable production metadata and actual result checking; a source/output/pin/index mismatch refuses before guest execution. Plans04/05/21 implement the required identity contract and controls.
</assumption_delta_decision>

## Capability applicability

| Capability | Decision and reason |
| --- | --- |
| Internal CPU/API | Full scoped ARM/Thumb and actual exception API evidence; no stable public ABI promise. |
| Toolchain | Exact3 host asset pins and narrow fetch/extract; actual archived components/notices inspected at execution. |
| ORM schema push | Not applicable: no database/ORM/schema migration files. |
| AI/UI design | Not applicable: no AI system or user interface. |
| Security | ASVS1, high/critical blocking; unique per-plan STRIDE rows plus conditional shared SC. |
| npm/pip/cargo installs | None; no legitimacy table/checkpoint invented. A newly introduced install activates the conditional gate. |

## Planned interface additions

- Tool lock fields/host records; fetch --lock/--root/--host/--check; generated install receipt.
- Original startup/link/header/assertion/case source; explicit per-lane builder SRC/OUT/TOOLCHAIN_ROOT/MODE/HOST/ISA/CASE_ID/BROKEN; exact input list and build receipts.
- GBL_TOOLCHAIN_ROOT, GBL_REQUIRE_DIAGNOSTICS (OFF local, ON required), GBL_DIAGNOSTIC_STAGE proof/admitted, GBL_LANE diagnostics and matching preset/inventory.
- manifest/1 optional build.artifact.path/sha256; build-root GBL_ARTIFACT_ROOT; unchanged source.commit/toolchain/result1 roles.
- Ephemeral CPU slot/result/effect capacities and symbolic loaded-PC dependency, PC-use context and decoded internal-cost tables; no planned persistent machine-state fields.
- Additive unpredictable/later-architecture reason enums/strings, existing stable values retained; coherent profile5 scope/policies/docs/controls.
- cases.json stable IDs/family/form/setup/observe/independent expected/source/fixture/native/trace/coverage-state fields plus execution_boundary/expected_status/companion_case_ids.
- Exact task-created test selectors and commands are mapped in 05-VALIDATION.md. Every PLAN carries its complete file/interface artifact list.
