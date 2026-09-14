"""Deterministic whole-surface elaboration into six immutable kernel tags.

This layer preserves syntax and dependency obligations. Its output is not, by
itself, a typing derivation or an operator-admission certificate.
"""
from __future__ import annotations

from dataclasses import dataclass
import struct

from .source import qualified_name
from .evidence import digest

ELABORATION_VERSION = "etellis.u/elaboration-1"
KERNEL_TAGS = frozenset(("wire", "gen", "seq", "par", "scope", "fix"))
THEORIES = {
    "mem": "M", "gpu": "M", "io": "M", "http": "C", "async": "C",
    "actor": "C", "temporal": "C", "duration": "C", "model": "C",
    "rel": "R", "logic": "R", "sql": "R", "prob": "P", "dist": "P",
    "dynamics": "D", "hw": "D", "cdc": "D", "topology": "D",
    "ad": "D", "recurrence": "D", "spectral": "D", "linear": "D",
    "quantum": "Q",
}

# These are inspectable semantic rule references, not executable callbacks or
# certificates. Each reference participates in the operation's identity.
DERIVED_RULES = {
    "sql.inner_join": "bag.cartesian; sql.predicate_three_valued; retain_true_with_multiplicity",
    "sql.project": "bag.map_preserving_multiplicity",
    "list.map": "list.fold_right_cons_applied_element",
    "list.filter": "list.fold_right_cons_when_predicate_true",
    "syntax.bind_once": "fresh_nominal_let; evaluate_argument_once; capture_avoiding_splice",
    "actor.supervise": "spawn_children; observe_failure; bounded_restart_protocol",
    "gpu.partition_launch": "checked_disjoint_index_leases; bounded_active_lanes; ordered_join",
    "cdc.u1.close": "same_executed_path; reciprocal; oriented_cover; accepted_commits; generated_coordinate; effect_binding",
}
REGION_POLICIES = {
    "syntax.quote": "staged", "syntax.bind_once": "staged-substitution",
    "logic.relation": "declarative", "logic.exists": "declarative",
    "logic.query": "ordered-search", "prob.model": "measure",
    "quantum.program": "quantum-program", "hw.reg": "clocked",
    "dynamics.dae": "equations", "mem.with_mut": "fresh-exclusive-lifetime",
    "gpu.partition_launch": "partitioned-device-region",
    "core.fix_partial": "partial-fixed-point",
}


def _digest(domain, value):
    return digest(domain, value)


def _freeze(value):
    if isinstance(value, dict):
        return ("mapping", tuple((k, _freeze(v)) for k, v in sorted(value.items())))
    if isinstance(value, (list, tuple)):
        return ("sequence", tuple(_freeze(v) for v in value))
    return ("scalar", value)


def _thaw(value):
    kind, body = value
    if kind == "mapping":
        return {k: _thaw(v) for k, v in body}
    if kind == "sequence":
        return [_thaw(v) for v in body]
    return body


@dataclass(frozen=True)
class KernelNode:
    tag: str
    attributes: tuple
    regions: tuple["KernelNode", ...] = ()

    def __post_init__(self):
        if self.tag not in KERNEL_TAGS:
            raise ValueError(f"invalid kernel tag: {self.tag}")

    @classmethod
    def make(cls, tag, *, regions=(), **attributes):
        return cls(tag, tuple((k, _freeze(v)) for k, v in sorted(attributes.items())), tuple(regions))

    def to_data(self):
        return {"tag": self.tag, **{k: _thaw(v) for k, v in self.attributes},
                "regions": [region.to_data() for region in self.regions]}


def literal_payload(value):
    if isinstance(value, bool):
        return {"kind": "bool", "value": value}
    if isinstance(value, int):
        return {"kind": "integer", "decimal": str(value)}
    if isinstance(value, float):
        return {"kind": "ieee754", "width": 64, "bits": struct.pack(">d", value).hex()}
    if isinstance(value, str):
        return {"kind": "text", "value": value}
    raise ValueError("unsupported literal carrier")


def operator_descriptor(name):
    family = THEORIES.get(name.split(".", 1)[0], "V")
    descriptor = {
        "name": name, "theory": family, "version": "etellis.u/operators-1",
        "origin": "specified-derived" if name in DERIVED_RULES else "specified-generator",
        "region_policy": REGION_POLICIES.get(name, "strict-left-to-right"),
        "reference_rule": DERIVED_RULES.get(name, f"u.reference/{name}/1"),
        "expansion_status": "specified-unverified" if name in DERIVED_RULES else "primitive-rule-reference",
        "proof_status": "not-established-by-elaboration",
        "native_support": "requires-runtime-capability-ledger",
        "trust_dependencies": ["stage0-elaborator"],
    }
    descriptor["signature_digest"] = _digest("u.operator-signature/1", descriptor)
    return descriptor


class Elaborator:
    def __init__(self, module):
        self.module = module
        self.bindings = []
        self.operators = {}
        self.obligations = []
        self.counter = 0
        self.wire_counts = {}
        self.binding_types = {}

    def binder(self, name, role, annotation=None):
        identity = f"b{self.counter:06d}"
        self.counter += 1
        self.bindings.append(dict(id=identity, name_hint=name, role=role))
        self.binding_types[identity] = annotation
        return identity

    def gen(self, name, args=(), **attributes):
        descriptor = operator_descriptor(name)
        self.operators[name] = descriptor
        return KernelNode.make("gen", regions=args, theory_id=descriptor["theory"],
                               operator_id=name, signature_digest=descriptor["signature_digest"],
                               **attributes)

    def expr(self, expr, env, stage=0):
        kind = expr["kind"]
        if kind == "literal":
            return self.gen("value.literal", payload=literal_payload(expr["value"]), stage=stage)
        if kind == "name":
            name = expr["name"]
            if name in env:
                self.wire_counts[env[name]] = self.wire_counts.get(env[name], 0) + 1
                return KernelNode.make("wire", binder=env[name], permutation=[0], stage=stage)
            return self.gen("value.external", symbol=name, resolution="requires-type-or-operator-check", stage=stage)
        if kind == "member":
            name = qualified_name(expr)
            if name and name.split(".", 1)[0] not in env:
                return self.gen("value.operator_ref", symbol=name,
                                target_digest=operator_descriptor(name)["signature_digest"], stage=stage)
            return self.gen("record.project", [self.expr(expr["object"], env, stage)],
                            field=expr["name"], stage=stage)
        if kind == "call":
            name = qualified_name(expr["callee"])
            operator = bool(name and "." in name and name.split(".", 1)[0] not in env)
            policy = REGION_POLICIES.get(name, "strict-left-to-right")
            arg_stage = stage + 1 if name == "syntax.quote" else stage
            regions = [self.expr(arg, env, arg_stage) for arg in expr["args"]]
            if name == "core.fix_partial" and operator:
                # The binder is nominal and distinct from the lambda's actual
                # parameters; the operator rule connects its recursive port.
                bound = self.binder("partial-fix", "iteration")
                return KernelNode.make("fix", regions=regions, binder=bound,
                                       iteration_mode="V.partial", rule_dependency=operator_descriptor(name)["signature_digest"],
                                       stage=stage, total_conversion=False)
            if operator:
                return self.gen(name, regions, ordered_ports=list(range(len(regions))),
                                region_policy=policy, stage=stage)
            return self.gen("value.apply", [self.expr(expr["callee"], env, stage), *regions],
                            ordered_ports=list(range(len(regions) + 1)), stage=stage)
        if kind == "lambda":
            local, binders, annotations = dict(env), [], []
            for param in expr["params"]:
                annotations.append(self.expr(param["type"], local, stage))
                identity = self.binder(param["name"], "parameter", param["type"])
                local[param["name"]] = identity
                binders.append(identity)
            node = self.expr(expr["body"], local, stage)
            for identity, annotation in reversed(list(zip(binders, annotations))):
                node = KernelNode.make("scope", regions=[annotation, node], binder=identity,
                                       name_kind="value-parameter", stage=stage)
            return self.gen("value.lambda", [node], arity=len(binders), stage=stage)
        if kind in ("list", "tuple"):
            return self.gen(f"value.{kind}", [self.expr(x, env, stage) for x in expr["items"]], stage=stage)
        if kind == "record":
            # Field order determines evaluation order in the strict profile.
            return self.gen("value.record", [self.expr(v, env, stage) for v in expr["fields"].values()],
                            fields=list(expr["fields"]), stage=stage)
        if kind == "block":
            local, pending = dict(env), []
            for statement in expr["statements"]:
                value = self.expr(statement["value"], local, stage)
                if statement["kind"] == "let":
                    identity = self.binder(statement["name"], "let")
                    local[statement["name"]] = identity
                else:
                    identity = None
                pending.append((value, identity))
            result = self.expr(expr["result"], local, stage)
            for value, identity in reversed(pending):
                if identity is not None:
                    result = KernelNode.make("scope", regions=[result], binder=identity, name_kind="let-result", stage=stage)
                result = KernelNode.make("seq", regions=[value, result], ordering="strict-left-to-right", stage=stage)
            return result
        raise ValueError(f"unrecognized AST kind: {kind}")

    def run(self):
        definitions = self.module["definitions"]
        globals_ = {definition["name"]: self.binder(definition["name"], "definition") for definition in definitions}
        regions, profiles = [], []
        dependencies = {
            definition["name"]: _semantic_dependencies(definition["body"], set(globals_),
                                                        {parameter["name"] for parameter in definition["params"]})
            for definition in definitions
        }
        family_sets = {name: families for name, (families, _) in dependencies.items()}
        calls = {name: references for name, (_, references) in dependencies.items()}
        for _ in range(len(definitions)):
            for name, references in calls.items():
                for reference in references:
                    family_sets[name].update(family_sets[reference])
        for definition in definitions:
            env = dict(globals_)
            binders, annotations = [], []
            for param in definition["params"]:
                annotations.append(self.expr(param["type"], env))
                identity = self.binder(param["name"], "parameter", param["type"])
                env[param["name"]] = identity
                binders.append(identity)
            annotation = self.expr(definition["type"], env)
            body = self.expr(definition["body"], env)
            body = self.gen("value.definition", [annotation, body], is_function=definition.get("is_function", bool(binders)), arity=len(binders))
            for identity, typ in reversed(list(zip(binders, annotations))):
                body = KernelNode.make("scope", regions=[typ, body], binder=identity, name_kind="value-parameter")
            regions.append(KernelNode.make("scope", regions=[body], binder=globals_[definition["name"]], name_kind="definition"))
            try:
                from .admission import check_composition
                profile = check_composition(sorted(family_sets[definition["name"]]), self.module["imports"])
            except ImportError:
                profile = dict(admitted=False, profile=None, dependencies=[], assumptions=[],
                               obligations=["composite profile checker unavailable"], adapter_witnesses=[])
            profiles.append({"definition": definition["name"], "families": sorted(family_sets[definition["name"]]), **profile})
            if _uses_cdc(definition["body"]):
                from .cdc import NUMERIC_PROFILE
                profiles[-1]["numeric_contract"] = NUMERIC_PROFILE
            if not profile.get("admitted"):
                self.obligations.append({"definition": definition["name"], "kind": "composition",
                                         "details": profile.get("obligations", [])})
        root = self.gen("value.module", regions, version=self.module["version"], imports=self.module["imports"],
                        exports=[dict(name=d["name"], binder=globals_[d["name"]]) for d in definitions])
        graph = root.to_data()
        usage_requirements = []
        for binding in self.bindings:
            if binding["role"] not in ("parameter", "let"):
                continue
            uses = self.wire_counts.get(binding["id"], 0)
            if uses == 1:
                continue
            annotation = self.binding_types.get(binding["id"])
            capability = "Copy" if uses > 1 else "Drop"
            copyable = _classical_type(annotation)
            requirement = dict(binder=binding["id"], capability=capability, occurrences=uses,
                               status="checked-classical-type-rule" if copyable else "requires-resource-checker",
                               witness="V.classical-structural/1" if copyable else None)
            usage_requirements.append(requirement)
            if not copyable:
                self.obligations.append({"kind": "resource-usage", **requirement})
        identity_input = {"elaboration": ELABORATION_VERSION, "graph": graph,
                          "profiles": profiles, "usage_requirements": usage_requirements,
                          "operators": sorted(self.operators.values(), key=lambda x: x["name"])}
        return dict(version=ELABORATION_VERSION, graph=graph, bindings=self.bindings,
                    operators=identity_input["operators"], profiles=profiles,
                    adapter_witnesses=[w for p in profiles for w in p.get("adapter_witnesses", [])],
                    obligations=self.obligations,
                    usage_requirements=usage_requirements,
                    structural_digest=_digest("u.structural/1", graph),
                    semantic_digest=_digest("u.semantic-candidate/1", identity_input),
                    status="elaborated", checked=False,
                    identity_scope="canonical structure and declared contracts; not general semantic equivalence")


def _walk(expr):
    yield expr
    for key, value in expr.items():
        if key == "span":
            continue
        if isinstance(value, dict):
            if "kind" in value:
                yield from _walk(value)
            else:
                for item in value.values():
                    if isinstance(item, dict):
                        yield from _walk(item)
        elif isinstance(value, list):
            for item in value:
                if isinstance(item, dict):
                    yield from _walk(item)


def _classical_type(annotation):
    if not annotation:
        return False
    name = qualified_name(annotation)
    if name in {"Int", "Nat", "Bool", "Text", "Unit", "F32", "F64", "U8", "U64", "I64", "Real", "Decimal", "Type"}:
        return True
    if annotation.get("kind") == "call":
        constructor = qualified_name(annotation["callee"])
        if constructor in {"List", "Pair"}:
            return all(_classical_type(arg) for arg in annotation["args"])
        if constructor == "Bits":
            return True
    return False


def _uses_cdc(expr):
    return any(node.get("kind") == "call" and (qualified_name(node["callee"]) or "").startswith("cdc.") for node in _walk(expr))


def _semantic_dependencies(expr, globals_, locals_):
    families, references = {"V"}, set()

    def visit(node, local):
        kind = node.get("kind")
        if kind == "name":
            if node["name"] in globals_ and node["name"] not in local:
                references.add(node["name"])
            return
        if kind == "call":
            name = qualified_name(node["callee"])
            if name and "." in name and name.split(".", 1)[0] not in globals_ | local:
                families.add(THEORIES.get(name.split(".", 1)[0], "V"))
        if kind == "lambda":
            visit(node["body"], local | {parameter["name"] for parameter in node["params"]})
            return
        if kind == "block":
            scope = set(local)
            for statement in node["statements"]:
                visit(statement["value"], scope)
                if statement["kind"] == "let":
                    scope.add(statement["name"])
            visit(node["result"], scope)
            return
        for key, value in node.items():
            if key == "span":
                continue
            if isinstance(value, dict):
                if "kind" in value:
                    visit(value, local)
                else:
                    for item in value.values():
                        if isinstance(item, dict):
                            visit(item, local)
            elif isinstance(value, list):
                for item in value:
                    if isinstance(item, dict):
                        visit(item, local)

    visit(expr, set(locals_))
    return families, references


def elaborate(module: dict) -> dict:
    """Lower every parsed expression, preserving unresolved semantic obligations."""
    try:
        return Elaborator(module).run()
    except RecursionError:
        raise ValueError("elaboration nesting budget exceeded") from None
