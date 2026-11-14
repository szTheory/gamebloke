#!/usr/bin/env python3
"""Local-only actual-log and isolated synthetic collector/CMake controls.

Never creates live hosted evidence. Synthetic identities/host fields remain in
a marked disposable tree with every API document explicitly tagged.
"""
import argparse
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path("."))
    parser.add_argument("--local-log", type=Path, required=True)
    parser.add_argument("--local-proof", type=Path, required=True)
    parser.add_argument("--bin", type=Path, required=True)
    args = parser.parse_args()
    root, out = args.source_root.resolve(), args.bin.resolve()
    assert out.is_relative_to(root / "build") and not out.exists(), "fresh contained output required"
    out.mkdir(parents=True)
    start = time.monotonic()
    spec = importlib.util.spec_from_file_location("collector", root / "scripts/collect-hosted-proof.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    local = json.loads(args.local_proof.read_text())
    matrix = json.loads((root / "diagnostics/armv4t/cases.json").read_text())
    manifest = json.loads((root / "tests/fixtures/manifest.json").read_text())
    expected = {row["name"]: row for row in local["macos"]["outputs"]}
    actual_text = args.local_log.read_text()
    identity, actual, api = module.admitted_log(actual_text,
        dict(host="darwin-arm64", group="all", run_id=0), expected, matrix, manifest)
    assert len(actual) == 722 and len(api) == 19
    print("Actual local admitted log: 722 exact guest results and 19 native API rows parsed.", flush=True)
    policy = json.loads((root / "cmake/CiJobBudgets.json").read_text())
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip()
    pin = policy["producer_head"]
    source = out / "source"
    subprocess.run(["git", "clone", "--quiet", "--no-local", str(root), str(source)], check=True)
    for path in ("CMakePresets.json", "cmake/CiPreflight.cmake", "scripts/collect-hosted-proof.py",
                 "tests/gates/final_diagnostic_proof.cmake", "tests/gates/admitted_hosted_proof.cmake",
                 ".github/workflows/ci.yml"):
        shutil.copy2(root / path, source / path)
    marker = "gamebloke.synthetic-final-proof/1\n"
    (source / "synthetic-control-authority.txt").write_text(marker)
    (out / "synthetic-control-authority.txt").write_text(marker)
    run = dict(id=2400, head_sha=head, status="completed", conclusion="success", synthetic_control=True)
    jobs = dict(synthetic_control=True, jobs=[dict(id=1000 + i, run_id=2400,
        head_sha=head, name=row["name"], status="completed", conclusion="success",
        started_at="2026-11-14T00:00:00Z", completed_at="2026-11-14T00:00:01Z")
        for i, row in enumerate(policy["jobs"])])
    local = copy.deepcopy(local)
    local["synthetic_control"] = True
    pins = {fields[0]: fields[2] for line in (root / "toolchain/arm-none-eabi.lock").read_text().splitlines()
            if (fields := line.split()) and fields[0] in ("darwin-arm64", "linux-x64")}
    texts, paths = {}, {}
    for prefix, host in (("linux", "linux-x64"), ("macos", "darwin-arm64")):
        for group in ("arm", "thumb"):
            observed = [row for row in actual if row["name"].startswith(group + "/") or
                        (group == "arm" and row["name"].startswith("smoke/"))]
            ident = dict(identity, host=host, group=group, run_id=2400,
                         job_name=f"diagnostics ({host}, {group})", toolchain_archive_sha256=pins[host])
            apis = [dict(row, group=group) for row in api] if group == "arm" else []
            records = [("identity", ident)] + [("result", row) for row in observed] + [("api", row) for row in apis]
            records += [("complete", dict(outputs=len(observed), apis=len(apis), stage="admitted"))]
            key = f"{prefix}_{group}_log"
            texts[key] = "\n".join(f"admitted-proof: {kind} {json.dumps(row, separators=(',', ':'))}" for kind, row in records) + "\n"
            paths[key] = out / (key + ".log")
            paths[key].write_text(texts[key])
    documents = {}
    for key, doc in (("run", run), ("jobs", jobs), ("local_proof", local)):
        path = out / (key + ".json")
        path.write_text(json.dumps(doc))
        documents[key] = path

    def collect(label, expected_reason=None, **updates):
        output = out / ("collected-" + label + ".json")
        options = dict(source_root=source, reviewed_head=head, synthetic_controls=True,
                       stage="admitted", output=output, **documents, **paths)
        options.update(updates)
        try:
            module.collect(argparse.Namespace(**options))
        except (ValueError, KeyError, json.JSONDecodeError) as error:
            assert expected_reason and expected_reason in str(error), (label, str(error))
            assert not output.exists()
        else:
            assert expected_reason is None, label
        print(f"Collector {label}: {'refused' if expected_reason else 'synthetic only accepted'}", flush=True)
        return output

    clean = collect("clean")
    # A synthetic target inside the disposable scope does not authorize an
    # original symlink alias outside it. Exercise every externally supplied path.
    aliases = out.parent / (out.name + "-external-aliases")
    aliases.mkdir()
    for key, target in dict(source_root=source, output=out,
                            **documents, **paths).items():
        alias = aliases / key
        alias.symlink_to(target, target_is_directory=target.is_dir())
        supplied = alias / "synthetic-alias-output.json" if key == "output" else alias
        collect("external-alias-" + key,
                "synthetic paths must stay in the marked disposable build tree",
                **{key: supplied})
    # Actual copied logs are made synthetic only inside this marked tree.
    for label, before, after, reason in (
        ("proof-stage", '"stage":"admitted"', '"stage":"proof"', "admitted stage/tool/token mismatch"),
        ("consumer", head, "0" * 40, "admitted identity binding mismatch"),
        ("producer", pin, "0" * 40, "admitted identity binding mismatch"),
        ("run", '"run_id":2400', '"run_id":2401', "admitted identity binding mismatch"),
        ("guest-status", '"status":"PASS"', '"status":"GUEST_FAIL"', "admitted semantic/fixture mismatch"),
        ("guest-progress", '"ran":true', '"ran":false', "admitted progress/result mismatch"),
        ("api-evidence", '"assertion_count":', '"changed_assertion_count":', "admitted native evidence mismatch"),
        ("api-token", '"token":"' + identity["token"] + '"', '"token":"' + "0" * 24 + '"', "admitted native identity mismatch"),
    ):
        key = "linux_arm_log"
        changed = texts[key].replace(before, after)
        assert changed != texts[key], label
        if label == "api-token":
            lines = changed.splitlines()
            lines[0] = texts[key].splitlines()[0]
            changed = "\n".join(lines) + "\n"
        paths[key].write_text(changed)
        collect(label, reason)
        paths[key].write_text(texts[key])
    key = "linux_arm_log"
    paths[key].write_text("\n".join(line for line in texts[key].splitlines() if '"fixture_id":"smoke"' not in line))
    collect("missing-guest", "incomplete admitted output set")
    paths[key].write_text(texts[key])
    collect("live-synthetic", "synthetic context cannot collect live evidence", synthetic_controls=False)

    def validate(label, doc, expected_reason=None, stage="admitted"):
        path = out / (label + "-receipt.json")
        path.write_text(json.dumps(doc))
        result = subprocess.run(["cmake", "-DCASE=final-hosted-proof", "-DSYNTHETIC_CONTROL=ON",
            f"-DSYNTHETIC_PRODUCER_HEAD={pin}", f"-DHOSTED_STAGE={stage}", f"-DSRC={source}",
            f"-DBIN={source / 'build' / ('validate-' + label)}", f"-DHOSTED_RECEIPT={path}",
            f"-DLIVE_RUN={documents['run']}", f"-DLIVE_JOBS={documents['jobs']}",
            f"-DLOCAL_RECEIPT={documents['local_proof']}", "-P", str(source / "tests/gates/toolchain_spike.cmake")],
            capture_output=True, text=True)
        report = result.stdout + result.stderr
        (out / (label + "-validation.log")).write_text(report)
        if expected_reason:
            assert result.returncode and expected_reason in report, (label, report)
        else:
            assert result.returncode == 0 and "no hosted qualification" in report, report
        print(f"CMake {label}: {'refused' if expected_reason else 'synthetic only accepted'}", flush=True)

    receipt = json.loads(clean.read_text())
    validate("clean", receipt)
    for label, mutate, reason in (
        ("proof-stage", lambda d: d["hosts"][0]["proof"].update(stage="pre-admission"), "host-source-identity-mismatch"),
        ("missing-result", lambda d: d["hosts"][0]["proof"]["executions"].pop(), "admitted-fixture-set-mismatch"),
        ("status", lambda d: d["hosts"][0]["proof"]["executions"][0].update(status="REFUSED"), "admitted-semantic-mismatch"),
        ("progress", lambda d: d["hosts"][0]["proof"]["executions"][0].update(ran=False), "admitted-progress-mismatch"),
        ("consumer", lambda d: d["hosts"][0]["proof"]["execution_identity"].update(reviewed_head="0" * 40), "admitted-identity-mismatch"),
        ("api", lambda d: d["hosts"][0]["proof"]["native_apis"].pop(), "admitted-api-set-mismatch"),
        ("tool", lambda d: (d["hosts"][0]["proof"].update(toolchain_archive_sha256="0" * 64),
                           d["hosts"][0]["proof"]["execution_identity"].update(toolchain_archive_sha256="0" * 64)), "host-tool-identity-mismatch"),
    ):
        changed = copy.deepcopy(receipt)
        mutate(changed)
        validate(label, changed, reason)
    validate("producer-only-mode", receipt, "host-source-identity-mismatch", stage="pre-admission")
    print(f"Admitted controls passed in {time.monotonic() - start:.2f}s; synthetic only, no hosted qualification.", flush=True)


if __name__ == "__main__":
    main()
