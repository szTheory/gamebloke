#!/usr/bin/env python3
"""Behavioral checks for the final local Libretro fixture provenance verifier."""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VERIFY_SCRIPT = ROOT / "scripts/verify-libretro-local.sh"
MANIFEST = ROOT / "tests/fixtures/manifest.json"
NOTICE = ROOT / "tests/fixtures/playable0-NOTICE.md"
FIXTURE_IDS = (
    "owned-playable0-interactive",
    "owned-playable0-interactive-input-disabled",
    "owned-playable0-interactive-wrong-color",
)
CMAKE_BIN = os.environ.get("CMAKE", "cmake")


def verifier_program() -> str:
    script = VERIFY_SCRIPT.read_text(encoding="utf-8")
    function = script.split("manifest_expected_digest() {", 1)[1]
    return function.split("<<'PY'\n", 1)[1].split("\nPY", 1)[0]


class LocalFixtureProvenanceTests(unittest.TestCase):
    def test_manifest_and_notice_digest_match(self) -> None:
        with tempfile.TemporaryDirectory(prefix="gbl-provenance-") as scratch:
            manifest = Path(scratch) / "manifest.json"
            manifest.write_bytes(MANIFEST.read_bytes())
            for fixture_id in FIXTURE_IDS:
                with self.subTest(fixture_id=fixture_id):
                    result = subprocess.run(
                        [sys.executable, "-", str(manifest), str(NOTICE), fixture_id],
                        input=verifier_program(),
                        text=True,
                        capture_output=True,
                        check=False,
                    )

                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertRegex(result.stdout.strip(), r"^[0-9a-f]{64}$")

    def test_local_fixture_cannot_be_promoted_to_public_ci(self) -> None:
        manifest_data = json.loads(MANIFEST.read_text(encoding="utf-8"))
        fixtures = [row for row in manifest_data["fixtures"] if row["id"] in FIXTURE_IDS]
        self.assertEqual({row["id"] for row in fixtures}, set(FIXTURE_IDS))

        # This is a schema-valid rights pairing. The local fixture's recorded
        # authorization remains local-only with redistribution unknown.
        for fixture in fixtures:
            fixture["permitted_use"] = ["public-ci"]
            fixture["license"]["redistributable"] = "yes"

        with tempfile.TemporaryDirectory(prefix="gbl-provenance-control-") as scratch:
            manifest = Path(scratch) / "manifest.json"
            manifest.write_text(json.dumps(manifest_data), encoding="utf-8")
            manifest_check = subprocess.run(
                [
                    CMAKE_BIN,
                    "-DMODE=all",
                    f"-DMANIFEST={manifest}",
                    f"-DPINS={ROOT / 'pins.txt'}",
                    f"-DREGISTRY={ROOT / 'registry/disputes.json'}",
                    f"-DVOCAB={ROOT / 'contracts/evidence.json'}",
                    "-P",
                    str(ROOT / "cmake/CheckManifest.cmake"),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(
                manifest_check.returncode,
                0,
                manifest_check.stdout + manifest_check.stderr,
            )
            for fixture_id in FIXTURE_IDS:
                with self.subTest(fixture_id=fixture_id):
                    result = subprocess.run(
                        [sys.executable, "-", str(manifest), str(NOTICE), fixture_id],
                        input=verifier_program(),
                        text=True,
                        capture_output=True,
                        check=False,
                    )

                    self.assertNotEqual(
                        result.returncode,
                        0,
                        "the local fixture provenance verifier accepted public-ci use",
                    )
                    self.assertIn("local-only", result.stderr + result.stdout)


if __name__ == "__main__":
    unittest.main()
