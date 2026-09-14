"""Conservative stage-0 type, scope, and resource checking.

Unknown semantic fragments produce obligations, never a successful universal
type or proof. Dependent conversion is reserved for the separate total kernel.
"""
from __future__ import annotations

from dataclasses import dataclass

from .source import qualified_name


@dataclass(frozen=True)
class Type:
    name: str
    args: tuple = ()

    def __str__(self):
        if self.name in ("Record", "Sum"):
            return self.name + "{" + ", ".join(f"{k}: {v}" for k, v in self.args) + "}"
        return self.name + ("(" + ", ".join(map(str, self.args)) + ")" if self.args else "")


UNKNOWN = Type("Unresolved")
TYPE, INT, NAT, BOOL, TEXT, UNIT = (Type(x) for x in ("Type", "Int", "Nat", "Bool", "Text", "Unit"))
BASE_TYPES = frozenset("Type Int Nat Bool Text Unit Real Decimal F32 F64 U8 U64 I64 Bit Clock ConsoleCap Http Response Substitution ActorSystemCap SupervisorRef FiniteModelReport ProofReceipt GpuThread CdcState CdcStepResult CdcNative ReciprocalBinding DecisionCoordinate U1Closure RecurrenceRequest PathTangent VerifiedRecurrence Spectrum PositiveReal RealTrajectory DAE IO Actors U1Closure ReturnMap U1Check".split())
CONSTRUCTORS = {
    "List": (1, 1), "Pair": (2, 2), "Fn": (1, 1000), "Owned": (1, 1),
    "Buffer": (2, 2), "MutView": (1, 1), "Task": (1, 1), "SharedCap": (1, 1),
    "Effect": (2, 2), "Code": (1, 1), "Rel": (1, 1000), "Logic": (1, 1),
    "Search": (1, 1), "Bag": (1, 1), "Array": (2, 2), "Measure": (1, 1),
    "ApproxPosterior": (1, 1), "ActorStep": (1, 1), "Option": (1, 1),
    "BehaviorSet": (1, 1), "Temporal": (1, 1), "Pi": (2, 2), "Eq": (3, 3),
    "Signal": (2, 2), "Bits": (1, 1), "GpuRead": (2, 2), "GpuOwn": (2, 2),
    "GpuOwnElement": (1, 1), "GpuTask": (1, 1), "QReg": (1, 1),
    "QuantumProgram": (1, 1), "ExecutedPath": (1, 1), "Analysis": (1, 1),
    "Partial": (1, 1), "Distribution": (1, 1),
}
LINEAR_NAMES = frozenset(("QReg", "Owned", "GpuOwn", "GpuOwnElement", "MutView"))
ADVANCED_OPERATORS = frozenset("""
http.get http.body_text async.then syntax.bind_once syntax.quote syntax.splice
logic.relation logic.exists logic.and logic.apply logic.query rel.rows
sql.inner_join sql.project sql.eq array.contract prob.model prob.sample
prob.observe_all prob.infer dist.normal actor.continue actor.fail actor.supervise
actor.child duration.seconds temporal.spec temporal.stutter temporal.weak_fair
temporal.always temporal.eventually temporal.and temporal.check model.closed_reachable_states
nat.induction eq.refl eq.congr proof.check hw.reg hw.at_tick
gpu.partition_launch gpu.write gpu.load gpu.read quantum.program quantum.measure_all
cdc.make_state cdc.flow cdc.commit cdc.nest cdc.u1.close cdc.check_reciprocal
topology.check_oriented_double_cover cdc.check_accepted_commits
cdc.check_generated_coordinate cdc.check_effect_binding analysis.then ad.executed_path
recurrence.check spectral.real_schur linear.return_map dynamics.dae dynamics.equal
dynamics.add dynamics.scale dynamics.derivative dynamics.at core.fix_partial
""".split())
CORE_OPERATORS = frozenset("""
int.add int.sub int.mul int.div int.mod int.eq int.lt int.le int.gt int.ge int.neg
nat.add nat.mul nat.sub nat.succ nat.pred nat.eq nat.lt nat.le nat.gt nat.ge nat.ceil_div nat.rec
bool.not bool.and bool.or text.concat text.eq text.length tuple.first tuple.second
list.map list.filter list.fold_left list.cons list.head_or list.tail_or_empty list.length
types.record types.sum sum.case partial.done option.when option.some option.none
value.select io.println bits.u8 bits.add_wrap f32.add f32.mul f64.add f64.mul
mem.with_mut mem.write mem.read quantum.zero quantum.h quantum.cx quantum.x
quantum.measure_all quantum.program
""".split())
KNOWN_OPERATORS = CORE_OPERATORS | ADVANCED_OPERATORS
NAMESPACES = frozenset(name.split(".")[0] for name in KNOWN_OPERATORS)


@dataclass
class Binding:
    type: Type
    uses: int = 0
    role: str = "value"


def is_linear(typ):
    if typ.name == "Fn":
        # A reusable function may consume a linear argument on each call.
        # Captures, rather than the function's argument types, control usage.
        return False
    if typ.name in LINEAR_NAMES:
        return True
    if typ.name in ("Record", "Sum"):
        return any(is_linear(item) for _, item in typ.args)
    return any(isinstance(item, Type) and is_linear(item) for item in typ.args)


def contains(typ, name):
    if typ.name == name:
        return True
    if typ.name in ("Record", "Sum"):
        return any(contains(item, name) for _, item in typ.args)
    return any(isinstance(item, Type) and contains(item, name) for item in typ.args)


class Checker:
    def __init__(self, module):
        self.module = module
        self.definitions = {definition["name"]: definition for definition in module["definitions"]}
        self.aliases, self.alias_stack = {}, set()
        self.diagnostics, self.obligations, self.results = [], [], []
        self.current = None
        self.steps = 0
        self.globals = {}

    def diagnostic(self, code, message, expr=None, severity="error"):
        item = dict(code=code, message=message, severity=severity, definition=self.current)
        if expr and "span" in expr:
            item["span"] = expr["span"]
        self.diagnostics.append(item)

    def unsupported(self, message, expr=None, code="unsupported_fragment"):
        item = dict(code=code, message=message, definition=self.current)
        if expr and "span" in expr:
            item["span"] = expr["span"]
        if item not in self.obligations:
            self.obligations.append(item)
        return UNKNOWN

    def step(self):
        self.steps += 1
        if self.steps > 250_000:
            raise ValueError("checker work budget exceeded")

    def annotation(self, expr, env):
        self.step()
        kind = expr["kind"]
        if kind == "name":
            name = expr["name"]
            if name in BASE_TYPES:
                return Type(name)
            if name in self.definitions and qualified_name(self.definitions[name]["type"]) == "Type":
                return self.alias(name, env)
            if name in env:
                return Type("Index", (name,))
            self.diagnostic("undefined_type", f"type name {name} is not declared", expr)
            return UNKNOWN
        if kind == "literal":
            value = expr["value"]
            if type(value) is int and value >= 0:
                return Type("Index", (value,))
            self.diagnostic("invalid_type_index", "type indices require a nonnegative integer or declared value", expr)
            return UNKNOWN
        if kind == "list":
            return Type("Shape", tuple(self.annotation(item, env) for item in expr["items"]))
        if kind == "call":
            name = qualified_name(expr["callee"])
            if name in CONSTRUCTORS:
                minimum, maximum = CONSTRUCTORS[name]
                if not minimum <= len(expr["args"]) <= maximum:
                    self.diagnostic("type_arity", f"{name} expects {minimum}..{maximum} parameters", expr)
                    return UNKNOWN
                if name in ("Pi", "Eq"):
                    self.validate_names(expr, env)
                    self.unsupported("dependent equality/product conversion requires the independent proof kernel", expr, "dependent_conversion")
                    return UNKNOWN
                arguments = tuple(self.annotation(item, env) for item in expr["args"])
                index_positions = {"Buffer": {1}, "Bits": {0}, "QReg": {0}, "GpuRead": {1}, "GpuOwn": {1}}
                for index, argument in enumerate(arguments):
                    node = expr["args"][index]
                    if index in index_positions.get(name, set()):
                        valid = argument.name == "Index"
                        if node["kind"] == "name" and node["name"] in env:
                            valid = env[node["name"]].type == NAT
                        if not valid:
                            self.diagnostic("type_index_kind", f"{name} index requires Nat", node)
                        if name in ("Bits", "QReg") and argument.name == "Index" and argument.args == (0,):
                            self.diagnostic("type_index_range", f"{name} width must be positive", node)
                    elif name == "Signal" and index == 1:
                        if node["kind"] != "name" or node["name"] not in env or env[node["name"]].type != Type("Clock"):
                            self.diagnostic("clock_index_kind", "Signal requires a declared Clock index", node)
                    elif name == "Array" and index == 1:
                        if argument.name != "Shape":
                            self.diagnostic("shape_kind", "Array shape requires an explicit list of Nat indices", node)
                        elif any(item.name != "Index" for item in argument.args):
                            self.diagnostic("shape_kind", "Array dimensions require Nat indices", node)
                    elif argument.name in ("Index", "Shape"):
                        self.diagnostic("type_parameter_kind", f"{name} parameter {index + 1} requires a type", node)
                return Type(name, arguments)
            self.validate_names(expr, env)
            return self.unsupported("computed type requires total normalization", expr, "computed_type")
        self.validate_names(expr, env)
        return self.unsupported("annotation is outside the decidable stage-0 type fragment", expr, "computed_type")

    def alias(self, name, env):
        if name in self.aliases:
            return self.aliases[name]
        if name in self.alias_stack:
            self.diagnostic("recursive_type_alias", f"unguarded recursive alias {name}")
            return UNKNOWN
        self.alias_stack.add(name)
        definition = self.definitions[name]
        expr = definition["body"]
        if expr["kind"] == "call" and qualified_name(expr["callee"]) in ("types.record", "types.sum") and len(expr["args"]) == 1 and expr["args"][0]["kind"] == "record":
            typ = Type("Record" if qualified_name(expr["callee"]) == "types.record" else "Sum",
                       tuple((field, self.annotation(value, env)) for field, value in expr["args"][0]["fields"].items()))
        else:
            typ = self.annotation(expr, env)
        self.aliases[name] = typ
        self.alias_stack.remove(name)
        return typ

    def validate_names(self, expr, env):
        """Scope-check unsupported/dependent syntax without evaluating it."""
        self.step()
        kind = expr.get("kind")
        if kind == "name":
            name = expr["name"]
            if name not in env and name not in self.definitions and name not in BASE_TYPES and name not in CONSTRUCTORS and name not in NAMESPACES:
                self.diagnostic("undefined_name", f"name {name} is not in scope", expr)
        elif kind == "lambda":
            local = dict(env)
            for param in expr["params"]:
                self.validate_names(param["type"], local)
                local[param["name"]] = Binding(UNKNOWN)
            self.validate_names(expr["body"], local)
        elif kind == "block":
            local = dict(env)
            for statement in expr["statements"]:
                self.validate_names(statement["value"], local)
                if statement["kind"] == "let":
                    local[statement["name"]] = Binding(UNKNOWN)
            self.validate_names(expr["result"], local)
        else:
            for key, value in expr.items():
                if key == "span":
                    continue
                if isinstance(value, dict):
                    if "kind" in value:
                        self.validate_names(value, env)
                    else:
                        for item in value.values():
                            if isinstance(item, dict):
                                self.validate_names(item, env)
                elif isinstance(value, list):
                    for item in value:
                        if isinstance(item, dict):
                            self.validate_names(item, env)

    def compatible(self, actual, expected):
        if actual == expected:
            return True
        if UNKNOWN in (actual, expected):
            return None
        if actual.name == expected.name and len(actual.args) == len(expected.args):
            if actual.name in ("Record", "Sum"):
                a, b = dict(actual.args), dict(expected.args)
                if set(a) != set(b):
                    return False
                answers = [self.compatible(a[key], b[key]) for key in a]
            else:
                answers = [self.compatible(a, b) if isinstance(a, Type) and isinstance(b, Type) else a == b
                           for a, b in zip(actual.args, expected.args)]
            if False in answers:
                return False
            return None if None in answers else True
        # An explicitly expected effect retains its effect boundary; its body
        # must actually produce Effect, so pure values do not silently acquire it.
        return False

    def require(self, actual, expected, expr):
        if expected is None:
            return actual
        compatible = self.compatible(actual, expected)
        if compatible is False:
            self.diagnostic("type_mismatch", f"expected {expected}, found {actual}", expr)
        elif compatible is None:
            self.unsupported(f"cannot establish {actual} : {expected}", expr, "unresolved_type")
        return actual

    def use(self, name, env, expr):
        if name not in env:
            if name in BASE_TYPES or name in CONSTRUCTORS:
                return TYPE
            self.diagnostic("undefined_name", f"name {name} is not in scope", expr)
            return UNKNOWN
        binding = env[name]
        binding.uses += 1
        if is_linear(binding.type) and binding.uses > 1:
            self.diagnostic("linear_reuse", f"linear resource {name} is consumed more than once", expr)
        return binding.type

    def finish_bindings(self, bindings, expr):
        for name, binding in bindings.items():
            if is_linear(binding.type) and binding.uses == 0:
                self.diagnostic("linear_discard", f"linear resource {name} is not consumed or returned", expr)

    def infer(self, expr, env, expected=None):
        self.step()
        kind = expr["kind"]
        if kind == "literal":
            value = expr["value"]
            actual = BOOL if type(value) is bool else INT if type(value) is int else Type("F64") if type(value) is float else TEXT
            if type(value) is int and expected is not None:
                if expected.name == "Nat" and value >= 0:
                    actual = NAT
                elif expected.name in ("U8", "U64", "I64"):
                    low, high = (-(1 << 63), 1 << 63) if expected.name == "I64" else (0, 1 << (8 if expected.name == "U8" else 64))
                    if low <= value < high:
                        actual = expected
                    else:
                        self.diagnostic("integer_range", f"literal is outside {expected}", expr)
                elif expected.name in ("F32", "F64", "Real", "PositiveReal", "Decimal"):
                    if expected.name == "PositiveReal" and value <= 0:
                        self.diagnostic("positive_real", "positive value required", expr)
                    actual = expected
            if type(value) is float and expected is not None and expected.name in ("F32", "F64", "Real", "PositiveReal"):
                if expected.name == "PositiveReal" and value <= 0:
                    self.diagnostic("positive_real", "positive value required", expr)
                actual = expected
            return self.require(actual, expected, expr)
        if kind == "name":
            return self.require(self.use(expr["name"], env, expr), expected, expr)
        if kind == "member":
            name = qualified_name(expr)
            if name and name.split(".", 1)[0] not in env:
                if name not in KNOWN_OPERATORS:
                    self.diagnostic("undefined_operator", f"operator {name} has no declared stage-0 contract", expr)
                    return UNKNOWN
                typ = self.operator_value(name, expr)
                return self.require(typ, expected, expr)
            obj = self.infer(expr["object"], env)
            field = expr["name"]
            if obj.name in ("Pair", "Tuple") and field in ("first", "second"):
                index = 0 if field == "first" else 1
                typ = obj.args[index] if index < len(obj.args) else UNKNOWN
                if typ == UNKNOWN:
                    self.diagnostic("no_member", f"{obj} has no {field}", expr)
            elif obj.name == "Record" and field in dict(obj.args):
                typ = dict(obj.args)[field]
            elif obj.name == "CdcStepResult" and field == "state":
                typ = Type("CdcState")
            elif obj.name == "GpuThread" and field == "global_x":
                typ = NAT
            elif obj == UNKNOWN:
                typ = self.unsupported(f"cannot resolve member {field} of unresolved value", expr)
            else:
                self.diagnostic("no_member", f"{obj} has no declared member {field}", expr)
                typ = UNKNOWN
            return self.require(typ, expected, expr)
        if kind == "lambda":
            local = {name: Binding(binding.type, binding.uses, binding.role) for name, binding in env.items()}
            params = {}
            expected_params = expected.args[:-1] if expected and expected.name == "Fn" else ()
            for index, param in enumerate(expr["params"]):
                typ = self.annotation(param["type"], local)
                if index < len(expected_params):
                    self.require(typ, expected_params[index], param)
                binding = Binding(typ, role="parameter")
                local[param["name"]] = params[param["name"]] = binding
            result_expected = expected.args[-1] if expected and expected.name == "Fn" else None
            result = self.infer(expr["body"], local, result_expected)
            self.finish_bindings(params, expr)
            for name, binding in env.items():
                if name not in params and is_linear(binding.type) and local[name].uses > binding.uses:
                    self.diagnostic("linear_capture", f"closure captures linear resource {name}; a checked one-shot closure is required", expr)
            typ = Type("Fn", tuple(binding.type for binding in params.values()) + (result,))
            return self.require(typ, expected, expr)
        if kind in ("list", "tuple"):
            items = expr["items"]
            if kind == "list":
                element = expected.args[0] if expected and expected.name == "List" else None
                types = []
                for item in items:
                    typ = self.infer(item, env, element)
                    types.append(typ)
                    if element is None:
                        element = typ
                if element is None:
                    element = self.unsupported("empty list needs an element type from context", expr)
                typ = Type("List", (element,))
            else:
                targets = expected.args if expected and expected.name in ("Pair", "Tuple") else ()
                types = [self.infer(item, env, targets[index] if index < len(targets) else None) for index, item in enumerate(items)]
                typ = UNIT if not types else Type("Pair" if len(types) == 2 else "Tuple", tuple(types))
            if any(contains(item, "Effect") and item.name != "Fn" for item in types):
                self.unsupported("effectful element evaluation requires an explicit sequencing derivation", expr, "nested_effect")
            return self.require(typ, expected, expr)
        if kind == "record":
            fields = dict(expected.args) if expected and expected.name == "Record" else {}
            typ = Type("Record", tuple((name, self.infer(value, env, fields.get(name))) for name, value in expr["fields"].items()))
            if any(contains(value, "Effect") and value.name != "Fn" for _, value in typ.args):
                self.unsupported("effectful field evaluation requires an explicit sequencing derivation", expr, "nested_effect")
            return self.require(typ, expected, expr)
        if kind == "block":
            local, bound = dict(env), {}
            effects = set()
            for statement in expr["statements"]:
                value = self.infer(statement["value"], local)
                if value.name == "Effect" and len(value.args) == 2:
                    effects.add(value.args[0])
                    value = value.args[1]
                if statement["kind"] == "let":
                    binding = Binding(value)
                    local[statement["name"]] = bound[statement["name"]] = binding
                elif is_linear(value):
                    self.diagnostic("linear_discard", "statement discards a linear result", statement)
            result_expected = expected.args[1] if effects and expected and expected.name == "Effect" else expected
            typ = self.infer(expr["result"], local, result_expected)
            if effects:
                if expected and expected.name == "Effect":
                    if effects != {expected.args[0]}:
                        self.unsupported("block combines effects without an admitted effect-composition derivation", expr, "effect_composition")
                    typ = Type("Effect", (expected.args[0], typ))
                else:
                    self.diagnostic("effect_escape", "block performs an effect omitted from its result annotation", expr)
            self.finish_bindings(bound, expr)
            return typ
        if kind == "call":
            name = qualified_name(expr["callee"])
            if name and "." in name and name.split(".", 1)[0] not in env:
                return self.require(self.call_operator(name, expr["args"], env, expr, expected), expected, expr)
            if name in CONSTRUCTORS and name not in env:
                self.annotation(expr, env)
                return self.require(TYPE, expected, expr)
            callee = self.infer(expr["callee"], env)
            if callee.name == "Fn":
                params, result = callee.args[:-1], callee.args[-1]
                if len(params) != len(expr["args"]):
                    self.diagnostic("arity", f"expected {len(params)} arguments, found {len(expr['args'])}", expr)
                for index, arg in enumerate(expr["args"]):
                    self.infer(arg, env, params[index] if index < len(params) else None)
                return self.require(result, expected, expr)
            for arg in expr["args"]:
                self.infer(arg, env)
            if callee == UNKNOWN:
                return self.unsupported("application target has unresolved type", expr)
            self.diagnostic("not_callable", f"{callee} is not a function", expr)
            return UNKNOWN
        self.diagnostic("invalid_ast", f"unsupported AST node {kind}", expr)
        return UNKNOWN

    def arity(self, name, args, count, expr):
        if len(args) != count:
            self.diagnostic("arity", f"{name} expects {count} arguments, found {len(args)}", expr)
            return False
        return True

    def fixed(self, name, args, env, expr, params, result):
        self.arity(name, args, len(params), expr)
        for index, arg in enumerate(args):
            self.infer(arg, env, params[index] if index < len(params) else None)
        return result

    def operator_value(self, name, expr):
        numeric = name.split(".")
        if len(numeric) == 2 and numeric[0] in ("int", "nat", "f32", "f64"):
            typ = {"int": INT, "nat": NAT, "f32": Type("F32"), "f64": Type("F64")}[numeric[0]]
            unary = numeric[1] in ("succ", "pred", "neg")
            result = BOOL if numeric[1] in ("eq", "lt", "le", "gt", "ge") else typ
            if name == "nat.rec":
                return self.unsupported("polymorphic nat.rec needs application context", expr)
            return Type("Fn", (typ,) * (1 if unary else 2) + (result,))
        return self.unsupported(f"first-class operator {name} requires explicit specialization", expr)

    def call_operator(self, name, args, env, expr, expected):
        if name not in KNOWN_OPERATORS:
            self.diagnostic("undefined_operator", f"operator {name} has no declared stage-0 contract", expr)
            for arg in args:
                self.infer(arg, env)
            return UNKNOWN
        namespace, operation = name.split(".", 1)
        if namespace in ("int", "nat", "f32", "f64") and name not in ("nat.rec", "nat.induction"):
            signature = self.operator_value(name, expr)
            return self.fixed(name, args, env, expr, signature.args[:-1], signature.args[-1])
        signatures = {
            "bool.not": ([BOOL], BOOL), "bool.and": ([BOOL, BOOL], BOOL), "bool.or": ([BOOL, BOOL], BOOL),
            "text.concat": ([TEXT, TEXT], TEXT), "text.eq": ([TEXT, TEXT], BOOL), "text.length": ([TEXT], NAT),
            "io.println": ([Type("ConsoleCap"), TEXT], Type("Effect", (Type("IO"), UNIT))),
            "http.get": ([Type("SharedCap", (Type("Http"),)), TEXT], Type("Task", (Type("Response"),))),
            "http.body_text": ([Type("Response")], TEXT),
            "dist.normal": ([Type("Real"), Type("PositiveReal")], Type("Distribution", (Type("Real"),))),
        }
        if name in signatures:
            params, result = signatures[name]
            if namespace in ("http", "dist"):
                self.unsupported(f"{name} interface checked; authority/domain obligations require its runtime contract", expr, "domain_obligation")
            return self.fixed(name, args, env, expr, params, result)
        if name in ("types.record", "types.sum"):
            if self.arity(name, args, 1, expr) and args[0]["kind"] == "record":
                for value in args[0]["fields"].values():
                    self.annotation(value, env)
            else:
                self.diagnostic("type_description", "record/sum type requires a record of type expressions", expr)
            return TYPE
        if name in ("tuple.first", "tuple.second"):
            if not self.arity(name, args, 1, expr):
                return UNKNOWN
            value = self.infer(args[0], env)
            index = 0 if name.endswith("first") else 1
            if value.name != "Pair":
                self.diagnostic("type_mismatch", "pair projection requires Pair", expr)
                return UNKNOWN
            return value.args[index]
        if name == "nat.rec":
            if not self.arity(name, args, 3, expr):
                return UNKNOWN
            self.infer(args[0], env, NAT)
            target = expected
            if args[2]["kind"] == "lambda" and len(args[2]["params"]) == 2:
                target = self.annotation(args[2]["params"][1]["type"], env)
            seed = self.infer(args[1], env, target)
            if is_linear(seed):
                self.unsupported("linear structural recursion requires a specialized usage derivation", expr)
            self.infer(args[2], env, Type("Fn", (NAT, seed, seed)))
            return seed
        if name in ("list.map", "list.filter", "list.fold_left"):
            count = 3 if name == "list.fold_left" else 2
            if not self.arity(name, args, count, expr):
                return UNKNOWN
            input_type = None
            callback = args[-1]
            if callback["kind"] == "lambda" and callback["params"]:
                input_type = self.annotation(callback["params"][-1]["type"], env)
            xs = self.infer(args[0], env, Type("List", (input_type,)) if input_type else None)
            if xs.name != "List":
                self.diagnostic("type_mismatch", f"{name} requires a list", args[0])
                return UNKNOWN
            element = xs.args[0]
            if is_linear(element):
                self.unsupported("linear list combinators require a specialized resource derivation", expr)
            if name == "list.fold_left":
                seed_target = self.annotation(callback["params"][0]["type"], env) if callback["kind"] == "lambda" and len(callback["params"]) == 2 else expected
                seed = self.infer(args[1], env, seed_target)
                self.infer(callback, env, Type("Fn", (seed, element, seed)))
                return seed
            if name == "list.filter":
                self.infer(callback, env, Type("Fn", (element, BOOL)))
                return xs
            result_target = expected.args[0] if expected and expected.name == "List" else None
            if result_target:
                function = self.infer(callback, env, Type("Fn", (element, result_target)))
            else:
                function = self.infer(callback, env)
            if function.name != "Fn" or len(function.args) != 2:
                self.diagnostic("type_mismatch", "map callback requires one argument", callback)
                return UNKNOWN
            self.require(function.args[0], element, callback)
            return Type("List", (function.args[-1],))
        if name in ("list.cons", "list.head_or", "list.tail_or_empty", "list.length"):
            if not self.arity(name, args, 2 if name in ("list.cons", "list.head_or") else 1, expr):
                return UNKNOWN
            list_arg = args[1] if name == "list.cons" else args[0]
            list_expected = expected if expected and expected.name == "List" else None
            xs = self.infer(list_arg, env, list_expected)
            if xs.name != "List":
                self.diagnostic("type_mismatch", "list operation requires List", list_arg)
                return UNKNOWN
            if name == "list.cons":
                self.infer(args[0], env, xs.args[0])
            if name == "list.head_or":
                self.infer(args[1], env, xs.args[0])
                return xs.args[0]
            return NAT if name == "list.length" else xs
        if name == "value.select":
            if not self.arity(name, args, 3, expr):
                return UNKNOWN
            self.infer(args[0], env, BOOL)
            left = self.infer(args[1], env, expected)
            right = self.infer(args[2], env, left)
            if is_linear(left) or is_linear(right):
                self.unsupported("linear branch selection requires an explicit branch usage proof", expr)
            return left
        if name == "bits.u8":
            if not self.arity(name, args, 1, expr):
                return UNKNOWN
            self.infer(args[0], env, Type("U8"))
            # U8 and Bits(8) are intentionally distinct in source. The explicit
            # bits.u8 constructor returns the fixed-width bit-vector carrier.
            return Type("Bits", (Type("Index", (8,)),))
        if name == "bits.add_wrap":
            if not self.arity(name, args, 2, expr):
                return UNKNOWN
            left = self.infer(args[0], env)
            if left.name != "Bits":
                self.diagnostic("type_mismatch", "wrapping bit addition requires Bits", args[0])
            self.infer(args[1], env, left)
            return left
        if name == "mem.with_mut":
            if not self.arity(name, args, 2, expr):
                return UNKNOWN
            owner = self.infer(args[0], env)
            if owner.name != "Owned":
                self.diagnostic("type_mismatch", "with_mut requires an Owned resource", args[0])
                return UNKNOWN
            view = Type("MutView", owner.args)
            function = self.infer(args[1], env, Type("Fn", (view, view)))
            if function.name == "Fn" and function.args[-1] != view:
                self.diagnostic("lifetime_escape", "mutable region must return its view directly to with_mut; borrowed values cannot escape", args[1])
            return owner
        if name in ("mem.write", "mem.read"):
            if not self.arity(name, args, 3 if name == "mem.write" else 2, expr):
                return UNKNOWN
            view = self.infer(args[0], env)
            self.infer(args[1], env, NAT)
            if view.name != "MutView" or not view.args or view.args[0].name != "Buffer":
                self.diagnostic("type_mismatch", "memory operation requires MutView(Buffer(...))", args[0])
                return UNKNOWN
            element = view.args[0].args[0]
            if name == "mem.write":
                # Buffer(U8, n) explicitly accepts the equivalent eight-bit
                # store representation produced by bits.u8, through this op.
                value_type = self.infer(args[2], env)
                bits8 = Type("Bits", (Type("Index", (8,)),))
                if not (element == Type("U8") and value_type == bits8):
                    self.require(value_type, element, args[2])
                return view
            self.unsupported("mem.read consumes a borrow in this first linear checker; threaded borrow result is required", expr)
            return element
        if name == "quantum.zero":
            if not self.arity(name, args, 1, expr):
                return UNKNOWN
            self.infer(args[0], env, NAT)
            if args[0]["kind"] == "literal" and type(args[0]["value"]) is int and args[0]["value"] > 0:
                return Type("QReg", (Type("Index", (args[0]["value"],)),))
            return self.unsupported("quantum width requires a positive static index", expr)
        if name in ("quantum.h", "quantum.x", "quantum.cx", "quantum.measure_all"):
            count = 3 if name == "quantum.cx" else 1 if name == "quantum.measure_all" else 2
            if not self.arity(name, args, count, expr):
                return UNKNOWN
            register = self.infer(args[0], env)
            if register.name != "QReg":
                self.diagnostic("type_mismatch", "quantum operation requires one joint QReg", args[0])
                return UNKNOWN
            width = register.args[0].args[0] if register.args and register.args[0].name == "Index" else None
            indices = []
            for arg in args[1:]:
                self.infer(arg, env, NAT)
                if arg["kind"] == "literal" and type(arg["value"]) is int:
                    index = arg["value"]
                    if type(width) is int and not 0 <= index < width:
                        self.diagnostic("quantum_index", "qubit index is outside the joint register", arg)
                    indices.append(index)
                else:
                    self.unsupported("dynamic qubit index requires a bounds obligation", arg)
            if name == "quantum.cx" and len(indices) == 2 and indices[0] == indices[1]:
                self.diagnostic("quantum_alias", "control and target must differ", expr)
            return Type("Bits", register.args) if name == "quantum.measure_all" else register
        if name == "quantum.program":
            if not self.arity(name, args, 1, expr):
                return UNKNOWN
            target = expected.args[0] if expected and expected.name == "QuantumProgram" else None
            fn = self.infer(args[0], env, Type("Fn", (target,)) if target else None)
            if fn.name != "Fn" or len(fn.args) != 1:
                self.diagnostic("type_mismatch", "quantum.program requires a zero-argument region", expr)
                return UNKNOWN
            return Type("QuantumProgram", (fn.args[-1],))
        if name in ("partial.done", "option.some"):
            if not self.arity(name, args, 1, expr):
                return UNKNOWN
            wrapper = "Partial" if name == "partial.done" else "Option"
            target = expected.args[0] if expected and expected.name == wrapper else None
            return Type(wrapper, (self.infer(args[0], env, target),))
        if name == "option.when":
            if not self.arity(name, args, 2, expr):
                return UNKNOWN
            self.infer(args[0], env, BOOL)
            fn = self.infer(args[1], env)
            if fn.name != "Fn" or len(fn.args) != 1:
                self.diagnostic("type_mismatch", "option.when needs a zero-argument thunk", expr)
                return UNKNOWN
            return Type("Option", (fn.args[-1],))
        if name == "sum.case":
            if not self.arity(name, args, 2, expr):
                return UNKNOWN
            value = self.infer(args[0], env)
            if value.name != "Sum" or args[1]["kind"] != "record":
                for arg in args[1:]:
                    self.infer(arg, env)
                self.diagnostic("type_mismatch", "sum.case needs a declared sum and explicit branch record", expr)
                return UNKNOWN
            alternatives, branches = dict(value.args), args[1]["fields"]
            if set(alternatives) != set(branches):
                self.diagnostic("nonexhaustive_sum", "case branches must exactly cover sum alternatives", args[1])
            result = expected
            for tag, branch in branches.items():
                if tag not in alternatives:
                    self.infer(branch, env)
                    continue
                fn = self.infer(branch, env, Type("Fn", (alternatives[tag], result)) if result else None)
                if fn.name != "Fn" or len(fn.args) != 2:
                    self.diagnostic("type_mismatch", "sum branch requires one argument", branch)
                else:
                    self.require(fn.args[0], alternatives[tag], branch)
                    if result is None:
                        result = fn.args[-1]
            return result or UNKNOWN
        if name == "core.fix_partial":
            if not self.arity(name, args, 2, expr):
                return UNKNOWN
            target = expected.args[0] if expected and expected.name == "Partial" else None
            initial = self.infer(args[1], env, target)
            partial = Type("Partial", (initial,))
            self.infer(args[0], env, Type("Fn", (Type("Fn", (initial, partial)), initial, partial)))
            return partial
        return self.advanced(name, args, env, expr, expected)

    def advanced(self, name, args, env, expr, expected):
        """Inspect all arguments and preserve the missing domain derivation."""
        self.unsupported(f"{name} has no complete stage-0 static domain derivation", expr, "domain_obligation")
        if name == "gpu.read" and len(args) == 1:
            # A read borrows the per-lane owner until the enclosing write.
            argument = args[0]
            if argument["kind"] == "name" and argument["name"] in env:
                binding = env[argument["name"]]
                if binding.uses:
                    self.diagnostic("linear_reuse", "read follows transfer of the owned lane", argument)
                if binding.type.name == "GpuOwnElement":
                    return binding.type.args[0]
            self.diagnostic("type_mismatch", "gpu.read requires a live named per-lane owner", expr)
            return UNKNOWN
        if name == "gpu.write" and len(args) == 2:
            # The value's borrow ends before the write transfers the handle.
            owner_type = env[args[0]["name"]].type if args[0]["kind"] == "name" and args[0]["name"] in env else None
            target = owner_type.args[0] if owner_type and owner_type.name == "GpuOwnElement" else None
            self.infer(args[1], env, target)
            owner = self.infer(args[0], env)
            if owner.name != "GpuOwnElement":
                self.diagnostic("type_mismatch", "gpu.write requires per-lane ownership", args[0])
            return owner
        if name == "gpu.load" and len(args) == 2:
            view = self.infer(args[0], env)
            self.infer(args[1], env, NAT)
            if view.name != "GpuRead":
                self.diagnostic("type_mismatch", "gpu.load requires GpuRead", args[0])
                return UNKNOWN
            return view.args[0]
        if name == "gpu.partition_launch" and len(args) == 3:
            self.infer(args[0], env)
            owner = self.infer(args[1], env)
            if owner.name != "GpuOwn":
                self.diagnostic("type_mismatch", "partition launch requires GpuOwn", args[1])
                self.infer(args[2], env)
                return UNKNOWN
            lane = Type("GpuOwnElement", (owner.args[0],))
            self.infer(args[2], env, Type("Fn", (Type("GpuThread"), lane, lane)))
            return Type("GpuTask", (owner,))
        # Actual declared boundary signatures prevent common wrong calls even
        # when stronger laws still await a dedicated semantic checker.
        signatures = {
            "cdc.flow": ([Type("CdcState"), TEXT, Type("F64")], Type("CdcStepResult")),
            "cdc.commit": ([Type("CdcState"), TEXT], Type("CdcStepResult")),
            "cdc.nest": ([Type("CdcState"), TEXT, TEXT], Type("CdcStepResult")),
            "proof.check": ([None], Type("ProofReceipt")),
            "duration.seconds": ([NAT], Type("Duration")),
            "actor.continue": ([NAT], Type("ActorStep", (NAT,))),
            "actor.fail": ([TEXT], expected if expected and expected.name == "ActorStep" else UNKNOWN),
            "dynamics.derivative": ([Type("RealTrajectory")], Type("RealTrajectory")),
            "dynamics.add": ([Type("RealTrajectory"), Type("RealTrajectory")], Type("RealTrajectory")),
            "dynamics.scale": ([Type("PositiveReal"), Type("RealTrajectory")], Type("RealTrajectory")),
            "dynamics.equal": ([Type("RealTrajectory"), Type("RealTrajectory")], Type("Equation")),
            "dynamics.at": ([Type("RealTrajectory"), Type("Real"), Type("Real")], Type("InitialCondition")),
        }
        if name in signatures:
            params, result = signatures[name]
            return self.fixed(name, args, env, expr, params, result)
        if name in ("syntax.quote", "syntax.splice", "syntax.bind_once"):
            # Quoted terms are scoped and typed but stage availability remains
            # an explicit outstanding obligation in this bootstrap checker.
            if name == "syntax.splice" and len(args) == 1:
                code = self.infer(args[0], env)
                if code.name == "Code":
                    return code.args[0]
                self.diagnostic("type_mismatch", "splice requires Code", expr)
                return UNKNOWN
            if name == "syntax.quote" and len(args) == 1:
                fn = self.infer(args[0], env)
                return Type("Code", (fn.args[-1],)) if fn.name == "Fn" else UNKNOWN
            if name == "syntax.bind_once" and len(args) == 2:
                code = self.infer(args[0], env)
                fn = self.infer(args[1], env, Type("Fn", (code, expected or code)))
                return fn.args[-1] if fn.name == "Fn" else UNKNOWN
        if name == "async.then" and len(args) == 2:
            task = self.infer(args[0], env)
            if task.name != "Task":
                self.diagnostic("type_mismatch", "async.then requires Task", args[0])
                return UNKNOWN
            target = expected.args[0] if expected and expected.name == "Task" else UNKNOWN
            fn = self.infer(args[1], env, Type("Fn", (task.args[0], target)))
            return Type("Task", (fn.args[-1],)) if fn.name == "Fn" else UNKNOWN
        if name == "prob.sample" and len(args) == 1:
            dist = self.infer(args[0], env)
            return dist.args[0] if dist.name == "Distribution" else UNKNOWN
        if name == "prob.model" and len(args) == 1:
            fn = self.infer(args[0], env)
            return Type("Measure", (fn.args[-1],)) if fn.name == "Fn" else UNKNOWN
        if name == "cdc.make_state" and len(args) == 1:
            # Lists of heterogeneous record schemas and empty channel vectors
            # need CDC's descriptor, so only names are checked at this boundary.
            self.validate_names(args[0], env)
            return Type("CdcState")
        if name == "array.contract" and len(args) == 5:
            values = [self.infer(arg, env) for arg in args]
            if values[0].name != "Array" or values[1].name != "Array":
                self.diagnostic("type_mismatch", "array.contract requires array operands", expr)
            return UNKNOWN
        if name in ("nat.induction", "eq.refl", "eq.congr"):
            self.validate_names(expr, env)
            return UNKNOWN
        for arg in args:
            self.infer(arg, env)
        return UNKNOWN

    def run(self):
        # Predeclare signatures, allowing mutual references without evaluating
        # definitions. General recursive calls remain a partiality obligation.
        for definition in self.module["definitions"]:
            self.current = definition["name"]
            local, params = {}, []
            for param in definition["params"]:
                typ = self.annotation(param["type"], local)
                local[param["name"]] = Binding(typ)
                params.append(typ)
            result = self.annotation(definition["type"], local)
            typ = Type("Fn", tuple(params) + (result,)) if definition.get("is_function", bool(params)) else result
            self.globals[definition["name"]] = Binding(typ, role="definition")
            if not definition.get("is_function", bool(params)) and is_linear(result):
                self.diagnostic("linear_global", "cached module values cannot own linear resources; use an explicit resource-producing function", definition)
        dependencies = {
            definition["name"]: {
                name for name in self.definitions
                if name not in {parameter["name"] for parameter in definition["params"]}
                and _references(definition["body"], name)
            }
            for definition in self.module["definitions"]
        }
        recursive = _cyclic_definitions(dependencies)
        for definition in self.module["definitions"]:
            self.current = definition["name"]
            local = {name: Binding(binding.type, role="definition") for name, binding in self.globals.items()}
            params = {}
            for param in definition["params"]:
                binding = Binding(self.annotation(param["type"], local), role="parameter")
                local[param["name"]] = params[param["name"]] = binding
            expected = self.annotation(definition["type"], local)
            actual = self.infer(definition["body"], local, expected)
            self.finish_bindings(params, definition)
            if contains(actual, "MutView"):
                self.diagnostic("lifetime_escape", "definition returns a mutable view beyond its region", definition)
            if definition["name"] in recursive:
                self.unsupported("recursive definitions require an explicit partial or well-founded iteration derivation", definition, "unguarded_recursion")
            errors = [d for d in self.diagnostics if d["definition"] == self.current and d["severity"] == "error"]
            obligations = [d for d in self.obligations if d["definition"] == self.current]
            status = "rejected" if errors else "unsupported" if obligations else "checked"
            self.results.append(dict(name=self.current, status=status, declared_type=str(expected), inferred_type=str(actual)))
        # A definition calling an unchecked definition does not inherit trust
        # solely from that callee's claimed return annotation.
        result_map = {item["name"]: item for item in self.results}
        for _ in range(len(self.results)):
            for definition in self.module["definitions"]:
                item = result_map[definition["name"]]
                if item["status"] != "checked":
                    continue
                unchecked = [name for name in dependencies[definition["name"]] if result_map[name]["status"] != "checked"]
                if unchecked:
                    item["status"] = "unsupported"
                    self.current = definition["name"]
                    self.unsupported("depends on unchecked definitions: " + ", ".join(sorted(unchecked)), definition, "unchecked_dependency")
        self.current = None
        errors = any(item["severity"] == "error" for item in self.diagnostics)
        status = "rejected" if errors else "unsupported" if self.obligations else "checked"
        return dict(status=status, parsed=True, typechecked=status == "checked", ok=status == "checked",
                    checker="etellis.u/stage0-checker-1", definitions=self.results,
                    diagnostics=self.diagnostics, obligations=self.obligations,
                    trust_dependencies=["python-stage0", "u.checker"],
                    scope="structural simple types, declared boundary signatures, conservative lexical linear usage; domain/proof obligations remain explicit")


def _references(expr, name):
    if expr.get("kind") == "name":
        return expr["name"] == name
    if expr.get("kind") == "lambda" and name in {p["name"] for p in expr["params"]}:
        return False
    if expr.get("kind") == "block":
        for statement in expr["statements"]:
            if _references(statement["value"], name):
                return True
            if statement["kind"] == "let" and statement["name"] == name:
                return False
        return _references(expr["result"], name)
    for key, value in expr.items():
        if key == "span":
            continue
        if isinstance(value, dict):
            if "kind" in value and _references(value, name):
                return True
            if "kind" not in value and any(_references(x, name) for x in value.values() if isinstance(x, dict)):
                return True
        if isinstance(value, list) and any(_references(x, name) for x in value if isinstance(x, dict)):
            return True
    return False


def _cyclic_definitions(edges):
    """Iterative path search: conservatively mark every member of a call cycle."""
    cyclic = set()
    for origin in edges:
        pending, seen = list(edges[origin]), set()
        while pending:
            name = pending.pop()
            if name == origin:
                cyclic.add(origin)
                break
            if name in seen:
                continue
            seen.add(name)
            pending.extend(edges.get(name, ()))
    return cyclic


def check(module: dict) -> dict:
    try:
        return Checker(module).run()
    except (RecursionError, ValueError) as error:
        return dict(status="rejected", parsed=True, typechecked=False, ok=False,
                    diagnostics=[dict(code="checker_limit", severity="error", message=str(error) or "checker nesting limit")],
                    obligations=[], definitions=[])
