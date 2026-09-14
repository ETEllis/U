"""Direct, fuel-accounted evaluation of U's parsed expression language.

No U source or embedded foreign source is passed to host eval/exec. Python is
the declared stage-0 implementation language, not U's semantic source language.
"""
from dataclasses import dataclass
import importlib
import math
import struct
import threading

from .theories.base import (RuntimeFault, UnsupportedOperation, TypeSymbol,
                            Capability, Sum, Partial, Option, OwnedBuffer,
                            MutView, Bits, natural, integer, boolean, index)


@dataclass
class Closure:
    params: list
    body: dict
    env: dict
    evaluator: object


@dataclass(frozen=True)
class Native:
    name: str
    function: object


@dataclass(frozen=True)
class Namespace:
    name: str


@dataclass
class Definition:
    definition: dict
    evaluated: bool = False
    evaluating: bool = False
    value: object = None


@dataclass(frozen=True)
class Code:
    expression: dict
    environment: dict
    stage: int = 1


class _PartialContinue(BaseException):
    def __init__(self, value):
        self.value = value


class Evaluator:
    def __init__(self, module, *, capabilities=(), budget=100000):
        if type(budget) is not int or budget <= 0:
            raise RuntimeFault("INVALID_BUDGET", "Evaluation requires positive integer fuel")
        self.module, self.initial_budget, self.remaining = module, budget, budget
        self.registry, self.globals = {}, {}
        self._authority = object()
        self.capabilities = {name: Capability(name, self._authority) for name in capabilities}
        self.active_borrows = 0
        self._fresh_id = 0
        self._quote_depth = 0
        self._calls = threading.local()
        self._fuel_lock = threading.Lock()
        self._register_core()
        for module_name in ("relational", "numeric", "processes"):
            importlib.import_module(f"u.theories.{module_name}").register(self)
        for module_name in ("cdc", "proof"):
            try:
                extension = importlib.import_module(f"u.{module_name}")
            except ModuleNotFoundError as error:
                if error.name != f"u.{module_name}":
                    raise
            else:
                extension.register(self)
        for definition in module.get("definitions", []):
            name = definition["name"]
            if name in self.globals:
                raise RuntimeFault("DUPLICATE_DEFINITION", "Repeated definition", {"name": name})
            self.globals[name] = Definition(definition)

    def register(self, name, function):
        if name in self.registry:
            raise RuntimeFault("DUPLICATE_OPERATOR", "Operator is already registered", {"name": name})
        self.registry[name] = function

    def supported_operators(self):
        return sorted(self.registry)

    def tick(self, amount=1):
        with self._fuel_lock:
            self.remaining -= amount
            if self.remaining < 0:
                raise RuntimeFault("BUDGET_EXHAUSTED", "Evaluation fuel exhausted", {"budget": self.initial_budget})

    def fresh(self, prefix="binding"):
        self._fresh_id += 1
        return f"${prefix}:{self._fresh_id}"

    def require_capability(self, value, name):
        admitted = self.capabilities.get(name)
        if not isinstance(value, Capability) or value is not admitted:
            raise RuntimeFault("CAPABILITY_REQUIRED", "Operation requires an explicitly granted capability", {"capability": name})

    def run(self, entry, args=None):
        value = self.resolve(entry, self.globals)
        arguments = [] if args is None else args
        if isinstance(value, (Closure, Native)) or callable(value):
            return self.invoke(value, arguments)
        if arguments:
            raise RuntimeFault("NOT_CALLABLE", "Value definition cannot accept arguments", {"entry": entry})
        return value

    def resolve(self, name, env):
        if name in env:
            value = env[name]
            if isinstance(value, Definition):
                if value.evaluated:
                    return value.value
                definition = value.definition
                if definition.get("is_function", bool(definition.get("params"))):
                    value.value = Closure(definition.get("params", []), definition["body"], self.globals, self)
                    value.evaluated = True
                    return value.value
                if value.evaluating:
                    raise RuntimeFault("UNGUARDED_RECURSION", "Value definitions cannot unfold recursively", {"name": name})
                value.evaluating = True
                try:
                    value.value = self.evaluate(definition["body"], self.globals)
                    value.evaluated = True
                    return value.value
                finally:
                    value.evaluating = False
            return value
        if name in self.registry:
            return Native(name, self.registry[name])
        if any(key.startswith(name + ".") for key in self.registry):
            return Namespace(name)
        if name in {"Type", "Unit", "Int", "Nat", "Bool", "Text", "Real", "Decimal", "F32", "F64", "U8", "U64", "Bit", "Clock", "ConsoleCap", "Http", "Response", "Substitution", "DAE", "RealTrajectory", "PositiveReal", "List", "Pair", "Fn", "Array", "Bag", "Owned", "Buffer", "Code", "Bits", "Task", "Partial", "Option", "Effect", "Measure", "Rel", "Logic"}:
            return TypeSymbol(name)
        raise RuntimeFault("UNBOUND_NAME", "Name is not in scope", {"name": name})

    def evaluate(self, expression, env):
        self.tick()
        kind = expression["kind"]
        if kind == "literal":
            return expression["value"]
        if kind == "name":
            return self.resolve(expression["name"], env)
        if kind == "member":
            value = self.evaluate(expression["object"], env)
            member = expression["name"]
            if isinstance(value, Namespace):
                name = value.name + "." + member
                if name in self.registry:
                    return Native(name, self.registry[name])
                if any(key.startswith(name + ".") for key in self.registry):
                    return Namespace(name)
                raise UnsupportedOperation(name)
            if isinstance(value, dict) and member in value:
                return value[member]
            if isinstance(value, tuple) and member in ("first", "second"):
                return value[index(0 if member == "first" else 1, len(value))]
            raise RuntimeFault("NO_MEMBER", "Value has no declared member", {"member": member})
        if kind == "lambda":
            return Closure(expression.get("params", []), expression["body"], dict(env), self)
        if kind in ("list", "tuple"):
            values = [self.evaluate(item, env) for item in expression["items"]]
            return tuple(values) if kind == "tuple" else values
        if kind == "record":
            return {key: self.evaluate(value, env) for key, value in expression["fields"].items()}
        if kind == "block":
            local = dict(env)
            for statement in expression["statements"]:
                value = self.evaluate(statement["value"], local)
                if statement["kind"] == "let":
                    local[statement["name"]] = value
            return self.evaluate(expression["result"], local)
        if kind == "call":
            callee = self.evaluate(expression["callee"], env)
            return self.invoke(callee, [self.evaluate(arg, env) for arg in expression["args"]])
        raise UnsupportedOperation(f"syntax.{kind}")

    def invoke(self, function, args):
        self.tick()
        if isinstance(function, Closure):
            if len(args) != len(function.params):
                raise RuntimeFault("ARITY", "Closure argument count differs", {"expected": len(function.params), "actual": len(args)})
            local = dict(function.env)
            for parameter, value in zip(function.params, args):
                if isinstance(parameter,dict):
                    self._validate_argument(parameter.get("type"), value, local)
                local[parameter["name"] if isinstance(parameter, dict) else parameter] = value
            active = getattr(self._calls,"active",set())
            self._calls.active = active
            identity = id(function)
            if identity in active:
                raise RuntimeFault("EXPLICIT_PARTIALITY_REQUIRED", "Recursive function re-entry requires nat.rec or core.fix_partial")
            active.add(identity)
            try:
                return self.evaluate(function.body, local)
            finally:
                active.remove(identity)
        if isinstance(function, TypeSymbol):
            return TypeSymbol(function.name, tuple(args))
        if isinstance(function, Native):
            function = function.function
        if callable(function):
            try:
                return function(*args)
            except RuntimeFault:
                raise
            except (TypeError, ValueError, KeyError, IndexError, OverflowError, ZeroDivisionError) as error:
                raise RuntimeFault("OPERATOR_DOMAIN", str(error), {"exception": type(error).__name__}) from error
        raise RuntimeFault("NOT_CALLABLE", "Application target is not a function")

    def _validate_argument(self, annotation, value, env=None):
        """Dynamic checks for decidable scalar interfaces, not a typing proof."""
        if not annotation:
            return
        if annotation.get("kind") == "call":
            callee = annotation["callee"]
            if callee.get("kind") != "name":
                return
            name,arguments = callee["name"],annotation["args"]
            def dimension(expression):
                if expression.get("kind") == "literal":
                    return natural(expression["value"])
                if expression.get("kind") == "name" and expression["name"] in (env or {}):
                    return natural(env[expression["name"]])
                raise UnsupportedOperation("runtime.dependent_index", "This interface requires an explicit natural index")
            if name == "Array" and len(arguments) == 2:
                from .theories.numeric import shape
                if arguments[1].get("kind") != "list":
                    raise UnsupportedOperation("runtime.array_shape", "Array dimensions must be an explicit list")
                expected = tuple(dimension(item) for item in arguments[1]["items"])
                actual = shape(value)
                if actual != expected:
                    raise RuntimeFault("ARRAY_INTERFACE_SHAPE", "Array value differs from its declared interface", {"expected":expected,"actual":actual})
            elif name in ("GpuRead","GpuOwn") and len(arguments) == 2:
                from .theories.processes import GpuRead
                if not isinstance(value,GpuRead if name == "GpuRead" else OwnedBuffer):
                    raise RuntimeFault("GPU_INTERFACE", "GPU input has the wrong resource interface")
                expected = dimension(arguments[1])
                if len(value.data) != expected:
                    raise RuntimeFault("GPU_INTERFACE_LENGTH", "GPU buffer length differs from the declared dependent index")
            elif name == "QReg" and len(arguments) == 1:
                from .theories.numeric import QReg
                if not isinstance(value,QReg) or value.qubits != dimension(arguments[0]):
                    raise RuntimeFault("QREG_INTERFACE", "Joint quantum register differs from the declared qubit count")
            elif name == "List" and len(arguments) == 1:
                if not isinstance(value,list):
                    raise RuntimeFault("TYPE_LIST", "Expected List")
                for item in value:
                    self.tick()
                    self._validate_argument(arguments[0],item,env)
            elif name == "Pair" and len(arguments) == 2:
                if not isinstance(value,tuple) or len(value) != 2:
                    raise RuntimeFault("TYPE_PAIR", "Expected a pair")
                for annotation,item in zip(arguments,value):
                    self._validate_argument(annotation,item,env)
            elif name == "Code" and not isinstance(value,Code):
                raise RuntimeFault("TYPE_CODE", "Expected Code")
            elif name == "Bits" and len(arguments) == 1:
                if not isinstance(value,Bits) or value.width != dimension(arguments[0]):
                    raise RuntimeFault("BITS_WIDTH", "Bits value differs from its declared width")
            elif name in ("Owned","MutView") and len(arguments) == 1:
                expected_class = OwnedBuffer if name == "Owned" else MutView
                if not isinstance(value,expected_class):
                    raise RuntimeFault("RESOURCE_INTERFACE", "Resource value has the wrong access interface")
                nested = arguments[0]
                if nested.get("kind") == "call" and nested["callee"].get("name") == "Buffer" and len(nested["args"]) == 2:
                    data = value.data if isinstance(value,OwnedBuffer) else value.owner.data
                    if len(data) != dimension(nested["args"][1]):
                        raise RuntimeFault("BUFFER_INTERFACE_LENGTH", "Buffer length differs from its declared interface")
            return
        if annotation.get("kind") != "name":
            return
        name = annotation["name"]
        if name == "Nat":
            natural(value)
        elif name == "Int":
            integer(value)
        elif name == "Bool":
            boolean(value)
        elif name == "Text" and not isinstance(value,str):
            raise RuntimeFault("TYPE_TEXT", "Expected Text")
        elif name == "PositiveReal" and (type(value) not in (int,float) or not math.isfinite(value) or value <= 0):
            raise RuntimeFault("TYPE_POSITIVE_REAL", "Expected a positive finite real realization")

    def _register_core(self):
        def nat_rec(n, seed, step):
            for k in range(natural(n)):
                self.tick()
                seed = self.invoke(step, [k, seed])
            return seed

        def fold(xs, seed, step):
            for item in xs:
                seed = self.invoke(step, [seed, item])
            return seed

        def partial_fix(body, initial):
            if isinstance(body,Closure):
                if len(body.params) != 2:
                    raise RuntimeFault("ARITY", "Partial fixed point region takes a continuation and a state")
                continuation_name = body.params[0]["name"]
                self._validate_partial_tail(body.body,continuation_name)
            state, steps = initial, 0
            def again(next_state):
                raise _PartialContinue(next_state)
            while True:
                try:
                    self.tick()
                    result = self.invoke(body, [again, state])
                    if not isinstance(result, Partial):
                        raise RuntimeFault("TYPE_PARTIAL", "Partial recursion body must return Partial")
                    return Partial(result.status, result.value, steps + result.steps)
                except _PartialContinue as continuation:
                    state, steps = continuation.value, steps + 1
                except RuntimeFault as error:
                    if error.code == "BUDGET_EXHAUSTED":
                        return Partial("budget_exhausted", state, steps)
                    raise

        def case(value, branches):
            if isinstance(value, dict) and "tag" in value:
                value = Sum(value["tag"], value.get("value"))
            if not isinstance(value, Sum) or value.tag not in branches:
                raise RuntimeFault("SUM_CASE", "Missing branch or invalid tagged sum")
            return self.invoke(branches[value.tag], [value.value])

        def quote(function):
            if not isinstance(function, Closure) or function.params:
                raise RuntimeFault("STAGING_REGION", "Quotation requires a zero-argument syntax region")
            return Code(function.body, dict(function.env))

        def bind_once(argument, builder):
            if not isinstance(argument, Code):
                raise RuntimeFault("TYPE_CODE", "bind_once requires a Code value")
            name = self.fresh("hygienic")
            reference = Code({"kind": "name", "name": name}, {}, argument.stage)
            result = self.invoke(builder, [reference])
            if not isinstance(result, Code) or result.stage != argument.stage:
                raise RuntimeFault("STAGE_MISMATCH", "Builder must return Code at the argument stage")
            # Capture each splice in its own lexical environment. A synthetic
            # name cannot collide with identifiers admitted by the U lexer.
            return Code({"kind": "bound_code", "name": name,
                         "argument": argument, "result": result}, {}, result.stage)

        def splice(code):
            if not self._quote_depth:
                raise RuntimeFault("STAGE_ESCAPE", "splice is admitted only while realizing Code")
            return self.realize_code(code, self._code_bindings)

        def println(capability, text):
            self.require_capability(capability, "console")
            if not isinstance(text, str):
                raise RuntimeFault("TYPE_TEXT", "Console output requires Text")
            print(text)
            return None

        def with_mut(buffer, body):
            if not isinstance(buffer, OwnedBuffer):
                raise RuntimeFault("TYPE_OWNED", "with_mut requires an owning buffer")
            buffer.usable()
            buffer.borrowed = True
            view = MutView(buffer, buffer.epoch)
            self.active_borrows += 1
            try:
                result = self.invoke(body, [view])
                if result is not view:
                    raise RuntimeFault("BORROW_RETURN", "Borrow region must return its mutable view")
            finally:
                self.active_borrows -= 1
                view.active, buffer.borrowed = False, False
                buffer.epoch += 1
            return buffer

        def mem_write(view, offset, value):
            if not isinstance(view, MutView):
                raise RuntimeFault("TYPE_MUT_VIEW", "Write requires an exclusive view")
            view.usable()
            view.owner.data[index(offset, len(view.owner.data))] = value
            return view

        def mem_read(access, offset):
            if not isinstance(access, (OwnedBuffer, MutView)):
                raise RuntimeFault("TYPE_ACCESS", "Read requires a live owner or borrow")
            access.usable()
            data = access.data if isinstance(access, OwnedBuffer) else access.owner.data
            return data[index(offset, len(data))]

        def mem_free(buffer):
            if not isinstance(buffer, OwnedBuffer):
                raise RuntimeFault("TYPE_OWNED", "free requires an owning buffer")
            buffer.usable()
            buffer.live = False
            buffer.epoch += 1
            return None

        def bits_add(a, b):
            if not isinstance(a, Bits) or not isinstance(b, Bits) or a.width != b.width:
                raise RuntimeFault("BITS_WIDTH", "Wrapping addition requires equal widths")
            return Bits((a.value + b.value) % (1 << a.width), a.width)

        operations = {
            "nat.zero": lambda: 0, "nat.succ": lambda a: natural(a) + 1,
            "nat.add": lambda a,b: natural(a)+natural(b), "nat.mul": lambda a,b: natural(a)*natural(b),
            "nat.lt": lambda a,b: natural(a)<natural(b), "nat.le": lambda a,b: natural(a)<=natural(b),
            "nat.eq": lambda a,b: natural(a)==natural(b), "nat.rec": nat_rec,
            "nat.ceil_div": lambda a,b: (natural(a)+natural(b)-1)//b,
            "int.add": lambda a,b: integer(a)+integer(b), "int.sub": lambda a,b: integer(a)-integer(b),
            "int.mul": lambda a,b: integer(a)*integer(b), "int.gt": lambda a,b: integer(a)>integer(b),
            "int.eq": lambda a,b: integer(a)==integer(b),
            "tuple.first": lambda p: p[0], "tuple.second": lambda p: p[1],
            "list.map": lambda xs,f: [self.invoke(f,[x]) for x in xs],
            "list.filter": lambda xs,f: [x for x in xs if boolean(self.invoke(f,[x]))],
            "list.fold_left": fold, "list.cons": lambda x,xs: [x]+xs,
            "list.tail_or_empty": lambda xs: xs[1:], "list.head_or": lambda xs,d: xs[0] if xs else d,
            "list.length": len, "sum.case": case, "sum.make": lambda tag,value: Sum(tag,value),
            "option.when": lambda predicate,body: Option(True,self.invoke(body,[])) if boolean(predicate) else Option(False),
            "partial.done": lambda value: Partial("done",value), "core.fix_partial": partial_fix,
            "syntax.quote": quote, "syntax.splice": splice, "syntax.bind_once": bind_once,
            "types.record": lambda fields: TypeSymbol("Record",tuple(fields.items())),
            "types.sum": lambda fields: TypeSymbol("Sum",tuple(fields.items())),
            "io.println": println, "mem.with_mut": with_mut, "mem.write": mem_write,
            "mem.read": mem_read, "mem.free": mem_free,
            "bits.u8": lambda value: Bits(integer(value),8), "bits.add_wrap": bits_add,
            "value.select": lambda p,a,b: a if boolean(p) else b,
            "f32.add": lambda a,b: f32(f32(a)+f32(b)), "f32.mul": lambda a,b: f32(f32(a)*f32(b)),
            "f64.add": lambda a,b: float(a)+float(b), "f64.mul": lambda a,b: float(a)*float(b),
        }
        for name, function in operations.items():
            self.register(name, function)

    def _validate_partial_tail(self, expression, continuation):
        """Refuse unsupported continuation use instead of discarding its context."""
        def contains(node):
            if isinstance(node,list):
                return any(contains(item) for item in node)
            if not isinstance(node,dict):
                return False
            if node.get("kind") == "name" and node.get("name") == continuation:
                return True
            if node.get("kind") == "lambda" and any(parameter["name"] == continuation for parameter in node.get("params",[])):
                return False
            return any(contains(value) for key,value in node.items() if key not in ("span","type"))

        def qualified(node):
            if node.get("kind") == "name":
                return node["name"]
            if node.get("kind") == "member":
                return qualified(node["object"])+"."+node["name"]
            return ""

        def refused():
            raise UnsupportedOperation("core.fix_partial", "The reference fixed-point realization requires direct tail continuations; non-tail continuation context is not discarded")

        def check(node):
            if not contains(node):
                return
            if node["kind"] == "block":
                if contains(node["statements"]):
                    refused()
                check(node["result"])
                return
            if node["kind"] == "call":
                callee,args = qualified(node["callee"]),node["args"]
                if callee == continuation:
                    if contains(args):
                        refused()
                    return
                if callee == "sum.case" and len(args) == 2 and args[1].get("kind") == "record" and not contains(args[0]):
                    for branch in args[1]["fields"].values():
                        if branch.get("kind") != "lambda":
                            if contains(branch):
                                refused()
                        else:
                            check(branch["body"])
                    return
            refused()
        check(expression)

    def realize_code(self, code, bindings=None):
        """Explicit stage transition for trusted stage-0 callers; never ambient eval."""
        if not isinstance(code, Code) or code.stage != 1:
            raise RuntimeFault("STAGE_MISMATCH", "Only Code at stage one is realizable here")
        self.tick()
        local_bindings = dict(bindings or {})
        if code.expression["kind"] == "bound_code":
            value = self.realize_code(code.expression["argument"], local_bindings)
            local_bindings[code.expression["name"]] = value
            return self.realize_code(code.expression["result"], local_bindings)
        old_bindings = getattr(self, "_code_bindings", {})
        self._code_bindings = local_bindings
        self._quote_depth += 1
        try:
            environment = dict(code.environment)
            environment.update(local_bindings)
            return self.evaluate(code.expression, environment)
        finally:
            self._quote_depth -= 1
            self._code_bindings = old_bindings


def f32(value):
    value = float(value)
    try:
        return struct.unpack("!f", struct.pack("!f", value))[0]
    except OverflowError:
        return math.copysign(math.inf, value)
