"""Direct native C and WebAssembly lowering for interval-certified integer expressions.

This is an admitted bounded-input realization of exact Int arithmetic, not a
reinterpretation of Int as wrapping machine arithmetic. Every intermediate is
proved to fit i64 by interval arithmetic; both targets guard input bounds.
"""

from pathlib import Path
import json
import subprocess
from .proof import qualified
from .evidence import digest

LOW, HIGH = -(2**63), 2**63 - 1


def uleb(n):
    out = bytearray()
    while True:
        byte, n = n & 127, n >> 7
        out.append(byte | (128 if n else 0))
        if not n:
            return bytes(out)


def sleb(n):
    out = bytearray()
    while True:
        byte, n = n & 127, n >> 7
        done = (n == 0 and not byte & 64) or (n == -1 and byte & 64)
        out.append(byte | (0 if done else 128))
        if done:
            return bytes(out)


def section(n, body):
    return bytes([n]) + uleb(len(body)) + body


def certify(module, entry, bounds):
    if any(p != 'u.standard/0.1' for p in module['imports']):
        raise ValueError('compiled fragment requires the pinned standard numeric profile')
    if any(d['name'] in {'int', 'nat'} for d in module['definitions']):
        raise ValueError('shadowed arithmetic namespace is outside the compiled fragment')
    definition = next((d for d in module["definitions"] if d["name"] == entry), None)
    if definition is None:
        raise ValueError("backend entry not found")
    params = definition["params"]
    if len(bounds) != len(params):
        raise ValueError("one explicit interval required per parameter")
    for param, (lo, hi) in zip(params, bounds):
        if type(lo) is not int or type(hi) is not int:
            raise ValueError('integer interval endpoints required')
        if param['name'] in {'int', 'nat'}:
            raise ValueError('shadowed arithmetic namespace')
        if qualified(param["type"]) not in {"Int", "Nat"}:
            raise ValueError("backend admits only exact Int/Nat parameter expressions")
        if not LOW <= lo <= hi <= HIGH or (qualified(param["type"]) == "Nat" and lo < 0):
            raise ValueError("invalid input interval")
    if qualified(definition["type"]) not in {"Int", "Nat"}:
        raise ValueError("backend return type outside integer fragment")
    env = {p["name"]: (i, tuple(bound)) for i, (p, bound) in enumerate(zip(params, bounds))}
    derivation = []
    def expression(e):
        if e["kind"] == "literal" and type(e["value"]) is int:
            n = e["value"]
            code, c, interval = b"\x42" + sleb(n), f"INT64_C({n})" if n != LOW else "INT64_MIN", (n, n)
        elif e["kind"] == "name" and e["name"] in env:
            index, interval = env[e["name"]]
            code, c = b"\x20" + uleb(index), f"p{index}"
        elif e["kind"] == "call" and qualified(e["callee"]) in {"int.add", "int.sub", "int.mul", "nat.add", "nat.mul"} and len(e["args"]) == 2:
            left, right = [expression(a) for a in e["args"]]
            op = qualified(e["callee"]).split(".")[1]
            a, b = left[2]
            x, y = right[2]
            if op == "add":
                interval, opcode, symbol = (a + x, b + y), 0x7c, "+"
            elif op == "sub":
                interval, opcode, symbol = (a - y, b - x), 0x7d, "-"
            else:
                products = [a*x, a*y, b*x, b*y]
                interval, opcode, symbol = (min(products), max(products)), 0x7e, "*"
            code, c = left[0] + right[0] + bytes([opcode]), f"({left[1]} {symbol} {right[1]})"
        else:
            raise ValueError("backend-expression-unsupported; no approximation emitted")
        if not LOW <= interval[0] <= interval[1] <= HIGH:
            raise ValueError("cannot prove intermediate fits i64")
        derivation.append({"expression": e, "range": interval})
        return code, c, interval
    code, c, interval = expression(definition["body"])
    if qualified(definition["type"]) == "Nat" and interval[0] < 0:
        raise ValueError("cannot prove Nat result is nonnegative")
    return {"code": code, "c": c, "bounds": bounds, "params": params,
            "entry": entry, "range": interval, "derivation": derivation}


def wasm(cert):
    n = len(cert["params"])
    types = b"\x01\x60" + uleb(n) + b"\x7e" * n + b"\x01\x7e"
    name = cert["entry"].encode()
    exports = b"\x01" + uleb(len(name)) + name + b"\x00\x00"
    guard = bytearray()
    for i, (lo, hi) in enumerate(cert["bounds"]):
        for bound, comparison in ((lo, 0x53), (hi, 0x55)):
            # if (parameter < lower || parameter > upper): unreachable
            guard += b"\x20" + uleb(i) + b"\x42" + sleb(bound) + bytes([comparison]) + b"\x04\x40\x00\x0b"
    body = b"\x00" + bytes(guard) + cert["code"] + b"\x0b"
    return b"\0asm\x01\0\0\0" + section(1, types) + section(3, b"\x01\x00") + section(7, exports) + section(10, b"\x01" + uleb(len(body)) + body)


def c_source(cert):
    lines = ["#include <stdint.h>", "#include <inttypes.h>", "#include <errno.h>",
             "#include <stdlib.h>", "#include <stdio.h>", "int main(int argc, char **argv) {",
             f"  if (argc != {len(cert['params']) + 1}) return 2;"]
    for i, (lo, hi) in enumerate(cert["bounds"]):
        lower = "INT64_MIN" if lo == LOW else f"INT64_C({lo})"
        lines += [f"  errno = 0; char *end{i};",
                  f"  int64_t p{i} = strtoll(argv[{i+1}], &end{i}, 10);",
                  f"  if (errno || *end{i} || end{i} == argv[{i+1}] || p{i} < {lower} || p{i} > INT64_C({hi})) return 3;"]
    lines += [f'  printf("%" PRId64 "\\n", (int64_t){cert["c"]});', "  return 0;", "}"]
    return "\n".join(lines) + "\n"


def build(module, entry, bounds, output: Path, target):
    cert = certify(module, entry, bounds)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        raise ValueError("output already exists; choose a new artifact path")
    if target == "wasm":
        output.write_bytes(wasm(cert))
    elif target == "native":
        source = output.with_suffix(".c")
        if source.exists():
            raise ValueError("native C output already exists")
        source.write_text(c_source(cert))
        subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-O2", str(source), "-o", str(output)], check=True)
    else:
        raise ValueError("unsupported realization target")
    contract = {k: v for k, v in cert.items() if k not in {"code", "c"}}
    result = {"schema": "etellis.u.integer-lowering/1", "verdict": "Done", "target": target,
              "scope": "exact Int/Nat expression over guarded certified input intervals",
              "contract": contract, "identity": digest("integer-realization", [contract, target, output.read_bytes()])}
    output.with_suffix(output.suffix + ".json").write_text(json.dumps(result, indent=2) + "\n")
    return result
