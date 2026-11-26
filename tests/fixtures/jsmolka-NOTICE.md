# jsmolka/gba-tests local fixture notice

The pinned upstream repository revision is `a7113b67e63f83a9b321696ddd7042ccfad6c881`.
Its `LICENSE` declares the upstream source under the MIT License. That declaration
does not establish a separate redistribution grant for the embedded Nintendo logo
bytes present in `lib/header.asm` and the prebuilt ROMs.

Gamebloke records these ROMs as local-only. Do not commit, upload, or run them in
public CI. The upstream BIOS test ROM is metadata-only and is not fetched or run;
the official-BIOS requirement produces a reasoned skip.

The recorded upstream ROM hashes establish byte identity only. They do not grant
permission to redistribute the ROMs or logo bytes.
