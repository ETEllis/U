"""Finite terms, ordered relational search, and SQL bag operations."""
from dataclasses import dataclass
from .base import RuntimeFault, UnsupportedOperation


@dataclass(frozen=True)
class LogicVar:
    identity: str
    sort: str | None = None


@dataclass(frozen=True)
class Goal:
    kind: str
    arguments: tuple


@dataclass(frozen=True)
class Relation:
    rows: tuple | None = None
    region: object = None
    arity: int = 0


def walk(term, substitution):
    while isinstance(term, LogicVar) and term in substitution:
        term = substitution[term]
    return term


def occurs(variable, term, substitution):
    term = walk(term, substitution)
    if term == variable:
        return True
    if isinstance(term, (tuple, list)):
        return any(occurs(variable, item, substitution) for item in term)
    if isinstance(term, dict):
        return any(occurs(variable, item, substitution) for item in term.values())
    return False


def reify(term, substitution):
    term = walk(term, substitution)
    if isinstance(term, tuple):
        return tuple(reify(item, substitution) for item in term)
    if isinstance(term, list):
        return [reify(item, substitution) for item in term]
    if isinstance(term, dict):
        return {key: reify(value, substitution) for key, value in term.items()}
    return term


def _sort_accepts(sort, term):
    if sort is None or isinstance(term, LogicVar):
        return True
    if sort == "Text":
        return isinstance(term, str)
    if sort == "Nat":
        return type(term) is int and term >= 0
    if sort == "Int":
        return type(term) is int
    return False


def unify(left, right, substitution=None):
    """Finite-term unification with occurs checks; None means contradiction."""
    result = dict(substitution or {})
    pending = [(left, right)]
    while pending:
        left, right = (walk(term, result) for term in pending.pop())
        if type(left) is type(right) and left == right:
            continue
        if isinstance(right, LogicVar) and not isinstance(left, LogicVar):
            left, right = right, left
        if isinstance(left, LogicVar):
            if isinstance(right, LogicVar) and left.sort and right.sort and left.sort != right.sort:
                return None
            if not _sort_accepts(left.sort, right) or occurs(left, right, result):
                return None
            # Keep the more constrained variable as representative.
            if isinstance(right, LogicVar) and left.sort and not right.sort:
                result[right] = left
            else:
                result[left] = right
        elif type(left) is type(right) and isinstance(left, (list, tuple)) and len(left) == len(right):
            pending.extend(zip(left, right))
        elif isinstance(left, dict) and isinstance(right, dict) and left.keys() == right.keys():
            pending.extend((left[key], right[key]) for key in left)
        else:
            return None
    return result


def register(evaluator):
    def fresh_region(region):
        variables = []
        for parameter in region.params:
            annotation = parameter.get("type") or {}
            sort = None
            if annotation.get("kind") == "call" and annotation.get("args"):
                sort = annotation["args"][0].get("name")
            variables.append(LogicVar(evaluator.fresh("logic"), sort))
        return variables

    def rows(values):
        if not isinstance(values, list) or any(not isinstance(row, tuple) for row in values):
            raise RuntimeFault("RELATION_ROWS", "A finite relation requires tuple rows")
        arity = len(values[0]) if values else 0
        if any(len(row) != arity for row in values):
            raise RuntimeFault("RELATION_ARITY", "All relation rows must have one arity")
        return Relation(tuple(values), arity=arity)

    def apply(relation, *terms):
        if not isinstance(relation, Relation) or len(terms) != relation.arity:
            raise RuntimeFault("RELATION_ARITY", "Relation application has incompatible arity")
        return Goal("apply", (relation, terms))

    def solve(goal, substitution):
        evaluator.tick()
        if not isinstance(goal, Goal):
            raise RuntimeFault("TYPE_GOAL", "Relational region must produce a goal")
        if goal.kind == "unify":
            result = unify(*goal.arguments, substitution)
            if result is not None:
                yield result
        elif goal.kind == "and":
            goals = goal.arguments
            if not goals:
                yield substitution
            else:
                for result in solve(goals[0], substitution):
                    yield from solve(Goal("and", goals[1:]), result)
        elif goal.kind == "or":
            for alternative in goal.arguments:
                yield from solve(alternative, substitution)
        elif goal.kind == "apply":
            relation, terms = goal.arguments
            if relation.rows is not None:
                for row in relation.rows:
                    evaluator.tick()
                    result = unify(terms, row, substitution)
                    if result is not None:
                        yield result
            else:
                yield from solve(evaluator.invoke(relation.region, list(terms)), substitution)
        else:
            raise UnsupportedOperation("logic." + goal.kind)

    def query(strategy, region):
        if strategy != "sld.leftmost.depth_first.finite_terms":
            raise UnsupportedOperation("logic.query", "Only the explicitly named finite-term depth-first strategy is realized")
        variables = fresh_region(region)
        names = [parameter["name"] for parameter in region.params]
        goal = evaluator.invoke(region, variables)
        answers = []
        try:
            for substitution in solve(goal, {}):
                answers.append({name: reify(variable, substitution) for name, variable in zip(names, variables)})
        except RuntimeFault as error:
            if error.code == "BUDGET_EXHAUSTED":
                return {"status": "unknown", "reason": "budget_exhausted", "answers": answers, "strategy": strategy}
            raise
        return {"status": "complete", "answers": answers, "strategy": strategy}

    def join(left, right, predicate):
        result = []
        for a in left:
            for b in right:
                outcome = evaluator.invoke(predicate, [a, b])
                if outcome is True:
                    result.append((a, b))
                elif outcome is not False and outcome is not None:
                    raise RuntimeFault("SQL_PREDICATE", "SQL predicate must return TRUE, FALSE, or UNKNOWN")
        return result

    def sql_and(a, b):
        if any(value is not True and value is not False and value is not None for value in (a,b)):
            raise RuntimeFault("SQL_PREDICATE", "Three-valued input required")
        return False if a is False or b is False else None if a is None or b is None else True

    operations = {
        "rel.rows": rows,
        "logic.relation": lambda region: Relation(region=region, arity=len(region.params)),
        "logic.exists": lambda region: evaluator.invoke(region, fresh_region(region)),
        "logic.fresh": lambda: LogicVar(evaluator.fresh("logic")),
        "logic.unify": lambda left,right: Goal("unify",(left,right)),
        "logic.apply": apply, "logic.and": lambda *goals: Goal("and",goals),
        "logic.or": lambda *goals: Goal("or",goals), "logic.query": query,
        "sql.eq": lambda a,b: None if a is None or b is None else a == b,
        "sql.and": sql_and, "sql.not": lambda a: None if a is None else not a,
        "sql.inner_join": join,
        "sql.project": lambda bag,region: [evaluator.invoke(region,[row]) for row in bag],
        "sql.union_all": lambda left,right: left+right,
    }
    for name, operation in operations.items():
        evaluator.register(name, operation)
