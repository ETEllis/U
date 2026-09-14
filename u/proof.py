"""Small independent total dependent core for Nat/Pi/Eq and induction.

Proof candidates are syntax. Only this checker creates CheckedProof. The
object language cannot call arbitrary host functions or add reduction rules.
The wider inductive/universe calculus remains an explicit extension gate.
"""

from __future__ import annotations

from dataclasses import dataclass
from .evidence import digest

NAT = ("Nat",)
SORT0 = ("sort", 0)
_RECEIPTS = {}


def is_checked_receipt(value):
    stored = _RECEIPTS.get(id(value))
    return stored is not None and stored[0] is value and stored[1] == digest("proof-receipt", value)


def free(term):
    if not isinstance(term, tuple):
        return set()
    if term[0] == "var":
        return {term[1]}
    if term[0] in {"pi", "lam"}:
        return free(term[2]) | (free(term[3]) - {term[1]})
    return set().union(*(free(t) for t in term[1:] if isinstance(t, tuple)))


def substitute(term, name, value):
    if term[0] == "var":
        return value if term[1] == name else term
    if term[0] in {"pi", "lam"}:
        tag, binder, typ, body = term
        typ = substitute(typ, name, value)
        if binder == name:
            return (tag, binder, typ, body)
        if binder in free(value):
            used = free(body) | free(value) | {name, binder}
            fresh = binder + "_"
            while fresh in used:
                fresh += "_"
            body = substitute(body, binder, ("var", fresh))
            binder = fresh
        return (tag, binder, typ, substitute(body, name, value))
    return tuple(substitute(x, name, value) if isinstance(x, tuple) else x for x in term)


def alpha(term, context=()):
    if term[0] == "var":
        for i, n in enumerate(reversed(context)):
            if term[1] == n:
                return ("bound", i)
        return term
    if term[0] in {"pi", "lam"}:
        return (term[0], alpha(term[2], context), alpha(term[3], context + (term[1],)))
    return tuple(alpha(x, context) if isinstance(x, tuple) else x for x in term)


class Kernel:
    def __init__(self, budget=100000):
        self.remaining = budget

    def tick(self):
        self.remaining -= 1
        if self.remaining < 0:
            raise ValueError("total-checker-resource-limit: no proof issued")

    def normalize(self, term):
        self.tick()
        if not isinstance(term, tuple) or not term:
            raise ValueError("invalid proof syntax")
        tag = term[0]
        if tag in {"sort", "Nat", "nat", "var"}:
            return term
        if tag in {"lam", "pi"}:
            return (tag, term[1], self.normalize(term[2]), self.normalize(term[3]))
        args = tuple(self.normalize(t) if isinstance(t, tuple) else t for t in term[1:])
        if tag == "app" and args[0][0] == "lam":
            return self.normalize(substitute(args[0][3], args[0][1], args[1]))
        if tag == "succ" and args[0][0] == "nat":
            return ("nat", args[0][1] + 1)
        if tag == "add":
            a, b = args
            if b == ("nat", 0):
                return a
            if b[0] == "nat":
                if a[0] == "nat":
                    return ("nat", a[1] + b[1])
                # Structural recursion on the second argument, with budget.
                result = a
                for _ in range(b[1]):
                    self.tick()
                    result = ("succ", result)
                return result
            if b[0] == "succ":
                return self.normalize(("succ", ("add", a, b[1])))
        if tag == "ind" and args[0][0] == "nat":
            n, motive, base, step = args
            result = base
            for k in range(n[1]):
                self.tick()
                result = self.normalize(("app", ("app", step, ("nat", k)), result))
            return result
        return (tag,) + args

    def equal(self, left, right):
        return alpha(self.normalize(left)) == alpha(self.normalize(right))

    def expect(self, term, typ, context):
        actual = self.infer(term, context)
        if not self.equal(actual, typ):
            raise ValueError(f"proof type mismatch: expected {typ!r}, inferred {actual!r}")

    def infer(self, term, context=None):
        self.tick()
        context = {} if context is None else context
        if not isinstance(term, tuple) or not term:
            raise ValueError("proof must be core syntax")
        tag = term[0]
        if tag == "sort":
            if type(term[1]) is not int or not 0 <= term[1] <= 64:
                raise ValueError("invalid universe")
            return ("sort", term[1] + 1)
        if tag == "Nat":
            return SORT0
        if tag == "nat":
            if type(term[1]) is not int or term[1] < 0:
                raise ValueError("invalid natural literal")
            return NAT
        if tag == "var":
            if term[1] not in context:
                raise ValueError("unbound proof variable " + term[1])
            return context[term[1]]
        if tag in {"pi", "lam"}:
            _, name, typ, body = term
            if name in context:
                # The named bootstrap kernel deliberately forbids shadowing:
                # rebinding a name would otherwise retarget dependent types
                # stored in the context. Surface alpha-renaming is separate.
                raise ValueError("proof binder shadows an existing dependent context name")
            universe = self.normalize(self.infer(typ, context))
            if universe[0] != "sort":
                raise ValueError("binder annotation is not a type")
            extended = dict(context, **{name: typ})
            body_type = self.infer(body, extended)
            if tag == "lam":
                return ("pi", name, typ, body_type)
            result_universe = self.normalize(body_type)
            if result_universe[0] != "sort":
                raise ValueError("Pi codomain is not a type")
            return ("sort", max(universe[1], result_universe[1]))
        if tag == "app":
            ft = self.normalize(self.infer(term[1], context))
            if ft[0] != "pi":
                raise ValueError("application of nonfunction proof")
            self.expect(term[2], ft[2], context)
            return substitute(ft[3], ft[1], term[2])
        if tag in {"succ", "add"}:
            for argument in term[1:]:
                self.expect(argument, NAT, context)
            return NAT
        if tag == "eq":
            _, typ, left, right = term
            universe = self.normalize(self.infer(typ, context))
            if universe[0] != "sort":
                raise ValueError("equality carrier is not a type")
            self.expect(left, typ, context)
            self.expect(right, typ, context)
            return universe
        if tag == "refl":
            _, typ, value = term
            self.expect(value, typ, context)
            return ("eq", typ, value, value)
        if tag == "cong":
            _, a, b, function, proof = term
            binder = "_cong"
            while binder in free(a) | free(b) | set(context):
                binder += "_"
            self.expect(function, ("pi", binder, a, b), context)
            equality = self.normalize(self.infer(proof, context))
            if equality[0] != "eq" or not self.equal(equality[1], a):
                raise ValueError("congruence requires an equality proof")
            return ("eq", b, ("app", function, equality[2]), ("app", function, equality[3]))
        if tag == "ind":
            _, n, motive, base, step = term
            self.expect(n, NAT, context)
            mt = self.normalize(self.infer(motive, context))
            if mt[0] != "pi" or not self.equal(mt[2], NAT) or self.normalize(mt[3])[0] != "sort":
                raise ValueError("induction motive must map Nat to a type")
            self.expect(base, ("app", motive, ("nat", 0)), context)
            used = free(step) | free(motive) | set(context)
            k = "_ind_k"
            while k in used:
                k += "_"
            kv = ("var", k)
            ih = "_ind_ih"
            while ih in used | {k}:
                ih += "_"
            step_type = ("pi", k, NAT, ("pi", ih, ("app", motive, kv),
                         ("app", motive, ("succ", kv))))
            self.expect(step, step_type, context)
            return ("app", motive, n)
        raise ValueError("operation outside total proof core: " + str(tag))


@dataclass(frozen=True)
class CheckedProof:
    theorem: tuple
    term_identity: str
    kernel: str = "etellis.u.total-nat-pi-eq/1"
    assumptions: tuple = ()


def check(term, expected=None):
    kernel = Kernel()
    inferred = kernel.infer(term)
    if expected is not None and not kernel.equal(inferred, expected):
        raise ValueError("candidate does not prove the requested theorem")
    # Identity is bound to both candidate and its inferred judgment.
    return CheckedProof(kernel.normalize(inferred), digest("checked-proof", [term, inferred]))


def qualified(expr):
    if expr.get("kind") == "name":
        return expr["name"]
    if expr.get("kind") == "member":
        prefix = qualified(expr["object"])
        return prefix + "." + expr["name"] if prefix else None
    return None


def surface_term(expr, bound=()):
    kind = expr["kind"]
    if kind == "literal" and type(expr["value"]) is int:
        return ("nat", expr["value"])
    name = qualified(expr)
    if name is not None:
        if name in bound:
            return ("var", name)
        if name == "Nat":
            return NAT
        if name in {"Type", "Type_0"}:
            return SORT0
        if name == "nat.succ":
            return ("lam", "_succ", NAT, ("succ", ("var", "_succ")))
        raise ValueError("unadmitted name in proof: " + name)
    if kind == "lambda":
        params = expr["params"]
        extended = tuple(bound)
        types = []
        for p in params:
            types.append(surface_term(p["type"], extended))
            extended += (p["name"],)
        term = surface_term(expr["body"], extended)
        for p, typ in reversed(list(zip(params, types))):
            term = ("lam", p["name"], typ, term)
        return term
    if kind == "call":
        name, args = qualified(expr["callee"]), expr["args"]
        if name == "Pi" and len(args) == 2 and args[1].get("kind") == "lambda":
            lam = args[1]
            if len(lam["params"]) != 1:
                raise ValueError("Pi motive arity")
            binder = lam["params"][0]["name"]
            return ("pi", binder, surface_term(args[0], bound), surface_term(lam["body"], bound + (binder,)))
        names = {"Eq": "eq", "nat.add": "add", "nat.succ": "succ",
                 "eq.refl": "refl", "eq.congr": "cong", "nat.induction": "ind"}
        if name in names:
            return (names[name],) + tuple(surface_term(a, bound) for a in args)
        function = surface_term(expr["callee"], bound)
        for arg in args:
            function = ("app", function, surface_term(arg, bound))
        return function
    raise ValueError("syntax outside the total proof fragment: " + kind)


def register(evaluator):
    def check_candidate(candidate):
        if not hasattr(candidate, "params") or not hasattr(candidate, "body"):
            raise ValueError("proof.check requires a typed proof closure")
        expr = {"kind": "lambda", "params": candidate.params, "body": candidate.body}
        proof = check(surface_term(expr))
        conclusion = proof.theorem
        while conclusion[0] == "pi":
            conclusion = conclusion[3]
        if conclusion[0] != "eq":
            raise ValueError("proof.check requires an equality proposition, optionally quantified")
        result = {"verdict": "Done", "kind": "ProofReceipt", "kernel": proof.kernel,
                "theorem": proof.theorem, "term_identity": proof.term_identity,
                "assumptions": [], "scope": "closed Nat/Pi/Eq induction judgment; stage0 checker"}
        _RECEIPTS[id(result)] = (result, digest("proof-receipt", result))
        return result
    evaluator.register("proof.check", check_candidate)
