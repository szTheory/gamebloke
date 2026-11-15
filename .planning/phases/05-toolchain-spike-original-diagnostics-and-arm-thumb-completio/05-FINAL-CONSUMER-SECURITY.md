# Independent consumer repair security follow-up

The independent security auditor returned SECURED for the nine-source-file
consumer repair and its report, with no blocking or nonblocking open threats.
This is a bounded follow-up to the prior phase threat audit, at ASVS level 1
with a high-severity blocking threshold. It does not qualify hosted execution
or complete the phase. The parent persisted the auditor's returned verdict
because that role is read-only under the single-writer protocol.

## Verified boundaries

| Threat mapping | Disposition | Evidence |
| --- | --- | --- |
| T-05-03 | Mitigated | The minimal smoke target calls the unchanged builder. Verified tools, object/final ELF attributes, disassembly, unresolved symbols, signature scans and actual positive/broken execution remain required. |
| T-05-05, T-05-47 | Mitigated for current repair | Admitted ARM/all consumes admission-smoke; proof retains final-smoke; Thumb registers no ARM smoke test. Registration and minimal prerequisite are checked before corpus preparation. |
| T-05-07 | Mitigated | Canonical generation validates output containment before cleanup/writes, then current contract, registry, control and full production manifests. Artifact identity checks precede emission. |
| T-05-09, T-05-41 | Mitigated | Preparation checks the exact fixture set, pinned source authority, ancestry/modes, rights/tools and actual hashes before copying. |
| T-05-10 | Mitigated | Admission renders validated current rows and checks actual result status, case ID, digest and progress through the canonical result checker. |
| T-05-42 | Mitigated | Child failure propagates and safely invalidates all three generated metadata files. Source/output/pin/index refusal controls have current passing evidence. |
| T-05-45 | Mitigated | Required lanes cannot pass with missing tools; receipts require complete admitted execution, the exact output set and rechecked identities. |
| T-05-49 | Mitigated | Normal runner build dependencies remain; receipts bind the actual post-build runner, and qualification/image-digest refusal remains active. |

The pure renderer consumes current in-memory rows, rejects unsafe C-string
characters and reads no serialized success cache. The trusted internal callback
runs after full canonical manifest validation and cannot replace later source,
artifact or rendering checks. There is no command-line callback-path option.

Containment resolution precedes reads, writes and cleanup. Escaping and dangling
symlinks refuse without touching their targets. Explicit wrapper arguments and
missing inputs refuse safely; invalid groups refuse before copying. Admitted
qualification is forced on; standalone native/proof paths keep canonical
validation and proof remains unqualified. Normal source/options/dependency
checks remain attached to runner generation.

Implementation references: [canonical generator](../../../cmake/GenerateFixtureIndex.cmake),
[renderer](../../../cmake/FixtureIndexRender.cmake),
[preparation](../../../tests/gates/prepare_diagnostic_artifacts.cmake),
[admitted entry](../../../tests/gates/admitted_index_generation.cmake),
[admission](../../../tests/gates/diagnostic_admission.cmake),
[setup](../../../tests/gates/diagnostic_setup.cmake),
[failure wrapper](../../../tests/gates/diagnostics_case.cmake),
[receipt checks](../../../tests/gates/diagnostic_receipt.cmake),
[path containment](../../../cmake/PathContainment.cmake).

## Evidence and limits

The auditor independently checked the ten candidate file hashes/modes, terminal
receipt hashes and seven JUnit hashes/inventories, and confirmed the extracted
preparation body is text-identical. Saved actual cold/refusal, smoke execution,
timestamp reuse, qualification-repair, cleanup/recovery and sanitizer-boundary
evidence was inspected without repeating heavy suites. No unregistered flag,
network source, package installation, rights grant or upload path was found.

After this audit, only tests/toolchain.cmake's minimal-graph guard changed.
The independent correctness reviewer closed its warning using actual graph
mutations and clean cold/warm/spaced-path companions; see
[the review](05-FINAL-CONSUMER-REVIEW.md). Producer, runtime, preparation,
renderer and security boundaries audited above are unchanged.

Local Linux ARM64 exceeds the existing 180-second hosted limit and cannot
predict hosted x64 timing. The corrected 40.26-second measurement is diagnostics
execution, not provisioning. Current hosted qualification remains pending; a
new hosted red preserves the stop. No risk acceptance or transfer is claimed.
