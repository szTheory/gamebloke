#!/usr/bin/env python3
"""Read four actual job logs and current raw GitHub run/jobs API documents.

No network, publication or ROM creation. The local proof is expected authority,
never a substitute for observed hosted output. Run the committed CMake validator
after collection. Keep each failed attempt's raw documents and logs unchanged.
"""
import argparse
import datetime as dt
import hashlib
import json
import os
import pathlib
import re
import subprocess


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def admitted_log(text, context, expected, matrix, manifest):
    """Parse actual mandatory-final witnesses; also used by local-only controls."""
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, "duplicate admitted JSON key")
            result[key] = value
        return result

    identities, completions, outputs, apis = [], [], {}, {}
    for line in text.splitlines():
        match = re.search(r"admitted-proof: (identity|result|api|complete) (\{.*\})\s*$", line)
        if not match:
            continue
        kind, raw = match.groups()
        row = json.loads(raw, object_pairs_hook=unique)
        if kind == "identity":
            identities.append(row)
        elif kind == "complete":
            completions.append(row)
        else:
            key = row["name"] if kind == "result" else str(row["evidence"]["id"])
            target = outputs if kind == "result" else apis
            require(key not in target or target[key] == row, "conflicting admitted witness")
            target[key] = row
    require(identities and all(row == identities[0] for row in identities), "missing/conflicting admitted identity")
    identity = identities[0]
    require(all(identity.get(key) == value for key, value in context.items()), "admitted identity binding mismatch")
    require(identity.get("format") == "gamebloke.admitted-host-log/1" and identity.get("stage") == "admitted" and
            identity.get("tools") == "available" and re.fullmatch(r"[0-9a-f]{24}", identity.get("token", "")),
            "admitted stage/tool/token mismatch")
    for key in ("manifest_sha256", "runner_sha256", "native_sha256"):
        require(re.fullmatch(r"[0-9a-f]{64}", identity.get(key, "")), "admitted identity digest invalid")
    require(set(outputs) == set(expected), "incomplete admitted output set")
    cases = {row["id"]: row for row in matrix["cases"]}
    fixtures = {row["id"]: row for row in manifest["fixtures"]}
    for name, row in outputs.items():
        fid = pathlib.PurePosixPath(name).stem
        source = expected[name]
        case = source["case_id"]
        stop = cases.get(case, {}).get("source_test", {}).get("kind") == "guest-stop"
        state = "pending" if stop else "fail" if fid.endswith("-broken") else "pass"
        status = "GUEST_STOP" if stop else "GUEST_FAIL" if state == "fail" else "PASS"
        reason = cases[case]["source_test"]["expected_reason"] if stop else "guest-mailbox-" + state
        termination = reason if stop else "mailbox-complete"
        require(all(row.get(key) == value for key, value in source.items()), "admitted output differs from producer proof")
        require(fid in fixtures and fixtures[fid]["build"]["artifact"]["sha256"] == row["sha256"] and
                row.get("fixture_id") == fid and row.get("state") == state and row.get("status") == status and
                row.get("reason") == reason and row.get("termination") == termination,
                "admitted semantic/fixture mismatch")
        require(row.get("ran") is True and all(type(row.get(key)) is int and row[key] > 0 for key in ("cycles", "instructions")) and
                row.get("budget_cycles") == 1000 and row["cycles"] <= 1000 and
                re.fullmatch(r"[0-9a-f]{64}", row.get("result_sha256", "")), "admitted progress/result mismatch")
    api_expected = {str(row["id"]): row for row in matrix["cases"] if row.get("source_test", {}).get("kind") == "cpu-api"}
    if context.get("group") == "thumb":
        api_expected = {}
    require(set(apis) == set(api_expected), "incomplete admitted native API set")
    for key, row in apis.items():
        require(row.get("evidence") == api_expected[key], "admitted native evidence mismatch")
        require(all(row.get(field) == identity.get(field) for field in (
            "token", "stage", "group", "reviewed_head", "source_commit", "source_input_sha256",
            "manifest_sha256", "runner_sha256", "native_sha256")), "admitted native identity mismatch")
    complete = {"outputs": len(outputs), "apis": len(apis), "stage": "admitted"}
    require(completions and all(row == complete for row in completions), "missing/conflicting admitted completion")
    return identity, [outputs[name] for name in sorted(outputs)], [apis[key] for key in sorted(apis)]


def synthetic_context(args, root, documents=None):
    tags = [document.get("synthetic_control", False) for document in documents or ()]
    require(all(type(tag) is bool for tag in tags), "synthetic controls require boolean input tags")
    marker_name = "synthetic-control-authority.txt"
    marker_bytes = b"gamebloke.synthetic-final-proof/1\n"
    output = args.output.resolve()
    inputs = [args.run, args.jobs, args.local_proof,
              args.linux_arm_log, args.linux_thumb_log, args.macos_arm_log, args.macos_thumb_log]
    if not args.synthetic_controls:
        require(not any(tags), "synthetic controls require explicit isolated mode; never treat them as hosted evidence")
        def marked_ancestor(path):
            for candidate in (path.absolute(), path.resolve()):
                for parent in (candidate, *candidate.parents):
                    marker = parent / marker_name
                    if marker.exists() or marker.is_symlink():
                        return True
            return False
        require(not any(marked_ancestor(path) for path in [root, args.source_root, args.output, *inputs]),
                "synthetic context cannot collect live evidence")
        return False
    require(all(tags), "synthetic controls require every input tag")

    def marked(path):
        marker = path / marker_name
        return marker.is_file() and not marker.is_symlink() and marker.read_bytes() == marker_bytes

    scope = next((parent for parent in output.parents if marked(parent)), None)
    require(scope is not None and "build" in scope.parts and root.is_relative_to(scope) and marked(root),
            "synthetic controls require a marked disposable build tree")
    require(all(pathlib.Path(os.path.abspath(path)).is_relative_to(scope) and
                path.resolve().is_relative_to(scope)
                for path in [args.source_root, args.output, *inputs]),
            "synthetic paths must stay in the marked disposable build tree")
    return True


def collect(args):
    root = args.source_root.resolve()
    # Reject path aliases before opening API documents; validate their tags
    # separately once those contained documents have been parsed.
    synthetic_context(args, root)
    run = json.loads(args.run.read_text())
    jobs_document = json.loads(args.jobs.read_text())
    local = json.loads(args.local_proof.read_text())
    synthetic = synthetic_context(args, root, (run, jobs_document, local))
    # Local-only stdlib helper. CI/runtime do not gain a Python dependency.
    # Run the shared cheap checks before parsing large local/output documents.
    subprocess.run(["cmake", f"-DSRC={root}", "-DREQUIRE_AUTHORITY=ON",
                    "-P", str(root / "cmake/CiPreflight.cmake")], check=True)
    policy = json.loads((root / "cmake/CiJobBudgets.json").read_text())
    source_head = policy["producer_head"]
    require(re.fullmatch(r"[0-9a-f]{40}", args.reviewed_head), "invalid reviewed head")
    actual_head = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    require(actual_head == args.reviewed_head, "reviewed head is not current checkout")
    budgets = {row["name"]: row["seconds"] for row in policy["jobs"]}
    jobs = jobs_document["jobs"]
    matrix = json.loads((root / "diagnostics/armv4t/cases.json").read_text())
    names = {"arm": {"smoke/smoke.gba": 20481, "smoke/smoke-broken.gba": 20481}, "thumb": {}}
    for row in matrix["cases"]:
        kind = row.get("source_test", {}).get("kind")
        if kind not in ("semantic", "guest-stop"):
            continue
        isa, case = row["isa"], row["id"]
        if kind == "semantic":
            names[isa].update({f"{isa}/{isa}-{case}.gba": case, f"{isa}/{isa}-{case}-broken.gba": case})
        else:
            names[isa][f"{isa}/{isa}-stop-{case}.gba"] = case
    require(not names["arm"].keys() & names["thumb"].keys(), "overlapping source groups")
    expected_names = names["arm"] | names["thumb"]
    require((len(names["arm"]), len(names["thumb"]), len(expected_names)) == (478, 244, 722), "unexpected source output inventory")
    require(len(set(expected_names.values())) == 367, "unexpected source case inventory")
    require(run["head_sha"] == args.reviewed_head and run["status"] == "completed" and run["conclusion"] == "success", "run is not current green")
    require(local["source_head"] == source_head and local["format"] == "gamebloke.final-local-proof/1" and local["literal_cmp_count"] == 722, "local proof source/comparison mismatch")
    required = set(budgets)
    require(len(jobs) == len(required) and {j["name"] for j in jobs} == required, "required job set mismatch")
    require(len({j["id"] for j in jobs}) == len(jobs), "duplicate live job IDs")
    for job in jobs:
        require(job["run_id"] == run["id"] and job["head_sha"] == args.reviewed_head and job["status"] == "completed" and job["conclusion"] == "success", "required job source/run/status mismatch")
        times = [dt.datetime.strptime(job[k], "%Y-%m-%dT%H:%M:%SZ") for k in ("started_at", "completed_at")]
        require(all(time.strftime("%Y-%m-%dT%H:%M:%SZ") == job[key] for time, key in zip(times, ("started_at", "completed_at"))), "invalid job time")
        require(0 <= (times[1] - times[0]).total_seconds() <= budgets[job["name"]], "required job exceeds exact budget")
    pins = {fields[0]: fields[2] for line in (root / "toolchain/arm-none-eabi.lock").read_text().splitlines() if (fields := line.split()) and fields[0] in ("linux-x64", "darwin-arm64", "linux-arm64")}
    material = ""
    for line in (root / "diagnostics/armv4t/inputs.txt").read_text().splitlines():
        if line and not line.startswith("#"):
            path = root / line
            require(path.is_file() and not path.is_symlink(), "producer input is not regular")
            material += f"{line}={hashlib.sha256(path.read_bytes()).hexdigest()}\n"
    inputs = hashlib.sha256(material.encode()).hexdigest()
    require(inputs == policy["producer_input_sha256"], "producer input digest mismatch")
    expected = {}
    for key, host in (("macos", "darwin-arm64"), ("linux_arm64", "linux-arm64")):
        proof = local[key]
        require(proof["source_head"] == source_head and proof["host"] == host and proof["source_input_sha256"] == inputs and proof["toolchain_archive_sha256"] == pins[host], "local source/input/tool authority mismatch")
        outputs = proof["outputs"]
        require(len(outputs) == len(expected_names) and {r["name"] for r in outputs} == expected_names.keys(), "local output set mismatch")
        require(sorted(proof["output_names"]) == sorted(expected_names) and sorted(proof["case_ids"]) == sorted(set(expected_names.values())), "local declared name/case set mismatch")
        rows = {r["name"]: r for r in outputs}
        require(all(r["case_id"] == expected_names[n] for n, r in rows.items()), "local case/name mismatch")
        if expected:
            require(rows == expected, "local cross-host rows differ")
        expected = rows
    stage = getattr(args, "stage", "admitted")
    require(stage in ("admitted", "pre-admission"), "invalid hosted stage")
    manifest = json.loads((root / "tests/fixtures/manifest.json").read_text())
    manifest_sha = hashlib.sha256((root / "tests/fixtures/manifest.json").read_bytes()).hexdigest()
    patterns = {
        "output": re.compile(r"final-proof: output host=(\S+) source=([0-9a-f]{40}) name=(\S+) case=([0-9]+) sha256=([0-9a-f]{64}) bytes=([0-9]+) group=(\S+)"),
        "tool": re.compile(r"final-proof: tool host=(\S+) source=([0-9a-f]{40}) archive_sha256=([0-9a-f]{64}) group=(\S+)"),
        "complete": re.compile(r"final-proof: complete host=(\S+) source=([0-9a-f]{40}) outputs=([0-9]+) cases=([0-9]+) stage=pre-admission inputs=([0-9a-f]{64}) group=(\S+)"),
        "job": re.compile(r"final-proof: job run_id=([0-9]+) name=diagnostics \(([^,]+), ([^)]+)\)"),
    }
    parts, log_hashes = [], set()
    for prefix, host in (("linux", "linux-x64"), ("macos", "darwin-arm64")):
        for group in names:
            path = getattr(args, f"{prefix}_{group}_log")
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            require(digest not in log_hashes, "one log supplied for multiple groups")
            log_hashes.add(digest)
            if stage == "admitted":
                context = dict(host=host, group=group, source_commit=source_head,
                               source_input_sha256=inputs, reviewed_head=args.reviewed_head,
                               run_id=run["id"], job_name=f"diagnostics ({host}, {group})",
                               toolchain_archive_sha256=pins[host], manifest_sha256=manifest_sha)
                identity, executions, api_rows = admitted_log(path.read_text(), context,
                    {name: expected[name] for name in names[group]}, matrix, manifest)
                job_name = context["job_name"]
                job = next(j for j in jobs if j["name"] == job_name)
                outputs = [{key: row[key] for key in ("name", "case_id", "sha256", "bytes")} for row in executions]
                proof = dict(format="gamebloke.final-diagnostic-host/1", stage="admitted", host=host,
                    group=group, source_head=source_head, source_input_sha256=inputs,
                    toolchain_archive_sha256=pins[host], run_id=run["id"], job_name=job_name,
                    case_ids=sorted(set(names[group].values())), output_names=sorted(names[group]),
                    outputs=outputs, execution_identity=identity, executions=executions, native_apis=api_rows)
                parts.append(dict(host=host, group=group, job_id=job["id"], log_sha256=digest, proof=proof))
                continue
            outputs, tools, completions, bindings = {}, set(), set(), set()
            for line in path.read_text().splitlines():
                if m := patterns["output"].search(line):
                    h, source, name, case, sha, size, g = m.groups()
                    require((h, source, g) == (host, source_head, group), "output source/host/group mismatch")
                    row = {"name": name, "case_id": int(case), "sha256": sha, "bytes": int(size)}
                    require(name in names[group] and row == expected[name], "observed hosted output differs from exact expected local bytes/case/subset")
                    require(name not in outputs or outputs[name] == row, "conflicting repeated output")
                    outputs[name] = row
                if m := patterns["tool"].search(line):
                    h, source, sha, g = m.groups()
                    require((h, source, sha, g) == (host, source_head, pins[host], group), "tool source/host/pin/group mismatch")
                    tools.add(sha)
                if m := patterns["complete"].search(line):
                    h, source, count, cases, sha, g = m.groups()
                    require((h, source, sha, g) == (host, source_head, inputs, group), "complete source/host/input/group mismatch")
                    completions.add((int(count), int(cases)))
                if m := patterns["job"].search(line):
                    rid, h, g = m.groups()
                    require((int(rid), h, g) == (run["id"], host, group), "log live run/job context mismatch")
                    bindings.add((int(rid), h, g))
            require(set(outputs) == set(names[group]) and len(tools) == len(completions) == len(bindings) == 1, "incomplete job log proof")
            ids = sorted(set(names[group].values()))
            require(completions == {(len(outputs), len(ids))}, "incomplete count witness")
            job_name = f"diagnostics ({host}, {group})"
            job = next(j for j in jobs if j["name"] == job_name)
            proof = {"format": "gamebloke.final-diagnostic-host/1", "stage": "pre-admission", "host": host, "group": group, "source_head": source_head, "source_input_sha256": inputs, "toolchain_archive_sha256": pins[host], "run_id": run["id"], "job_name": job_name, "case_ids": ids, "output_names": sorted(outputs), "outputs": [outputs[n] for n in sorted(outputs)]}
            parts.append({"host": host, "group": group, "job_id": job["id"], "log_sha256": digest, "proof": proof})
    require(not args.output.exists(), "preserve existing receipt; select fresh output")
    result = {"reviewed_head": args.reviewed_head, "producer_head": source_head, "run_id": run["id"], "hosts": parts}
    if synthetic:
        result["synthetic_control"] = True
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print("Synthetic parser control only; no hosted evidence." if synthetic else "Collected four current grouped log witnesses; run committed final-hosted-proof. No fixture admission.")


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--reviewed-head", required=True, help="actual tested run HEAD; immutable producer pin comes from policy")
    p.add_argument("--source-root", type=pathlib.Path, default=pathlib.Path("."))
    p.add_argument("--synthetic-controls", action="store_true")
    p.add_argument("--stage", choices=("admitted", "pre-admission"), default="admitted",
                   help="final execution is admitted; pre-admission is producer qualification only")
    for option in ("run", "jobs", "local-proof", "linux-arm-log", "linux-thumb-log", "macos-arm-log", "macos-thumb-log", "output"):
        p.add_argument("--" + option, type=pathlib.Path, required=True)
    return p


if __name__ == "__main__":
    collect(parser().parse_args())
