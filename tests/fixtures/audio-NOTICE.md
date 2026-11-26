# Gamebloke local audio diagnostic notice

`owned-audio-mix.gba` is generated from the original C17 source
`tests/fixtures/audio_image.c` at source revision
`36bff0a5df21445d3648b6776be7172c562a0e98`. It is a 0x400-byte direct-start
diagnostic written for Gamebloke. It contains no BIOS bytes, commercial game
content, or borrowed fixture data.

The owner authorized use of this original diagnostic for local verification
under the approved v4.0 Audio milestone. Its permitted use is **local-only**.
The license and redistribution status remain **unknown**; the permission does
not authorize public CI, redistribution, packaging, mirroring, publication, or
use in another project. Do not infer additional rights from the source or its
digest. These limits follow milestone decisions D-19-06 and D-19-08.

## Local verification

Run from the Gamebloke repository root:

```sh
cmake --preset ci -B build/phase19-local-audio -DGBL_LANE=local-audio
cmake --build build/phase19-local-audio --target gamebloke-run
ctest --test-dir build/phase19-local-audio --output-on-failure \
  --no-tests=error -R '^audio[.]guest[.]local$'
```

The generated image and the invalid-completion-marker negative control stay
under the ignored `build/phase19-local-audio` directory. The test observes
software-model PCM from PSG channel 2 and Direct Sound FIFO A, guest mailbox
progress, and a DMA1 FIFO refill. These checks establish reproducibility and
the selected software path only. They do not establish physical hardware
accuracy, broad game compatibility, or permission to distribute the image.
