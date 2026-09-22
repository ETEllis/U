/** Execute U-emitted WebAssembly and compare its results with independent
 * BigInt arithmetic and the compiled U native path. No Python participates. */
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import assert from "node:assert/strict";
import { createHash } from "node:crypto";

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
const options = new Map();
for (let i = 2; i < process.argv.length; i += 2) {
  assert.ok(["--compiler", "--emitter", "--landings"].includes(process.argv[i]), "unknown test option");
  assert.ok(process.argv[i + 1], "test option requires a path");
  options.set(process.argv[i], path.resolve(process.argv[i + 1]));
}
if (options.has("--landings")) {
  function siteModel(directory, entry) {
    const binary = fs.readFileSync(path.join(directory, "model.wasm"));
    const receipt = JSON.parse(fs.readFileSync(path.join(directory, "model.wasm.json"), "utf8"));
    assert.equal(receipt.contract.entry, entry);
    assert.equal(receipt.binary_sha256, createHash("sha256").update(binary).digest("hex"));
    assert.ok(WebAssembly.validate(binary));
    return new WebAssembly.Instance(new WebAssembly.Module(binary)).exports[entry];
  }
  const decide = siteModel(path.join(root, "landing"), "decide");
  const prefix = siteModel(options.get("--landings"), "prefix_admissible");
  let uCases = 0, bidiCases = 0, bidiAccepted = 0;
  for (let scenario = 0n; scenario <= 3n; scenario++) for (let sharing = 0n; sharing <= 1n; sharing++)
    for (let profile = 0n; profile <= 1n; profile++) {
      const expected = scenario === 3n ? 0n : scenario === 2n ? 1n :
        scenario === 1n && sharing === 0n ? 2n : profile === 0n ? 3n : 4n;
      assert.equal(decide(scenario, sharing, profile), expected);
      uCases++;
    }
  for (let a = -1n; a <= 1n; a++) for (let b = -1n; b <= 1n; b++)
    for (let c = -1n; c <= 1n; c++) for (let d = -1n; d <= 1n; d++) {
      let sum = 0n;
      const expected = [a, b, c, d].every(trit => (sum += trit) >= 0n);
      assert.equal(prefix(a, b, c, d), Number(expected));
      bidiAccepted += Number(expected);
      bidiCases++;
    }
  assert.equal(prefix(1n, 1n, 1n, 1n), 1, "positive final sum must remain admissible");
  assert.throws(() => prefix(2n, 0n, 0n, 0n), WebAssembly.RuntimeError);
  assert.throws(() => decide(4n, 0n, 0n), WebAssembly.RuntimeError);
  console.log(JSON.stringify({ status: "passed", uCases, bidiCases, bidiAccepted, binaryHashChecks: 2 }));
  process.exit(0);
}
const compiler = options.get("--compiler") ??
  ["build/native/uc", "work/selfhost/uc"].map(p => path.join(root, p)).find(p => fs.existsSync(p));
assert.ok(compiler, "build the native U compiler or pass --compiler /absolute/path/uc");
fs.mkdirSync(path.join(root, "work"), { recursive: true });
const output = fs.mkdtempSync(path.join(root, "work/wasm-verification-"));
const fixtures = path.join(root, "tests/native_programs");
const trusted = { ...process.env, U_NATIVE_ALLOW: "read,write,exec,console,env" };
delete trusted.U_NATIVE_STEPS;
const libraries = process.platform === "darwin" ? ["-lm"] : ["-lm", "-lcrypto"];
function run(executable, args, env = trusted) {
  return spawnSync(executable, args, { cwd: root, env, encoding: "utf8", timeout: 120_000, maxBuffer: 16 * 1024 * 1024 });
}
function requireSuccess(result) {
  assert.equal(result.status, 0, result.error?.message ?? result.stderr);
  return result;
}
function compileC(input, destination) {
  requireSuccess(run(process.env.CC ?? "cc", ["-std=c11", "-O1", "-ffp-contract=off", "-I", path.join(root, "native"),
    input, path.join(root, "native/runtime.c"), ...libraries, "-o", destination]));
}
let emitter = options.get("--emitter");
if (!emitter) {
  const bundle = ["compiler/compiler.u", "compiler/wasm.u", "tests/native_programs/wasm_driver.u"]
    .map(p => fs.readFileSync(path.join(root, p), "utf8")).join("\n");
  const source = path.join(output, "wasm-compiler.u");
  const c = path.join(output, "wasm-compiler.c");
  fs.writeFileSync(source, bundle);
  requireSuccess(run(compiler, ["emit", source, c, "wasm_main"]));
  emitter = path.join(output, "wasm-compiler");
  compileC(c, emitter);
}
const nativeSource = path.join(fixtures, "wasm_arithmetic.u");
const nativeC = path.join(output, "native-arithmetic.c");
const nativeArithmetic = path.join(output, "native-arithmetic");
requireSuccess(run(compiler, ["emit", nativeSource, nativeC, "calculate"]));
compileC(nativeC, nativeArithmetic);

let index = 0, executions = 0, nativeComparisons = 0, refused = 0;
const boundsText = bounds => "[" + bounds.map(([a, b]) => `[${a},${b}]`).join(",") + "]";
function generate(file, entry, bounds, expectedError = null) {
  const destination = path.join(output, `${index++}-${entry}.wasm`);
  const result = run(emitter, [path.join(fixtures, file), entry, boundsText(bounds), destination]);
  if (expectedError) {
    assert.equal(result.status, 70, result.stdout + result.stderr);
    assert.match(result.stderr, new RegExp(expectedError));
    assert.equal(fs.existsSync(destination), false, "refused lowering created an artifact");
    refused++;
    return;
  }
  requireSuccess(result);
  const receipt = JSON.parse(result.stdout);
  const bytes = fs.readFileSync(destination);
  assert.equal(WebAssembly.validate(bytes), true, entry);
  assert.equal(receipt.byte_length, bytes.length);
  assert.equal(receipt.binary_sha256, createHash("sha256").update(bytes).digest("hex"));
  const module = new WebAssembly.Module(bytes);
  const instance = new WebAssembly.Instance(module);
  return { destination, receipt, fn: instance.exports[entry], bytes };
}
function checked(artifact, ...args) {
  assert.equal(args.length, artifact.receipt.abi_bounds.length);
  args.forEach((n, i) => {
    const bound = artifact.receipt.abi_bounds[i];
    if (typeof n !== "bigint" || n < BigInt(bound.low) || n > BigInt(bound.high)) {
      throw new RangeError("outside the exact input contract before i64 conversion");
    }
  });
  return artifact.fn(...args);
}
const arithmetic = generate("wasm_arithmetic.u", "calculate", [[-10, 10], [-5, 5]]);
for (let x = -10n; x <= 10n; x++) for (let y = -5n; y <= 5n; y++) {
  const expected = (x + 3n) * y - 65n;
  assert.equal(checked(arithmetic, x, y), expected);
  executions++;
  if (x % 5n === 0n && y % 5n === 0n) {
    const result = requireSuccess(run(nativeArithmetic, [`[${x},${y}]`], { ...process.env, U_NATIVE_ALLOW: "" }));
    assert.equal(BigInt(result.stdout.trim()), expected);
    nativeComparisons++;
  }
}
for (const [x, y] of [[-11n, 0n], [11n, 0n], [0n, -6n], [0n, 6n]]) {
  assert.throws(() => arithmetic.fn(x, y), WebAssembly.RuntimeError);
  refused++;
}
const predicate = generate("wasm_admissibility.u", "admissible", [[-1, 1], [-1, 1], [-1, 1], [-1, 1]]);
let admitted = 0;
for (let a = -1n; a <= 1n; a++) for (let b = -1n; b <= 1n; b++)
  for (let c = -1n; c <= 1n; c++) for (let d = -1n; d <= 1n; d++) {
    const expected = a + b + c + d === 0n && (a !== 0n || b !== 0n || c !== 0n || d !== 0n);
    assert.equal(checked(predicate, a, b, c, d), Number(expected));
    admitted += Number(expected);
    executions++;
  }
const low = -(2n ** 63n), high = 2n ** 63n - 1n;
const identity = generate("wasm_bounds.u", "identity", [[low, high]]);
for (const n of [low, low + 1n, -8193n, -8192n, -129n, -128n, -65n, -64n, -1n, 0n, 63n, 64n, 127n, 128n, 8192n, high]) {
  assert.equal(checked(identity, n), n);
  executions++;
}
for (const [entry, expected] of [["minimum", low], ["maximum", high], ["negative", -65n], ["yes", 1]]) {
  assert.equal(generate("wasm_bounds.u", entry, []).fn(), expected);
  executions++;
}
const choose = generate("wasm_bounds.u", "choose", [[-100, 100]]);
for (const n of [-100n, -1n, 0n, 1n, 100n]) {
  assert.equal(checked(choose, n), n < 0n ? -n : n);
  executions++;
}
const comparisons = generate("wasm_bounds.u", "comparisons", [[-3, 3], [-3, 3]]);
for (let x = -3n; x <= 3n; x++) for (let y = -3n; y <= 3n; y++) {
  assert.equal(checked(comparisons, x, y), Number(x <= y));
  executions++;
}
assert.throws(() => checked(identity, 2n ** 64n + 1n), RangeError);
refused++;
// The raw JS/Wasm ABI wraps before the function sees its input. This deliberately
// demonstrates why the checked adapter is part of the public input contract.
assert.equal(identity.fn(2n ** 64n + 1n), 1n);
for (const [entry, bounds, error] of [
  ["overflowing", [[0n, high]], "WASM_INTERMEDIATE"],
  ["intermediate", [[high, high]], "WASM_INTERMEDIATE"],
  ["negative_nat", [], "WASM_NAT"], ["natural_domain", [], "WASM_NAT"],
  ["mixed", [[0, 1]], "WASM_TYPE"], ["identity", [[1, 0]], "WASM_BOUNDS"],
  ["identity", [[low - 1n, high]], "WASM_BOUNDS"], ["identity", [[0.5, 1]], "WASM_BOUNDS"],
]) generate("wasm_bounds.u", entry, bounds, error);
generate("wasm_shadowed.u", "bad", [[0, 1]], "WASM_NAMESPACE");
generate("wasm_bad_profile.u", "bad", [[0, 1]], "WASM_PROFILE");
const before = fs.readFileSync(arithmetic.destination);
const duplicate = run(emitter, [nativeSource, "calculate", "[[-10,10],[-5,5]]", arithmetic.destination]);
assert.equal(duplicate.status, 70);
assert.deepEqual(fs.readFileSync(arithmetic.destination), before);
refused++;
const repeat = generate("wasm_arithmetic.u", "calculate", [[-10, 10], [-5, 5]]);
assert.equal(repeat.receipt.identity, arithmetic.receipt.identity);
console.log(JSON.stringify({ status: "passed", executions, nativeComparisons, refused,
  syntheticTritStatesAdmitted: admitted, output }));
