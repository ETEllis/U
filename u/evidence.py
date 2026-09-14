"""Canonical object identity, dependency binding, and recoverable local records.

Hashes establish identity, not signer authority or mathematical truth.
"""

from __future__ import annotations

import dataclasses
import hashlib
import json
import math
import os
import struct
from pathlib import Path
from typing import Any


def tagged(value: Any) -> Any:
    if value is None or isinstance(value, (str, bool)):
        return value
    if isinstance(value, int):
        return {"$int": str(value)}
    if isinstance(value, float):
        return {"$f64": struct.pack(">d", value).hex()}
    if isinstance(value, complex):
        return {"$complex": [tagged(value.real), tagged(value.imag)]}
    if isinstance(value, bytes):
        return {"$bytes": value.hex()}
    if dataclasses.is_dataclass(value):
        return tagged(dataclasses.asdict(value))
    if isinstance(value, dict):
        if not all(isinstance(k, str) for k in value):
            raise TypeError("canonical object keys must be strings")
        # An explicit map envelope prevents marker-shaped user maps from
        # colliding with numeric/byte tags. Keys remain exact Unicode strings.
        return {"$map": [[k, tagged(v)] for k, v in sorted(value.items())]}
    if isinstance(value, tuple):
        return {"$tuple": [tagged(v) for v in value]}
    if isinstance(value, list):
        return [tagged(v) for v in value]
    raise TypeError(f"unserializable semantic value: {type(value).__name__}")


def canonical_bytes(value: Any) -> bytes:
    return json.dumps(tagged(value), ensure_ascii=False, sort_keys=True,
                      separators=(",", ":"), allow_nan=False).encode("utf-8")


def digest(domain: str, value: Any) -> str:
    return "sha256:" + hashlib.sha256(b"etellis.u/identity-v1\0" +
            domain.encode() + b"\0" + canonical_bytes(value)).hexdigest()


def exact_source(data: bytes) -> str:
    return "sha256:" + hashlib.sha256(data).hexdigest()


def strict_json(text: str) -> Any:
    def pairs(items):
        result = {}
        for k, v in items:
            if k in result:
                raise ValueError(f"duplicate JSON key {k}")
            result[k] = v
        return result
    def invalid(value):
        raise ValueError(f"non-finite JSON number {value}")
    def finite_float(value):
        parsed = float(value)
        if not math.isfinite(parsed):
            raise ValueError("JSON number exceeds finite binary64 range")
        return parsed
    return json.loads(text, object_pairs_hook=pairs, parse_constant=invalid, parse_float=finite_float)


def receipt(*, scope: str, verdict: str, meaning: str, realization: str,
            source: str, inputs: Any, output: Any = None,
            obligations=(), assumptions=(), observations=()) -> dict:
    if verdict not in {"Done", "Held", "Rejected", "Fault", "Indeterminate", "Unsupported"}:
        raise ValueError("unknown receipt verdict")
    r = {"schema": "etellis.u.receipt/1", "scope": scope,
         "maturity": {"executed": verdict in {"Done", "Held", "Fault"},
                      "mechanized": False, "physical_device": False},
         "verdict": verdict, "meaning": meaning, "realization": realization,
         "source": source, "inputs": tagged(inputs), "output": tagged(output),
         "obligations": list(obligations), "assumptions": list(assumptions),
         "observations": tagged(list(observations))}
    r["identity"] = digest("receipt", r)
    return r


def verify_receipt(r: dict, *, meaning: str | None = None) -> bool:
    if r.get("schema") != "etellis.u.receipt/1":
        return False
    body = {k: v for k, v in r.items() if k != "identity"}
    return (r.get("identity") == digest("receipt", body) and
            (meaning is None or meaning == r.get("meaning")))


def validate_dag(objects: dict[str, dict]) -> None:
    visiting, done, reachable = set(), set(), {}
    def visit(key):
        if key in visiting:
            raise ValueError("cyclic content-addressed dependency")
        if key in done:
            return reachable[key]
        if key not in objects:
            raise ValueError(f"missing dependency {key}")
        visiting.add(key)
        item = objects[key]
        kinds = {item.get("kind")}
        for dep in item.get("dependencies", []):
            kinds.update(visit(dep))
        if item.get("kind") == "meaning" and "evidence" in kinds:
            raise ValueError("meaning cannot depend transitively on execution evidence")
        visiting.remove(key)
        done.add(key)
        reachable[key] = kinds
        return kinds
    for key in objects:
        visit(key)


class Journal:
    """Local fsync journal with hash-linked records; no authenticity claim.

    Concurrent access uses advisory flock. Torn final records are detected and
    retained as recovery obligations; corruption never silently truncates.
    """
    def __init__(self, path: Path):
        self.path = Path(path)

    def read(self) -> dict:
        if not self.path.exists():
            return {"records": [], "tail": None}
        return self._decode(self.path.read_bytes())

    @staticmethod
    def _decode(raw: bytes) -> dict:
        records, previous = [], "genesis"
        lines = raw.splitlines(keepends=True)
        for index, line in enumerate(lines):
            if not line.endswith(b"\n"):
                return {"records": records, "tail": {"verdict": "Indeterminate", "bytes": len(line)}}
            item = strict_json(line.decode())
            body = {k: v for k, v in item.items() if k != "identity"}
            if item.get("previous") != previous or item.get("identity") != digest("journal", body):
                raise ValueError(f"journal corruption at record {index}")
            if item.get("sequence") != index:
                raise ValueError("journal sequence mismatch")
            records.append(item)
            previous = item["identity"]
        return {"records": records, "tail": None}

    def append(self, payload: dict) -> dict:
        import fcntl
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("a+b") as f:
            fcntl.flock(f, fcntl.LOCK_EX)
            f.seek(0)
            state = self._decode(f.read())
            if state["tail"]:
                raise ValueError("torn tail requires explicit recovery before append")
            items = state["records"]
            item = {"sequence": len(items), "previous": items[-1]["identity"] if items else "genesis",
                    "payload": tagged(payload)}
            item["identity"] = digest("journal", item)
            f.seek(0, os.SEEK_END)
            f.write(json.dumps(item, sort_keys=True, ensure_ascii=False).encode() + b"\n")
            f.flush()
            os.fsync(f.fileno())
        # Persist the directory entry as well as the record where supported.
        fd = os.open(self.path.parent, os.O_RDONLY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)
        return item
