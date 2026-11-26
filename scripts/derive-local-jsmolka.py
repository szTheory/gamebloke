#!/usr/bin/env python3
"""Create local direct-start variants of the pinned jsmolka ROM inputs."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile


INPUTS = {
    "jsmolka-arm": (
        "arm.gba",
        "77ee88662552bdc885c1080c0172ff119d54db791bd73b21808cf1ff1fe5b40e",
        "09e9a33570f5461108ee6d1a8fcab89cdfbee518d8680ab9d3ded08574603e57",
    ),
    "jsmolka-thumb": (
        "thumb.gba",
        "b5cb2291df4ab314b31c598acd9bff2ccfa0b38efff29daadfe97422ce369b67",
        "1f9663c94b79a83213841dae656139c33c5a75193969defee08271ce2a97100b",
    ),
    "jsmolka-memory": (
        "memory.gba",
        "21024fb6aae6343f5f0466dd54e3149de1fbeb23f78e7d85a015c983684d2f87",
        "f710599178269914ab0edf5b462df01a2d851f68eaa190378de636b4fa45dcb5",
    ),
}

ENTRY_BRANCH = struct.pack("<I", 0xEA00002C)  # B 0x080000B8
STACK_BOOTSTRAP = bytes.fromhex("03d4a0e37fdc8de3")  # MOV/ORR SP = 0x03007F00
EXPECTED_ENTRY_BRANCH = bytes.fromhex("2e0000ea")
BOOTSTRAP_OFFSET = 0xB8
BOOTSTRAP_END = BOOTSTRAP_OFFSET + len(STACK_BOOTSTRAP)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def atomic_write(destination: Path, contents: bytes) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary_name = tempfile.mkstemp(
        prefix=f".{destination.name}.", suffix=".tmp", dir=destination.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(fd, "wb") as output:
            output.write(contents)
            output.flush()
            os.fsync(output.fileno())
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()

    provenance = {
        "schema": "gamebloke.local-jsmolka-derivation/1",
        "source_commit": "a7113b67e63f83a9b321696ddd7042ccfad6c881",
        "patch": {
            "entry_offset": "0x00000000",
            "entry_bytes": ENTRY_BRANCH.hex(),
            "bootstrap_offset": f"0x{BOOTSTRAP_OFFSET:08x}",
            "bootstrap_bytes": STACK_BOOTSTRAP.hex(),
            "stack_top": "0x03007f00",
            "execution": "direct-start guest instructions; no runner register override",
        },
        "fixtures": [],
    }

    for fixture_id, (source_name, source_digest, expected_derived_digest) in INPUTS.items():
        source = args.input_dir / source_name
        if source.is_symlink() or not source.is_file():
            raise SystemExit(f"derive-jsmolka: input-missing-or-unsafe:{source_name}")
        original = source.read_bytes()
        if digest(original) != source_digest:
            raise SystemExit(f"derive-jsmolka: input-digest-mismatch:{fixture_id}")
        if len(original) < BOOTSTRAP_END or original[:4] != EXPECTED_ENTRY_BRANCH:
            raise SystemExit(f"derive-jsmolka: input-layout-mismatch:{fixture_id}")

        derived = bytearray(original)
        derived[:4] = ENTRY_BRANCH
        derived[BOOTSTRAP_OFFSET:BOOTSTRAP_END] = STACK_BOOTSTRAP
        derived_bytes = bytes(derived)
        derived_digest = digest(derived_bytes)
        if derived_digest != expected_derived_digest:
            raise SystemExit(f"derive-jsmolka: output-digest-mismatch:{fixture_id}")

        destination = args.output_dir / f"{fixture_id}.gba"
        atomic_write(destination, derived_bytes)
        provenance["fixtures"].append(
            {
                "id": fixture_id,
                "source_file": source_name,
                "source_sha256": source_digest,
                "derived_file": destination.name,
                "derived_sha256": derived_digest,
            }
        )
        print(f"{fixture_id}: derived sha256={derived_digest}")

    atomic_write(
        args.output_dir / "derivation.json",
        (json.dumps(provenance, sort_keys=True, indent=2) + "\n").encode(),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
