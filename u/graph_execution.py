"""Execute a validated six-tag graph through U's independent direct evaluator.

Reconstruction uses structural nodes and lexical references only. No original
source text, host code, callback payload, or opaque syntax blob is consulted.
"""
from __future__ import annotations

import math
import struct

from .graph import ELABORATION_VERSION, elaborate


class GraphError(ValueError):
    pass


def _name(name):
    pieces = name.split(".")
    node = {"kind": "name", "name": pieces[0]}
    for piece in pieces[1:]:
        node = {"kind": "member", "object": node, "name": piece}
    return node


class Decoder:
    def __init__(self, artifact):
        if artifact.get("version") != ELABORATION_VERSION:
            raise GraphError("unsupported elaboration version")
        self.artifact = artifact
        self.graph = artifact.get("graph", {})
        self.steps = 0
        self.allocated = set()
        self.used_names = set()

    def tick(self):
        self.steps += 1
        if self.steps > 100_000:
            raise GraphError("graph decoding budget exceeded")

    def fresh(self, binder):
        if not isinstance(binder, str) or binder in self.allocated:
            raise GraphError("binder identities must be fresh strings")
        self.allocated.add(binder)
        name = "__kernel_" + binder
        while name in self.used_names:
            name += "_"
        self.used_names.add(name)
        return name

    def scopes(self, node, env):
        params, local = [], dict(env)
        while node.get("tag") == "scope" and node.get("name_kind") == "value-parameter":
            self.tick()
            regions = node.get("regions", [])
            if len(regions) != 2:
                raise GraphError("parameter scope requires type and body regions")
            typ = self.expr(regions[0], local)
            binder = node.get("binder")
            name = self.fresh(binder)
            local[binder] = name
            params.append(dict(name=name, type=typ))
            node = regions[1]
        return params, node, local

    def expr(self, node, env):
        self.tick()
        if not isinstance(node, dict):
            raise GraphError("kernel node must be an object")
        tag, regions = node.get("tag"), node.get("regions", [])
        if tag == "wire":
            if node.get("binder") not in env or regions or node.get("permutation") != [0]:
                raise GraphError("wire must refer to an in-scope single port")
            return _name(env[node["binder"]])
        if tag == "seq":
            statements, local, cursor = [], dict(env), node
            while cursor.get("tag") == "seq":
                self.tick()
                items = cursor.get("regions", [])
                if len(items) != 2:
                    raise GraphError("seq requires two ordered regions")
                value, continuation = self.expr(items[0], local), items[1]
                if continuation.get("tag") == "scope" and continuation.get("name_kind") == "let-result":
                    body = continuation.get("regions", [])
                    if len(body) != 1:
                        raise GraphError("let scope requires one continuation")
                    binder = continuation.get("binder")
                    name = self.fresh(binder)
                    local[binder] = name
                    statements.append(dict(kind="let", name=name, value=value))
                    cursor = body[0]
                else:
                    statements.append(dict(kind="expr", value=value))
                    cursor = continuation
            return dict(kind="block", statements=statements, result=self.expr(cursor, local))
        if tag == "fix":
            if node.get("iteration_mode") != "V.partial" or len(regions) != 2:
                raise GraphError("only declared partial iteration has a stage-0 graph execution rule")
            # Reserve the explicit nominal iteration identity even though its
            # recursion interface is reconstructed from the typed lambda.
            args = [self.expr(region, env) for region in regions]
            self.fresh(node.get("binder"))
            return dict(kind="call", callee=_name("core.fix_partial"), args=args)
        if tag == "par":
            raise GraphError("par execution requires an admitted independence and scheduling realization")
        if tag != "gen":
            raise GraphError("scope outside a declared binding position or unknown kernel tag")
        operator = node.get("operator_id")
        if operator == "value.literal":
            payload = node.get("payload", {})
            kind = payload.get("kind")
            if kind == "integer":
                decimal = payload.get("decimal", "")
                if not isinstance(decimal, str) or len(decimal) > 4097:
                    raise GraphError("invalid canonical integer")
                value, typ = int(decimal), "Int"
                if str(value) != decimal:
                    raise GraphError("noncanonical integer")
            elif kind == "ieee754" and payload.get("width") == 64:
                bits = payload.get("bits", "")
                if not isinstance(bits, str) or len(bits) != 16:
                    raise GraphError("invalid binary64 payload")
                value, typ = struct.unpack(">d", bytes.fromhex(bits))[0], "F64"
                if not math.isfinite(value):
                    raise GraphError("non-finite literal is outside the declared source fragment")
            elif kind == "text" and isinstance(payload.get("value"), str):
                value, typ = payload["value"], "Text"
            elif kind == "bool" and type(payload.get("value")) is bool:
                value, typ = payload["value"], "Bool"
            else:
                raise GraphError("invalid literal payload")
            if regions:
                raise GraphError("literal has no regions")
            return dict(kind="literal", value=value, literal_type=typ)
        if operator in ("value.external", "value.operator_ref"):
            if regions or not isinstance(node.get("symbol"), str):
                raise GraphError("invalid symbol reference")
            return _name(node["symbol"])
        if operator == "record.project":
            if len(regions) != 1 or not isinstance(node.get("field"), str):
                raise GraphError("invalid projection")
            return dict(kind="member", object=self.expr(regions[0], env), name=node["field"])
        if operator == "value.apply":
            if not regions:
                raise GraphError("application requires a callee region")
            return dict(kind="call", callee=self.expr(regions[0], env), args=[self.expr(region, env) for region in regions[1:]])
        if operator == "value.lambda":
            if len(regions) != 1:
                raise GraphError("lambda requires its bound body region")
            params, body, local = self.scopes(regions[0], env)
            if node.get("arity") != len(params):
                raise GraphError("lambda arity disagrees with scopes")
            return dict(kind="lambda", params=params, body=self.expr(body, local))
        if operator in ("value.list", "value.tuple"):
            return dict(kind=operator.split(".")[1], items=[self.expr(region, env) for region in regions])
        if operator == "value.record":
            fields = node.get("fields", [])
            if len(fields) != len(regions) or len(set(fields)) != len(fields):
                raise GraphError("record fields must uniquely match ordered regions")
            return dict(kind="record", fields={field: self.expr(region, env) for field, region in zip(fields, regions)})
        if operator in ("value.module", "value.definition"):
            raise GraphError("module/definition outside its structural position")
        if not isinstance(operator, str):
            raise GraphError("missing generator identity")
        return dict(kind="call", callee=_name(operator), args=[self.expr(region, env) for region in regions])

    def run(self):
        graph = self.graph
        if graph.get("tag") != "gen" or graph.get("operator_id") != "value.module":
            raise GraphError("artifact root must be a U module generator")
        exports, regions = graph.get("exports", []), graph.get("regions", [])
        if len(exports) != len(regions):
            raise GraphError("module exports and definition regions differ")
        env = {}
        for export in exports:
            binder, name = export.get("binder"), export.get("name")
            if not isinstance(name, str) or name in self.used_names or binder in env:
                raise GraphError("duplicate or invalid module export")
            env[binder] = name
            self.used_names.add(name)
            self.allocated.add(binder)
        definitions = []
        for export, region in zip(exports, regions):
            if region.get("tag") != "scope" or region.get("name_kind") != "definition" or region.get("binder") != export["binder"] or len(region.get("regions", [])) != 1:
                raise GraphError("definition scope does not match its export")
            params, node, local = self.scopes(region["regions"][0], env)
            if node.get("tag") != "gen" or node.get("operator_id") != "value.definition" or len(node.get("regions", [])) != 2:
                raise GraphError("invalid definition generator")
            if node.get("arity") != len(params):
                raise GraphError("definition arity disagrees with scopes")
            definitions.append(dict(name=export["name"], params=params,
                                    type=self.expr(node["regions"][0], local),
                                    body=self.expr(node["regions"][1], local),
                                    is_function=node.get("is_function")))
        module = dict(version=graph.get("version"), imports=graph.get("imports"), definitions=definitions, source="")
        # Re-elaboration validates every rule digest, policy, stage, port,
        # binder, profile, and contract field. Unknown extra node metadata is
        # rejected too; a hash by itself never grants these contracts.
        canonical = elaborate(module)
        if canonical["structural_digest"] != self.artifact.get("structural_digest") or canonical["graph"] != graph:
            raise GraphError("graph is not canonical under the declared elaboration rules")
        if canonical["semantic_digest"] != self.artifact.get("semantic_digest"):
            raise GraphError("semantic profile or rule dependencies have changed")
        for key in ("profiles", "operators", "usage_requirements"):
            if canonical[key] != self.artifact.get(key):
                raise GraphError(f"artifact {key} disagree with checked graph dependencies")
        return module


def reconstruct(artifact: dict) -> dict:
    try:
        return Decoder(artifact).run()
    except (RecursionError, KeyError, TypeError, ValueError) as error:
        if isinstance(error, GraphError):
            raise
        raise GraphError(f"invalid or over-budget kernel graph: {error}") from None


def run_graph(artifact: dict, entry: str, args=None, *, capabilities=(), budget=100_000):
    module = reconstruct(artifact)
    from .checker import check
    from .evaluator import Evaluator
    checked = check(module)
    if checked["status"] == "rejected":
        raise GraphError("reconstructed graph fails static checks: " + "; ".join(item["message"] for item in checked["diagnostics"]))
    if any(not profile.get("admitted") for profile in artifact["profiles"]):
        raise GraphError("graph contains an unadmitted theory composition")
    return Evaluator(module, capabilities=capabilities, budget=budget).run(entry, [] if args is None else args)
