# Final original diagnostic admission

Plan21 is preparing complete final-source proof. No new output digest or
fixture row has been admitted. Earlier smoke/ARM/Thumb receipts remain
historical and cannot establish current complete parity.

The source-only authority is the committed HEAD used by
`cmake -DCASE=final-output-proof -DSRC=. -DBIN=build/phase05-final-parity -DTOOLCHAIN_ROOT=build/toolchains -P tests/gates/toolchain_spike.cmake`.
The builder validates every sorted path in `diagnostics/armv4t/inputs.txt`
against that exact Git commit before constructing smoke, ARM and Thumb
positive/broken/guest-stop outputs. Host archive identity is separate from
source identity and from generated ROM SHA-256.

Local proof uses tracked-only `git clone --no-local` checkouts, actual macOS
arm64 and a native Linux arm64 container pinned to the existing Ubuntu26.04
image digest. Verified existing host tool installs are reused read-only.
The complete sorted output-name sets must agree, followed by literal `cmp`
of every corresponding ROM. Existing canonical output roots are preserved.
Per-image object/final-ELF v4T, executable sections/disassembly, exact original
object map, unresolved-symbol and exact-name generated signature checks remain
mandatory. A mismatch is preserved and stops admission for a parent decision
about explicit Linux ownership and host-specific binding.

Local macOS proof runs the canonical diagnostics workflow once, including its
actual build and registered controls. Selected matrix children label signature
proof deferred/unverified; only the mandatory parent scan of the exact final
output directory establishes scanned authority. Standalone selected cases
retain their own scan. The complete proof consumer requires the successful
parent witness and exact output set before accepting any deferred child.
Map and header validation execute the same standalone scripts in fresh function
scopes to avoid duplicate process startup without removing their checks.

The diagnostics workflow retains the existing pinned actions, exact PR-head
checkout with full history, required jobs and180s limits. Proof mode builds
the entire selected output set and emits each output's name, stable case ID,
byte count, SHA-256, source revision and host tool identity in logs. These
pre-admission outputs never claim registered fixture PASS. Normal metadata-only
lanes continue using the existing admitted manifest without requiring tools.
No ROM artifact is uploaded.

Before admission the parent publishes the committed proof head through the
existing phase PR and supplies actual GitHub run/jobs API documents plus both
diagnostic jobs' complete log-derived host receipts. `final-hosted-proof`
requires the current exact source HEAD, complete source/output/case sets,
both named host jobs, every required job green and every job duration at most
180s. Hosted SHA-256 identity is a hash witness, distinct from local literal
comparison. The focused controls use explicitly synthetic parser inputs only;
their success is never hosted evidence. A new hosted red preserves the standing
stop before any fix or retry.

Actual local receipts and the publication checkpoint live under ignored
`build/phase05-plan21/` and `build/phase05-final-parity/`. Task2 remains blocked
until the parent provides real green hosted proof. It will pin only these
final proven bytes, exact source-only revision, recipe/input/tool identity,
fixture-scoped NOTICE/public-CI permission, BIOS-none mode and independent
expectation. Semantic pairs and actual named guest stops remain separate;
CPU API witnesses remain lower-boundary evidence.

No BIOS execution, physical accuracy, broad game compatibility, general binary
redistribution, project license, release or Phase6 claim follows.
