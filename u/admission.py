"""Explicit profile composition and multidimensional operator trust."""

from dataclasses import dataclass, asdict
from .evidence import digest

FAMILIES = frozenset("VMRPCDQ")


@dataclass(frozen=True)
class Admission:
    origin: str
    native_support: str
    checked_laws: tuple[str, ...] = ()
    assumptions: tuple[str, ...] = ()
    trust_dependencies: tuple[str, ...] = ()
    realizations: tuple[str, ...] = ()

    def descriptor(self):
        result = asdict(self)
        result["identity"] = digest("admission", result)
        return result


def check_composition(families, imports, *, ordering=None):
    families = frozenset(families)
    obligations, witnesses = [], []
    if ordering not in {None, "before", "after"}:
        obligations.append("unknown probability/choice observation order")
    declared_orders = {p.split("/")[1] for p in imports if p in {"u.prob-choice/before/0.1", "u.prob-choice/after/0.1"}}
    if len(declared_orders) > 1 or (ordering is not None and declared_orders and ordering not in declared_orders):
        obligations.append("conflicting probability/choice order contracts")
    unknown = families - FAMILIES
    if unknown:
        obligations.append("unknown theory families: " + ",".join(sorted(unknown)))
    allowed_imports = {"u.standard/0.1", "etellis.cdc/native-0.3.0-1307f2a7",
                       "u.prob-choice/before/0.1", "u.prob-choice/after/0.1"}
    for name in imports:
        if name not in allowed_imports:
            obligations.append("profile-unavailable: " + name)
    if "R" in families and "P" in families:
        declared = [p for p in imports if p.startswith("u.prob-choice/")]
        if ordering is None and len(declared) != 1:
            obligations.append("probability/search requires an admitted observation-order profile")
        elif ordering in {None, "before", "after"}:
            ordering = ordering or declared[0].split("/")[1]
            witnesses.append({"adapter": "finite-probability-choice/1", "order": ordering,
                              "domain": "finite carriers only", "relation": "ordered behavior"})
    non_values = families - {"V", "M"}
    cdc = "etellis.cdc/native-0.3.0-1307f2a7" in imports
    if len(non_values) > 1 and not (families <= set("VMDC") and cdc) and not families <= set("VMRP"):
        obligations.append("composite-model-unavailable for " + ",".join(sorted(families)))
    for family in sorted(families - {"V"}):
        witnesses.append({"adapter": "reference-V-" + family + "/1",
                          "observation": "explicit classical descriptions and results",
                          "assumptions": ["stage0 trusted reference implementation"],
                          "physical_execution": False})
    body = {"families": sorted(families), "imports": sorted(imports), "ordering": ordering,
            "adapter_witnesses": witnesses, "numeric": "binary64-ordered-host-libm",
            "observation": "declared-reference-events"}
    return {"admitted": not obligations, "profile": digest("composite-profile", body),
            "dependencies": sorted(imports), "assumptions": ["reference-fragment contracts"],
            "obligations": obligations, "adapter_witnesses": witnesses}
