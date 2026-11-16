# Timer 0 polling guest fixture notice

## Authorship and source binding

These instruction images are original Gamebloke test inputs generated from
`tests/fixtures/instruction_image.c`. They do not copy or derive from an
external ROM, BIOS, firmware image, or third-party test suite. The control
manifest records the generated image path and the SHA-256 digest of the exact
output bytes. Its build recipe also records the SHA-256 digest of the generator
source, and its toolchain identity records the C compiler selected by the active
CMake configuration. Those digests bind identities; they do not grant rights.

## Build and execution scope

The `gbl_instruction_image` CMake target emits both 0x240-byte images only into
the build tree when configured with the `local-timer0` lane. The bytes use a
zero-filled header area and direct-start ARM instructions to set up Timer 0,
poll its IF bit, and then write the project's existing mailbox completion
record. The Timer 0 control's IRQ-enable bit produces the modeled IF request;
the guest polls IF directly and does not use CPU IRQ exception entry. IE and
IME are not implemented by the current model, so the guest does not access
them. The disabled control is generated from the same instruction layout and
differs only in the Timer 0 enable bit. Both require no BIOS. The `ci`, `asan`,
`hygiene`, and `diagnostics` lanes do not generate the Timer 0 images, pass them
to control-index generation, or register the guest execution test.

To exercise the guest locally, configure and build the dedicated lane, then
run its focused test:

```sh
cmake -S . -B build/local-timer0 -DGBL_LANE=local-timer0
cmake --build build/local-timer0
ctest --test-dir build/local-timer0 --output-on-failure -R '^timer0[.]guest[.]cli$'
```

The `local-timer0` lane refuses to configure when the `CI` environment variable
is set to a truthy value.

## License and permitted use

The fixture manifest uses `LicenseRef-Gamebloke-Synthetic-Control`, identifies
this notice, sets redistributability to `unknown`, and limits permitted use to
`local-only`. This notice does not add a redistribution grant or change the
project's license status. Keep generated image bytes in ignored build output;
do not commit or upload them, or use them in public CI.
