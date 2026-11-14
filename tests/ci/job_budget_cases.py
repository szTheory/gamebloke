#!/usr/bin/env python3
"""Local-only stdlib controls for the hosted collector and source preflight.

Synthetic logs/API documents are explicit parser controls, never hosted evidence.
No Python requirement is added to CI or to the emulator runtime.
"""
import argparse
import copy
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

sys.dont_write_bytecode = True


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path("."))
    parser.add_argument("--bin", type=Path, required=True)
    parser.add_argument("--local-proof", type=Path, required=True)
    args = parser.parse_args()
    root = args.source_root.resolve()
    out = args.bin.resolve()
    assert out.is_relative_to(root / "build") and not out.exists(), "select fresh contained output"
    out.mkdir(parents=True)
    started = time.monotonic()
    policy = json.loads((root / "cmake/CiJobBudgets.json").read_text())
    pin = policy["producer_head"]
    head = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    script = root / "cmake/CiPreflight.cmake"

    def preflight(source, expected=None, extra=()):
        result = subprocess.run(["cmake", f"-DSRC={source}", *extra, "-P", str(script)], text=True, capture_output=True)
        if expected:
            assert result.returncode and expected in result.stdout + result.stderr, result.stdout + result.stderr
        else:
            assert result.returncode == 0, result.stdout + result.stderr
        return result.stdout + result.stderr

    preflight(root, extra=("-DREQUIRE_AUTHORITY=ON",))
    # Small disposable clone; only its own bytes/modes/history are mutated.
    clone = out / "source"
    subprocess.run(["git", "clone", "--quiet", "--shared", str(root), str(clone)], check=True)
    for path in ("CMakeLists.txt", "cmake/CiPreflight.cmake", "cmake/CiJobBudgets.cmake", "cmake/CiJobBudgets.json", ".github/workflows/ci.yml"):
        shutil.copy2(root / path, clone / path)
    preflight(clone, extra=("-DREQUIRE_AUTHORITY=ON",))
    policy_path = clone / "cmake/CiJobBudgets.json"
    for label, change, reason in (
        ("raised-policy", lambda doc: doc["jobs"][0].update(seconds=300), "unauthorized-job-budget"),
        ("typed-policy", lambda doc: doc["jobs"][0].update(seconds="180"), "unauthorized-job-budget"),
        ("duplicate-policy", lambda doc: doc["jobs"][0].update(name="title"), "unauthorized-job-budget"),
        ("historical-policy", lambda doc: doc.update(producer_head="c3af6debd2f162c8eb91dc96906cf6f6a5ed5d16"), "policy-authority-mismatch")):
        changed = copy.deepcopy(policy)
        change(changed)
        policy_path.write_text(json.dumps(changed))
        preflight(clone, reason)
    policy_path.write_text(json.dumps(policy))
    for label, document, reason in (
        ("comment-policy", json.dumps(policy) + "\n// control\n", "json: comment"),
        ("trailing-policy", json.dumps(policy)[:-1] + ",}", "json: trailing-comma"),
        ("duplicate-key-policy", json.dumps(policy).replace('"format":', '"format":"duplicate-control","format":', 1), "json: duplicate-key")):
        policy_path.write_text(document)
        preflight(clone, reason)
    policy_path.write_text(json.dumps(policy))
    target = clone / "diagnostics/armv4t/smoke.S"
    original = target.read_bytes()
    target.write_bytes(original + b"\n")
    preflight(clone, "producer-input-digest-mismatch")
    target.write_bytes(original)
    mode = target.stat().st_mode
    target.chmod(mode ^ 0o111)
    preflight(clone, "stale-diagnostic-source-input")
    target.chmod(mode)
    preflight(clone, extra=("-DREQUIRE_AUTHORITY=ON",))
    # A historical real producer is an ancestor but its old inputs no longer match.
    authority = out / "old-authority.cmake"
    authority.write_text('cmake_minimum_required(VERSION 3.25)\n'
                         f'include("{root}/cmake/CheckDiagnosticAuthority.cmake")\n'
                         f'gbl_check_diagnostic_authority("{root}" "c3af6debd2f162c8eb91dc96906cf6f6a5ed5d16")\n')
    refused = subprocess.run(["cmake", "-P", str(authority)], capture_output=True, text=True)
    assert refused.returncode and "stale-diagnostic-source-input" in refused.stdout + refused.stderr
    # Shallow metadata-only checkout genuinely lacks the producer object. A new
    # root commit models GitHub's single-commit checkout without lying about ancestry.
    shallow_source = out / "shallow-origin"
    shutil.copytree(clone, shallow_source, ignore=shutil.ignore_patterns(".git", "build"))
    env = dict(os.environ, GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1")
    for command in (["init", "--quiet", "--template="], ["add", "--", "."],
                    ["-c", "user.name=Controls", "-c", "user.email=controls@example.com", "commit", "--quiet", "-m", "synthetic shallow metadata"]):
        subprocess.run(["git", "-C", str(shallow_source), *command], env=env, check=True)
    shallow = out / "shallow"
    subprocess.run(["git", "clone", "--quiet", "--depth=1", shallow_source.as_uri(), str(shallow)], check=True)
    assert subprocess.run(["git", "-C", str(shallow), "cat-file", "-e", pin], capture_output=True).returncode
    assert "ancestry/modes NOT checked" in preflight(shallow)
    preflight(shallow, "diagnostic-source-not-ancestor", ("-DREQUIRE_AUTHORITY=ON",))
    for lane in ("ci", "hygiene"):
        result = subprocess.run(["cmake", "-S", str(shallow), "-B", str(shallow / "build" / lane),
                                 f"-DGBL_LANE={lane}", "-DGBL_DIAGNOSTIC_STAGE=proof"], capture_output=True, text=True)
        assert result.returncode == 0 and "ancestry/modes NOT checked" in result.stdout, result.stdout + result.stderr
    result = subprocess.run(["cmake", "-S", str(shallow), "-B", str(shallow / "build/diagnostics"),
                             "-DGBL_LANE=diagnostics", "-DGBL_DIAGNOSTIC_STAGE=proof"], capture_output=True, text=True)
    assert result.returncode and "diagnostic-source-not-ancestor" in result.stderr and "compiler identification" not in result.stdout
    print("Source controls passed: real pin, changed bytes/modes, historical source, absent pin in shallow/full modes.", flush=True)

    marker = "gamebloke.synthetic-final-proof/1\n"
    (out / "synthetic-control-authority.txt").write_text(marker)
    (clone / "synthetic-control-authority.txt").write_text(marker)
    collector_source = out / "collector-source"
    subprocess.run(["git", "clone", "--quiet", "--shared", str(root), str(collector_source)], check=True)
    consumer_files = ("cmake/CiPreflight.cmake", "cmake/CiJobBudgets.cmake", "cmake/CiJobBudgets.json",
                      ".github/workflows/ci.yml", "tests/gates/final_diagnostic_proof.cmake")
    for path in consumer_files:
        shutil.copy2(root / path, collector_source / path)
        assert (root / path).read_bytes() == (collector_source / path).read_bytes(), path
    (collector_source / "synthetic-control-authority.txt").write_text(marker)

    # Tiny live-gate context probes run before parsing synthetic output sets.
    # Empty documents cannot assert real server evidence or qualify a result.
    plain = out.parent / (out.name + "-live-context")
    plain.mkdir()
    empty = plain / "empty.json"
    empty.write_text("{}\n")
    marked_input = out / "nested/empty.json"
    marked_input.parent.mkdir()
    marked_input.write_text("{}\n")
    fields = ("HOSTED_RECEIPT", "LIVE_RUN", "LIVE_JOBS", "LOCAL_RECEIPT")

    def cmake_context(label, source=root, binary=None, documents=None, expected="synthetic-evidence-forbidden"):
        documents = documents or {field: empty for field in fields}
        command = ["cmake", "-DCASE=final-hosted-proof", f"-DSRC={source}",
                   f"-DBIN={binary or plain / label}"]
        command += [f"-D{field}={path}" for field, path in documents.items()]
        command += ["-P", str(root / "tests/gates/toolchain_spike.cmake")]
        result = subprocess.run(command, capture_output=True, text=True)
        report = result.stdout + result.stderr
        assert result.returncode and expected in report, (label, report)
        if expected == "JSON member 'head_sha' not found":
            assert "synthetic-evidence-forbidden" not in report, report
        print("CMake live context control " + label + ": refused at expected boundary", flush=True)

    cmake_context("unmarked-incomplete", expected="JSON member 'head_sha' not found")
    source_marker = collector_source / "synthetic-control-authority.txt"
    source_marker.unlink()
    try:
        cmake_context("source-ancestor", source=collector_source, binary=collector_source / "build/live-check")
    finally:
        source_marker.write_text(marker)
    cmake_context("output-ancestor", binary=out / "nested/validation")
    for field in fields:
        documents = {name: empty for name in fields}
        documents[field] = marked_input
        cmake_context(field.lower() + "-ancestor", documents=documents)
    special = out.parent / (out.name + ";marked-context")
    special.mkdir()
    (special / "synthetic-control-authority.txt").write_text(marker)
    (special / "empty.json").write_text("{}\n")
    cmake_context("semicolon-input", documents={field: special / "empty.json" if field == "LIVE_RUN" else empty for field in fields})
    source_alias = out / "source-alias"
    source_alias.symlink_to(root, target_is_directory=True)
    output_alias = out / "output-alias"
    output_alias.symlink_to(plain, target_is_directory=True)
    input_alias = out / "input-alias.json"
    input_alias.symlink_to(empty)
    cmake_context("source-symlink", source=source_alias)
    cmake_context("output-symlink", binary=output_alias / "validation")
    cmake_context("input-symlink", documents={field: input_alias if field == "LIVE_RUN" else empty for field in fields})

    spec = importlib.util.spec_from_file_location("collector", root / "scripts/collect-hosted-proof.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    local = json.loads(args.local_proof.read_text())
    assert local["source_head"] == pin
    local["synthetic_control"] = True
    run = dict(synthetic_control=True, id=123, head_sha=head, status="completed", conclusion="success")
    jobs = dict(synthetic_control=True, jobs=[dict(id=i + 1, name=row["name"], run_id=123,
                head_sha=head, status="completed", conclusion="success", started_at="2026-11-13T00:00:00Z",
                completed_at=f'2026-11-13T00:{row["seconds"] // 60:02}:00Z') for i, row in enumerate(policy["jobs"])])
    paths = {}
    for host, prefix in (("linux-x64", "linux"), ("darwin-arm64", "macos")):
        for group in ("arm", "thumb"):
            rows = [row for row in local["macos"]["outputs"] if row["name"].startswith(group + "/") or group == "arm" and row["name"].startswith("smoke/")]
            tool = local["macos"]["toolchain_archive_sha256"]
            if prefix == "linux":
                tool = next(line.split()[2] for line in (root / "toolchain/arm-none-eabi.lock").read_text().splitlines() if line.startswith("linux-x64 "))
            lines = [f"final-proof: job run_id=123 name=diagnostics ({host}, {group})",
                     f"final-proof: tool host={host} source={pin} archive_sha256={tool} group={group}"]
            lines += [f'final-proof: output host={host} source={pin} name={row["name"]} case={row["case_id"]} sha256={row["sha256"]} bytes={row["bytes"]} group={group}' for row in rows]
            lines += [f'final-proof: complete host={host} source={pin} outputs={len(rows)} cases={len({r["case_id"] for r in rows})} stage=pre-admission inputs={policy["producer_input_sha256"]} group={group}']
            path = out / f"synthetic-{prefix}-{group}.log"
            path.write_text("\n".join(lines) + "\n")
            paths[f"{prefix}_{group}_log"] = path

    def collect(label, job_doc=None, local_doc=None, run_doc=None, expected=None, synthetic=True, log_paths=None, source=collector_source, reviewed=head, output=None):
        directory = out / label
        directory.mkdir()
        documents = {"run": run_doc or run, "jobs": job_doc or jobs, "local_proof": local_doc or local}
        options = dict(source_root=source, reviewed_head=reviewed, synthetic_controls=synthetic, output=output or directory / "receipt.json", **(log_paths or paths))
        for name, doc in documents.items():
            path = directory / (name + ".json")
            path.write_text(json.dumps(doc))
            options[name] = path
        try:
            module.collect(argparse.Namespace(**options))
        except (ValueError, KeyError) as error:
            assert expected and expected in str(error), (label, str(error))
            assert not options["output"].exists()
        else:
            assert expected is None, label
            receipt = json.loads(options["output"].read_text())
            assert receipt["synthetic_control"] and receipt["reviewed_head"] == reviewed and receipt["producer_head"] == pin
        print(f"Collector control {label}: {'refused' if expected else 'accepted synthetic only'}", flush=True)

    collect("clean")
    for mask in range(7):
        tagged = [copy.deepcopy(document) for document in (run, jobs, local)]
        for index, document in enumerate(tagged):
            document["synthetic_control"] = bool(mask & (1 << index))
        collect(f"mixed-tags-{mask}", run_doc=tagged[0], job_doc=tagged[1], local_doc=tagged[2],
                expected="require every input tag")
    for value in (1, "true", None):
        tagged = dict(run, synthetic_control=value)
        collect(f"typed-tag-{type(value).__name__}", run_doc=tagged, expected="boolean input tags")
    collect("unmarked-source", source=root, expected="marked disposable build tree")
    collect("unmarked-output", output=out.parent / (out.name + "-outside.json"), expected="marked disposable build tree")
    outside_logs = dict(paths, linux_arm_log=root / "scripts/collect-hosted-proof.py")
    collect("outside-log", log_paths=outside_logs, expected="inputs must stay")
    # No untagged test documents are written outside their marked scope. Probe
    # the live context boundary directly to isolate each path's ancestry.
    plain = out.parent / (out.name + "-unmarked")
    fake_inputs = {name: plain / (name + ".json") for name in ("run", "jobs", "local_proof", *paths)}
    context = argparse.Namespace(source_root=root, synthetic_controls=False,
                                output=plain / "receipt.json", **fake_inputs)
    untagged = [{"synthetic_control": False} for _ in range(3)]
    assert module.synthetic_context(context, root, untagged) is False
    marker_path = collector_source / "synthetic-control-authority.txt"
    marker_path.unlink()
    try:
        contexts = [("source-ancestor", dict(source_root=collector_source))]
        contexts += [("output-ancestor", dict(output=out / "nested/receipt.json"))]
        contexts += [("source-symlink", dict(source_root=source_alias)),
                     ("output-symlink", dict(output=output_alias / "receipt.json")),
                     ("input-symlink", dict(run=input_alias))]
        contexts += [(name + "-ancestor", {name: out / "nested/control.json"}) for name in fake_inputs]
        for label, changed in contexts:
            probe = argparse.Namespace(**(vars(context) | changed))
            try:
                module.synthetic_context(probe, probe.source_root.resolve(), untagged)
            except ValueError as error:
                assert "synthetic context cannot" in str(error), (label, str(error))
            else:
                raise AssertionError("marked ancestor accepted: " + label)
            print("Collector context control " + label + ": refused", flush=True)
    finally:
        marker_path.write_text(marker)
    # Actual descendant Git identity with unchanged pinned producer inputs.
    subprocess.run(["git", "-C", str(clone), "-c", "core.hooksPath=/dev/null",
                    "-c", "user.name=Controls", "-c", "user.email=controls@example.com",
                    "commit", "--quiet", "--allow-empty", "-m", "synthetic consumer-only descendant"], check=True)
    descendant = subprocess.check_output(["git", "-C", str(clone), "rev-parse", "HEAD"], text=True).strip()
    descendant_run = dict(run, head_sha=descendant)
    descendant_jobs = copy.deepcopy(jobs)
    for job in descendant_jobs["jobs"]:
        job["head_sha"] = descendant
    collect("consumer-descendant", job_doc=descendant_jobs, run_doc=descendant_run, source=clone, reviewed=descendant)
    for index, row in enumerate(policy["jobs"]):
        changed = copy.deepcopy(jobs)
        changed["jobs"][index]["completed_at"] = f'2026-11-13T00:{row["seconds"] // 60:02}:01Z'
        collect(f"over-budget-{index}", job_doc=changed, expected="exceeds exact budget")
    for label, field, value, reason in (
        ("unknown", "name", "unknown", "job set mismatch"),
        ("negative-time", "completed_at", "2026-11-12T23:59:59Z", "exceeds exact budget"),
        ("invalid-time", "completed_at", "2026-02-30T00:00:00Z", "day"),
        ("wrong-run", "run_id", 999, "source/run/status"),
        ("wrong-head", "head_sha", "0" * 40, "source/run/status"),
        ("red", "conclusion", "failure", "source/run/status")):
        changed = copy.deepcopy(jobs)
        changed["jobs"][0][field] = value
        collect(label, job_doc=changed, expected=reason)
    changed = copy.deepcopy(jobs)
    changed["jobs"].pop()
    collect("missing-job", job_doc=changed, expected="job set mismatch")
    changed = copy.deepcopy(jobs)
    changed["jobs"][1]["id"] = changed["jobs"][0]["id"]
    collect("duplicate-job", job_doc=changed, expected="duplicate live job IDs")
    changed = copy.deepcopy(local)
    changed["source_head"] = "c3af6debd2f162c8eb91dc96906cf6f6a5ed5d16"
    collect("old-local-source", local_doc=changed, expected="local proof source")
    changed = copy.deepcopy(local)
    changed["macos"]["source_input_sha256"] = "0" * 64
    collect("old-local-digest", local_doc=changed, expected="source/input/tool authority")
    swapped = dict(paths)
    swapped["linux_arm_log"], swapped["linux_thumb_log"] = swapped["linux_thumb_log"], swapped["linux_arm_log"]
    collect("swapped-groups", log_paths=swapped, expected="log live run/job context mismatch")
    collect("synthetic-live-refusal", synthetic=False, expected="synthetic controls require explicit")
    (out / "RESULTS.json").write_text(json.dumps(dict(synthetic_control=True, passed=True, elapsed_seconds=time.monotonic() - started), indent=2) + "\n")
    print("Local source/collector controls passed; no hosted qualification.")


if __name__ == "__main__":
    main()
