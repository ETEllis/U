"""Independent release verification of native U, not a language implementation."""
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/native"
ARTIFACTS = ROOT / "artifacts"
LOG = []
RECORDS = []


def run(name, command, *, env=None, timeout=600):
    began = time.monotonic()
    result = subprocess.run(list(map(str, command)), cwd=ROOT, env=env, capture_output=True, text=True, timeout=timeout)
    output = result.stdout + result.stderr
    LOG.append(f"=== {name} ===\n{output}")
    counts = re.findall(r"Ran (\d+) tests?", output)
    record = {"name": name, "command": list(map(str, command)), "exit_code": result.returncode,
              "seconds": round(time.monotonic() - began, 3), "passed": result.returncode == 0}
    if counts: record["test_groups"] = int(counts[-1])
    if re.search(r"skipped[= ]", output, re.I): raise RuntimeError("verification must not silently skip tests: " + name)
    RECORDS.append(record)
    print(name + ": " + ("passed" if result.returncode == 0 else "FAILED"), flush=True)
    if result.returncode:
        print(output)
        raise RuntimeError(name)
    return output


def compile_fixture(folder, name, modules, entry="main"):
    source = folder / (name + ".u")
    generated = folder / (name + ".c")
    executable = folder / name
    env = {**os.environ, "U_NATIVE_ALLOW": "read,write,console,env"}
    env.pop("U_NATIVE_STEPS", None)
    run(name + " source linkage", [BUILD / "u-link", source, *[ROOT / p for p in modules]], env=env)
    run(name + " U compilation", [BUILD / "uc", "emit", source, generated, entry], env=env)
    libs = ["-lm"] + ([] if platform.system() == "Darwin" else ["-lcrypto"])
    run(name + " machine linkage", ["cc", "-std=c11", "-O1", "-ffp-contract=off", "-I", ROOT / "native",
                                    generated, ROOT / "native/runtime.c", *libs, "-o", executable])
    return executable


def main():
    ARTIFACTS.mkdir(exist_ok=True)
    began = datetime.now(timezone.utc).isoformat()
    preserved = json.loads((ROOT / "provenance/PRESERVATION.json").read_text())
    hashes = {}
    for group, prefix in (("files", "provenance/source"), ("examples", "examples/original")):
        for entry in preserved[group]:
            path = ROOT / prefix / entry["name"]
            actual = hashlib.sha256(path.read_bytes()).hexdigest()
            if actual != entry["sha256"]: raise RuntimeError("preserved source changed: " + str(path))
            hashes[str(path.relative_to(ROOT))] = actual
    if (ROOT / "u/evaluator.py").exists(): raise RuntimeError("historical evaluator remains in production tree")
    if not (BUILD / "uc").is_file(): raise RuntimeError("run bootstrap/native-build.sh first")
    success = False
    try:
        run("native conformance", ["sh", "scripts/verify-native.sh"])
        for name, path in (("platform bridge", "tests/native_bootstrap"), ("self compiler", "tests/selfhost")):
            run(name, [sys.executable, "-m", "unittest", "discover", "-s", path, "-v"])
        run("public native commands", [sys.executable, "-m", "unittest", "discover", "-s", "tests", "-p", "test_native_cli.py", "-v"])
        run("WASM actual execution and refusals", ["node", "tests/native_programs/wasm_verify.mjs", "--compiler", BUILD / "uc"])
        run("independent CDC C oracle", [sys.executable, "scripts/verify_cdc.py", "--cases", "96"])
        with tempfile.TemporaryDirectory(prefix="u-release-fixtures-") as temporary:
            # macOS exposes its temporary root through /var -> /private/var.
            # Package tests use canonical paths because symlink traversal is refused.
            folder = Path(temporary).resolve()
            core = ["stdlib/core.u"]
            cdc = core + ["stdlib/numeric.u", "cdc/primitives.u", "cdc/analysis.u"]
            source_test = compile_fixture(folder, "cdc-source", cdc + ["cdc/source.u", "tests/native_programs/cdc_source.u"], "cdc_source_test_main")
            result = json.loads(run("CDC source conformance", [source_test], env={"PATH": "/nonexistent"}))
            if result["status"] != "passed": raise RuntimeError("CDC source fixture")
            package = compile_fixture(folder, "package", core + ["stdlib/evidence.u", "tools/package.u", "tests/native_programs/package_tests.u"], "package_tests")
            package_root = folder / "package-fixture"; package_root.mkdir()
            package_env = {"PATH": "/nonexistent", "U_NATIVE_ALLOW": "read,write"}
            run("native package lifecycle", [package, json.dumps([str(package_root)])], env=package_env)
            bad_package = compile_fixture(folder, "package-negative", core + ["stdlib/evidence.u", "tools/package.u", "tests/native_programs/package_tests.u"], "package_negative")
            rejected = []
            for mode in ["changed", "added", "missing", "legacy", "name", "duplicate", "hash", "traversal", "policy", "nested-store", "source-store"]:
                area = folder / ("package-" + mode); area.mkdir()
                result = subprocess.run([str(bad_package), json.dumps([mode, str(area)])], env=package_env, capture_output=True, text=True, timeout=30)
                if result.returncode != 70: raise RuntimeError("package refusal failed: " + mode + result.stdout + result.stderr)
                rejected.append(mode)
            RECORDS.append({"name": "native package adversarial cases", "passed": True, "cases": rejected})
            hmc = compile_fixture(folder, "hmc", core + ["stdlib/numeric.u", "stdlib/probability.u", "examples/original/11_bayes.u", "tests/native_programs/probability_full.u"])
            posterior = json.loads(run("original four-chain HMC workload", [hmc, "[81]"], env={"PATH": "/nonexistent", "U_NATIVE_STEPS": "1000000000"}, timeout=180))
            if len(posterior["chains"]) != 4 or any(len(chain) != 1000 for chain in posterior["chains"]): raise RuntimeError("HMC workload incomplete")
            RECORDS[-1].update(chains=4, retained_draws_each=1000, warmup_each=500, convergence_claim=False)
        success = True
    finally:
        sources = {}
        for directory in ["compiler", "stdlib", "cdc", "tools", "native", "bootstrap"]:
            for path in sorted((ROOT / directory).rglob("*")):
                if path.is_file() and path.suffix in {".u", ".c", ".h", ".sh", ".py"}:
                    sources[str(path.relative_to(ROOT))] = hashlib.sha256(path.read_bytes()).hexdigest()
        executable_hashes = {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                             for path in BUILD.glob("etellis-u-*") if path.is_file()}
        report = {"schema": "etellis.u.native-verification/2", "started": began,
                  "finished": datetime.now(timezone.utc).isoformat(), "passed": success,
                  "host": platform.platform(), "verification_driver": "independent development harness",
                  "production_python_required": False, "historical_reference_commit": "5dce6e72e93a165913f38713626b8c3e5c8a8f1b",
                  "checks": RECORDS, "preserved_bytes": hashes, "implementation_sources": sources,
                  "native_tools": executable_hashes,
                  "limits": ["scoped core and runtime contracts, not full dependent/resource theory",
                             "finite CDC reductions, not full U1/U2 compatibility",
                             "simulation is not physical execution", "identity is not authenticity",
                             "no general tracing garbage collector or foreign-language frontends"]}
        (ARTIFACTS / "native-verification.json").write_text(json.dumps(report, indent=2) + "\n")
        (ARTIFACTS / "native-tests.log").write_text("\n".join(LOG))
    print("Native U release verification passed.")


if __name__ == "__main__":
    main()
