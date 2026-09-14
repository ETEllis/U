"""Independent U realization of the pinned CDC finite-map primitive fragment.

This module never launches the BiDi executable. The isolated oracle belongs
to tests. State dictionaries are copied at operation boundaries; source order
and binary64 operation order are retained. No global rollback on HOLD.
"""

from __future__ import annotations

from copy import deepcopy
from dataclasses import dataclass
import math
import shlex
from .evidence import digest, exact_source

PROFILE = "etellis.cdc/native-0.3.0@1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157"
NUMERIC_PROFILE = "binary64-clang-arm64-contracted-last-mul-add-libm/1"
_TANGENTS = {}
_RECURRENCES = {}
_RETURNS = {}


def _admit_artifact(registry, value):
    registry[id(value)] = (value, digest("cdc-earned-artifact", value))
    return value


def _is_artifact(registry, value):
    found = registry.get(id(value))
    return found is not None and found[0] is value and found[1] == digest("cdc-earned-artifact", value)


def find(items, name):
    for item in items:
        if item["name"] == name:
            return item
    raise ValueError("CDC unknown reference: " + str(name))


def make_state(spec):
    result = {key: deepcopy(spec.get(key, [])) for key in ("fields", "modules", "cells", "channels")}
    for key in ("fields", "modules", "cells"):
        names = [item["name"] for item in result[key]]
        if len(set(names)) != len(names):
            raise ValueError("duplicate CDC " + key + " name")
    for field in result["fields"]:
        field.setdefault("gain", 1.0)
        field.setdefault("deadband", 0.5)
        field.setdefault("dt", 0.01)
    for module in result["modules"]:
        find(result["fields"], module["field"])
        module.setdefault("belief", 0.0)
        module.setdefault("prior", 0.0)
    for cell in result["cells"]:
        find(result["modules"], cell["module"])
        for key, value in {"theta": 0.0, "omega": 0.0, "has_latch": False, "latch": 0}.items():
            cell.setdefault(key, value)
    for edge in result["channels"]:
        find(result["cells"], edge["source"])
        find(result["cells"], edge["target"])
        edge.setdefault("weight", 1.0)
        edge.setdefault("angle", 0.0)
        edge.setdefault("delay", 0.0)
    for group in result.values():
        for item in group:
            if any(isinstance(v, float) and not math.isfinite(v) for v in item.values()):
                raise ValueError("CDC finite-input fragment excludes nonfinite parameters")
    return result


def flow(state, field_name, duration):
    if not math.isfinite(duration):
        raise ValueError("nonfinite duration")
    new = deepcopy(state)
    field = find(state["fields"], field_name)
    indices = {c["name"]: i for i, c in enumerate(state["cells"])}
    eligible = {c["name"] for c in state["cells"] if find(state["modules"], c["module"])["field"] == field_name}
    next_theta = [float(c["theta"]) for c in state["cells"]]
    for i, cell in enumerate(state["cells"]):
        if cell["name"] in eligible:
            next_theta[i] = math.fma(cell["omega"], duration, next_theta[i])
    for channel in state["channels"]:
        if channel["source"] not in eligible or channel["target"] not in eligible:
            continue
        source, target = (state["cells"][indices[channel[k]]] for k in ("source", "target"))
        phase = math.sin(source["theta"] + channel["angle"] - target["theta"])
        i = indices[target["name"]]
        next_theta[i] = math.fma(field["gain"] * channel["weight"] * phase, duration, next_theta[i])
    for i, cell in enumerate(new["cells"]):
        if cell["name"] in eligible:
            cell["theta"] = next_theta[i]
    return {"verdict": "Done", "status": "accepted", "state": new, "operation": "cdc.flow"}


def quantize(theta, deadband):
    value = math.cos(theta)
    return 1 if value > deadband else -1 if value < -deadband else 0


def commit(state, module_name):
    new = deepcopy(state)
    module = find(state["modules"], module_name)
    field = find(state["fields"], module["field"])
    cells = [c for c in new["cells"] if c["module"] == module_name]
    if not cells:
        raise ValueError("commit module has no cells")
    trits = [quantize(c["theta"], field["deadband"]) for c in cells]
    running, accepted = 0, True
    for t in trits:
        running += t
        accepted = accepted and running >= 0
    if accepted:
        for c, t in zip(cells, trits):
            c["latch"], c["has_latch"] = t, True
    return {"verdict": "Done" if accepted else "Held", "state": new,
            "status": "accepted" if accepted else "held", "trits": trits,
            "balance": "admissible" if accepted else "violated",
            "reason": "none" if accepted else "balance-violation",
            "operation": "cdc.commit", "earned_artifacts": []}


def nest(state, parent_name, child_name):
    new = deepcopy(state)
    parent = find(new["modules"], parent_name)
    child = find(new["modules"], child_name)
    if parent["field"] != child["field"]:
        raise ValueError("nest modules must share a field")
    field = find(new["fields"], parent["field"])
    cells = [c for c in new["cells"] if c["module"] == child_name]
    if not cells:
        raise ValueError("nest child has no cells")
    total = 0.0
    for c in cells:
        total += float(c["latch"] if c["has_latch"] else quantize(c["theta"], field["deadband"]))
    up = total / float(len(cells))
    parent["belief"] = math.fma(field["gain"], up, parent["belief"])
    child["prior"] = parent["belief"]
    return {"verdict": "Done", "status": "accepted", "state": new, "up": up,
            "parent_belief": parent["belief"], "child_prior": child["prior"], "operation": "cdc.nest"}


def parse_cdc(text):
    declarations = {"fields": [], "modules": [], "cells": [], "channels": []}
    statements, steps, unsupported = [], [], []
    groups = {"field": "fields", "module": "modules", "cell": "cells"}
    numeric = {"dt", "gain", "deadband", "belief", "prior", "theta", "omega", "amplitude", "precision", "weight", "angle", "delay"}
    for line_number, line in enumerate(text.splitlines(), 1):
        words = shlex.split(line, comments=True)
        if not words:
            continue
        if len(words) < 2:
            raise ValueError(f"CDC line {line_number}: missing identifier")
        kind, name = words[:2]
        pairs = [w.split("=", 1) for w in words[2:] if "=" in w]
        first, last = {}, {}
        for key, value in pairs:
            first.setdefault(key, value)
            last[key] = value
        statements.append({"line": line_number, "kind": kind, "name": name,
                           "ordered_attributes": pairs, "native_first": first, "registry_last": last})
        attrs = {k: float(v) if k in numeric else v for k, v in first.items()}
        if kind in groups:
            declarations[groups[kind]].append(dict(attrs, name=name))
        elif kind == "channel":
            if len(words) < 4 or words[2] != "->":
                raise ValueError("CDC channel requires source -> target")
            declarations["channels"].append(dict(attrs, source=name, target=words[3]))
        elif kind in {"flow", "commit", "nest"}:
            steps.append({"kind": kind, "name": name, "attrs": first})
        else:
            unsupported.append({"line": line_number, "directive": kind,
                                "status": "residual-preserved, unsupported native import"})
    return {"state": make_state(declarations), "steps": steps, "statements": statements,
            "residual": text, "source_identity": exact_source(text.encode()),
            "profile": PROFILE, "unsupported": unsupported}


def _expect(result, attrs):
    tolerance = float(attrs.get("tolerance", "0.000001"))
    if not math.isfinite(tolerance) or tolerance < 0:
        raise ValueError("invalid expectation tolerance")
    def close(actual, expected):
        expected_value = float(expected)
        if not math.isfinite(actual) or not math.isfinite(expected_value) or abs(actual - expected_value) > tolerance:
            raise ValueError("CDC post-step expectation mismatch")
    if "expect-theta" in attrs:
        name, value = attrs["expect-theta"].rsplit(":", 1)
        close(find(result["state"]["cells"], name)["theta"], value)
    for key, field in (("expect-parent-belief", "parent_belief"), ("expect-child-prior", "child_prior")):
        if key in attrs:
            close(result[field], attrs[key])
    for key, field in (("expect-status", "status"), ("expect-reason", "reason"), ("expect-balance", "balance")):
        if key in attrs and attrs[key] != result[field]:
            raise ValueError("CDC post-step " + key + " mismatch")
    if "expect-trits" in attrs:
        trits = "".join("+" if t == 1 else "-" if t == -1 else "0" for t in result["trits"])
        if trits != attrs["expect-trits"]:
            raise ValueError("CDC post-step trit mismatch")


def execute_source(text, budget=100000):
    program = parse_cdc(text)
    if program["unsupported"]:
        return {"verdict": "Unsupported", "obligations": program["unsupported"],
                "source_identity": program["source_identity"], "state": program["state"]}
    state, trace = program["state"], []
    for step in program["steps"]:
        if len(trace) >= budget:
            return {"verdict": "Held", "reason": "execution-budget", "state": state, "trace": trace}
        attrs = step["attrs"]
        if step["kind"] == "flow":
            result = flow(state, attrs["field"], float(attrs["duration"]))
        elif step["kind"] == "commit":
            result = commit(state, attrs["module"])
        else:
            result = nest(state, attrs["parent"], attrs["child"])
        state = result["state"]
        trace.append(dict(result, step=step["name"]))
        try:
            _expect(result, attrs)
        except ValueError as error:
            return {"verdict": "Fault", "reason": str(error), "state": state,
                    "trace": trace, "known_progress": len(trace)}
    return {"verdict": "Done", "state": state, "trace": trace, "profile": PROFILE,
            "source_identity": program["source_identity"]}


def coordinates(state):
    return (["theta:" + c["name"] for c in state["cells"]] +
            [k + ":" + m["name"] for m in state["modules"] for k in ("belief", "prior")])


def vector(state):
    return ([c["theta"] for c in state["cells"]] +
            [m[k] for m in state["modules"] for k in ("belief", "prior")])


def identity(n):
    return [[float(i == j) for j in range(n)] for i in range(n)]


def matmul(a, b):
    if not a or not b or any(len(row) != len(b) for row in a):
        raise ValueError("matrix dimension mismatch")
    width = len(b[0])
    if any(len(row) != width for row in b):
        raise ValueError("ragged matrix")
    return [[sum(a[i][k] * b[k][j] for k in range(len(b))) for j in range(width)] for i in range(len(a))]


def jacobian(state, kind, attrs):
    manifest = coordinates(state)
    matrix = identity(len(manifest))
    if kind == "flow":
        field = find(state["fields"], attrs["field"])
        duration = float(attrs["duration"])
        cells = {c["name"]: c for c in state["cells"]}
        for edge in state["channels"]:
            if edge["delay"] != 0:
                raise ValueError("delay-history-derivative-unavailable")
            source, target = cells[edge["source"]], cells[edge["target"]]
            if any(find(state["modules"], c["module"])["field"] != field["name"] for c in (source, target)):
                continue
            i, j = manifest.index("theta:" + target["name"]), manifest.index("theta:" + source["name"])
            derivative = field["gain"] * edge["weight"] * math.cos(source["theta"] + edge["angle"] - target["theta"]) * duration
            matrix[i][j] += derivative
            matrix[i][i] -= derivative
    elif kind == "nest":
        child, parent = attrs["child"], attrs["parent"]
        module = find(state["modules"], child)
        field = find(state["fields"], module["field"])
        for c in state["cells"]:
            if c["module"] == child and not c["has_latch"] and abs(abs(math.cos(c["theta"])) - field["deadband"]) < 1e-12:
                raise ValueError("quantization-boundary")
        i, j = manifest.index("prior:" + child), manifest.index("belief:" + parent)
        matrix[i] = matrix[j][:]
    elif kind == "commit":
        module = find(state["modules"], attrs["module"])
        field = find(state["fields"], module["field"])
        for c in state["cells"]:
            if c["module"] == module["name"] and abs(abs(math.cos(c["theta"])) - field["deadband"]) < 1e-12:
                raise ValueError("quantization-boundary")
    else:
        raise ValueError("unknown local derivative")
    return matrix


def path_tangent(text):
    program = parse_cdc(text)
    if program["unsupported"]:
        return {"verdict": "Unsupported", "obligations": program["unsupported"]}
    initial, state = program["state"], program["state"]
    tangent = identity(len(coordinates(state)))
    for step in program["steps"]:
        attrs, kind = step["attrs"], step["kind"]
        try:
            local = jacobian(state, kind, attrs)
        except ValueError as e:
            return {"verdict": "Held", "reason": str(e), "earned_artifacts": []}
        result = (flow(state, attrs["field"], float(attrs["duration"])) if kind == "flow" else
                  commit(state, attrs["module"]) if kind == "commit" else
                  nest(state, attrs["parent"], attrs["child"]))
        if result["verdict"] != "Done":
            return {"verdict": "Held", "reason": "unaccepted-commit-path", "earned_artifacts": []}
        try:
            _expect(result, attrs)
        except ValueError as e:
            return {"verdict": "Fault", "reason": str(e), "state": result["state"]}
        tangent, state = matmul(local, tangent), result["state"]
    return _admit_artifact(_TANGENTS, {"verdict": "Done", "kind": "PathTangent", "initial": initial, "final": state,
            "manifest": coordinates(state), "matrix": tangent, "source_identity": program["source_identity"],
            "path_identity": digest("cdc-executed-path", [PROFILE, text, initial, state]),
            "derivative_scope": "real-valued extension of ordered executed finite map; fixed itinerary"})


@dataclass(frozen=True)
class VerifiedRecurrence:
    path_identity: str
    manifest: tuple
    restoration_derivative: tuple
    mode: str
    tolerance: float
    norm: str
    residual: float
    claim: str


def check_recurrence(tangent, request):
    if not _is_artifact(_TANGENTS, tangent):
        return {"verdict": "Held", "reason": "path-tangent-required", "earned_artifacts": []}
    initial, final = tangent["initial"], tangent["final"]
    manifest = tuple(tangent["manifest"])
    mode = request.get("mode", "absolute")
    if request.get("coordinates", "all") != "all" or coordinates(initial) != coordinates(final):
        return {"verdict": "Held", "reason": "incomplete-state-manifest", "earned_artifacts": [tangent]}
    x0, x1 = vector(initial), vector(final)
    if [(c["has_latch"], c["latch"]) for c in initial["cells"]] != [(c["has_latch"], c["latch"]) for c in final["cells"]]:
        return {"verdict": "Held", "reason": "recurrence-mode-mismatch", "earned_artifacts": [tangent]}
    if mode != "absolute":
        # An arbitrary restoration callback is not a symmetry certificate.
        return {"verdict": "Held", "reason": "relative-symmetry-proof-required", "earned_artifacts": [tangent]}
    tolerance = request.get("tolerance", 1e-9)
    if type(tolerance) not in {float, int} or not math.isfinite(tolerance) or tolerance < 0:
        raise ValueError("invalid recurrence tolerance")
    if request.get("norm", "linf") != "linf":
        raise ValueError("unsupported recurrence norm")
    if any(not math.isfinite(a) or not math.isfinite(b) or abs(a - b) > tolerance for a, b in zip(x0, x1)):
        return {"verdict": "Held", "reason": "recurrence-state-mismatch", "earned_artifacts": [tangent]}
    residual = max((abs(a-b) for a,b in zip(x0,x1)), default=0.0)
    cert = VerifiedRecurrence(tangent["path_identity"], manifest,
                              tuple(map(tuple, identity(len(manifest)))), mode,
                              float(tolerance), "linf", residual,
                              "numerically accepted recurrence under declared tolerance; not an exact periodic-orbit theorem")
    _admit_artifact(_RECURRENCES, cert)
    # The certificate is held privately by the runtime; U data cannot construct its class.
    return {"verdict": "Done", "value": cert, "earned_artifacts": [tangent]}


def return_map(tangent, certificate):
    if not _is_artifact(_TANGENTS, tangent) or not _is_artifact(_RECURRENCES, certificate) or not isinstance(certificate, VerifiedRecurrence) or certificate.path_identity != tangent.get("path_identity") or certificate.manifest != tuple(tangent.get("manifest", [])):
        raise ValueError("recurrence-certificate-path-mismatch")
    return _admit_artifact(_RETURNS, {"verdict": "Done", "kind": "ReturnMap", "path_identity": certificate.path_identity,
            "matrix": matmul(certificate.restoration_derivative, tangent["matrix"]),
            "recurrence_mode": certificate.mode,
            "recurrence_contract": {"tolerance":certificate.tolerance, "norm":certificate.norm,
                                    "residual":certificate.residual, "claim":certificate.claim}})


def spectral(return_operator, options):
    if not _is_artifact(_RETURNS, return_operator):
        raise ValueError("spectral analysis requires an earned return map")
    # Do not call eigensolvers on an unvalidated matrix and invent a Schur receipt.
    return {"verdict": "Held", "reason": "validated-real-schur-backend-unavailable",
            "earned_artifacts": [return_operator]}


def register(evaluator):
    for name, fn in {"cdc.make_state": make_state, "cdc.flow": flow, "cdc.commit": commit,
                     "cdc.nest": nest, "recurrence.check": check_recurrence,
                     "linear.return_map": return_map, "spectral.real_schur": spectral}.items():
        evaluator.register(name, fn)
    def then(result, continuation):
        if result.get("verdict") != "Done":
            return result
        value = result.get("value", result)
        following = evaluator.invoke(continuation, [value])
        if isinstance(following, dict) and following.get("verdict") == "Held":
            following = dict(following, earned_artifacts=result.get("earned_artifacts", []) + following.get("earned_artifacts", []))
        return following
    evaluator.register("analysis.then", then)
