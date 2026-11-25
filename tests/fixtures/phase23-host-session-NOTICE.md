# Phase 23 Host Session Diagnostic Notice

`phase23-host-session.gba` is a 4,096-byte original Gamebloke diagnostic image
generated from `tests/fixtures/phase23_host_session.c` by a host C17 compiler.
It contains no imported BIOS, logo, artwork, or third-party test content. The
runner records the exact source revision, compiler recipe, and SHA-256 values
for this source, this notice, and the generated image in its ignored local
fixture record.

The guest writes palette entry zero before the first visible-line boundary,
sets the project-model Direct Sound B channel after line 1, checks active-low
KEYINPUT at lines 6, 14, and 24, and writes a marker-last mailbox after line
162. The expected input samples are `0x03FE`, `0x03DF`, and `0x03FF`. The
expected first-frame backdrop is black on row 0 and BGR555LE `0x56AA` on rows
1–159. The audio capture is raw pre-PWM signed-16 stereo; the expected steady
sample is `-9472` on the left and zero on the right. These are deterministic
software-model witnesses, not hardware or compatibility claims.

The owner authorized this distinct fixture for local Phase 23 verification
only. No public CI, upload, package, or redistribution permission is granted.
The identity digests are evidence of byte identity and do not grant rights.

<!-- PHASE23_AUTHORITY_START -->
fixture_id=phase23-host-session
permitted_use=local-only
permitted_environment=local-phase23-verification
license_status=no-redistribution-license-granted
public_ci=false
upload=false
package=false
redistribution=false
profile=agb
profile_version=16
start_mode=direct
bios=none
cartridge=rom-only
<!-- PHASE23_AUTHORITY_END -->
