"""Independent adversarial cases found during the final authority review."""

import dataclasses
import math
import unittest

from u import admission, cdc, evidence, proof, resources
from u.backend import certify
from u.source import parse


class FinalAuthorityRegressions(unittest.TestCase):
    def test_dependent_shadow_cannot_prove_zero_equals_one(self):
        # A name-only context used to reinterpret x : outer A as inner A.
        bad_function = (
            "lam", "A", proof.SORT0,
            ("lam", "x", ("var", "A"),
             ("lam", "A", proof.SORT0, ("var", "x"))),
        )
        false_equality = ("eq", proof.NAT, ("nat", 0), ("nat", 1))
        candidate = (
            "app",
            ("app", ("app", bad_function, proof.NAT), ("nat", 0)),
            false_equality,
        )
        with self.assertRaises(ValueError):
            proof.check(candidate, false_equality)

    def test_marker_shaped_records_do_not_collide_with_typed_values(self):
        examples = [
            (1, {"$int": "1"}),
            (1.0, {"$f64": "3ff0000000000000"}),
            (b"a", {"$bytes": "61"}),
            (1 + 2j, {"$complex": [{"$f64": "3ff0000000000000"},
                                      {"$f64": "4000000000000000"}]}),
            ((1, 2), [1, 2]),
        ]
        for typed, record in examples:
            with self.subTest(typed=repr(typed)):
                self.assertNotEqual(evidence.canonical_bytes(typed), evidence.canonical_bytes(record))
                self.assertNotEqual(evidence.digest("review-value", typed), evidence.digest("review-value", record))

    def test_nonfinite_json_cannot_hide_in_exponent_notation(self):
        for source in ("1e400", "-1e400", '{"amount":1e400}', "[1e9999]"):
            with self.subTest(source=source), self.assertRaises(ValueError):
                evidence.strict_json(source)
        self.assertEqual(evidence.strict_json("1e300"), 1e300)

    def test_nonfinite_time_neither_issues_nor_admits_a_lease(self):
        authority = resources.Authority("review")
        lease = authority.issue("read", "frame", 1)
        for bad in (float("nan"), float("inf"), -float("inf"), True):
            with self.subTest(value=repr(bad)):
                with self.assertRaises(ValueError):
                    authority.issue("read", "frame", bad)
                with self.assertRaises(ValueError):
                    authority.admit(lease, action="read", frame="frame", now=bad)
        # Refused attempts cannot consume the still-valid genuine authority.
        self.assertTrue(authority.admit(lease, action="read", frame="frame", now=0))

    def test_invalid_access_shapes_never_earn_independence(self):
        right = [resources.Access("same", 0, 0, 10, "write")]
        for access in (
            resources.Access("same", 0, 9, 1, "write"),
            resources.Access("same", -1, 0, 1, "write"),
            resources.Access("same", 0, -1, 0, "write"),
            resources.Access("same", 0, 0, 1, "invented-mode"),
        ):
            with self.subTest(access=access):
                self.assertFalse(resources.independence([access], right)["admitted"])
        self.assertFalse(resources.independence(
            [resources.Access("same", 0, 0, 0, "owned")],
            [resources.Access("same", 0, 0, 0, "owned")],
        )["admitted"])
        self.assertFalse(resources.independence(
            [resources.Access("same", 0, 0, 10, "write")],
            [resources.Access("same", 1, 0, 10, "write")],
        )["admitted"])

    def test_probability_order_must_be_valid_and_match_import(self):
        before = ["u.prob-choice/before/0.1"]
        self.assertFalse(admission.check_composition("RP", before, ordering="evil")["admitted"])
        self.assertFalse(admission.check_composition("RP", before, ordering="after")["admitted"])
        self.assertTrue(admission.check_composition("RP", before, ordering="before")["admitted"])

    def test_recurrence_retains_its_quantitative_qualification(self):
        source = ("field f gain=0\nmodule m field=f\n"
                  "cell c module=m theta=0 omega=1\n"
                  "flow step field=f duration=1\n")
        tangent = cdc.path_tangent(source)
        self.assertEqual(cdc.check_recurrence(tangent, {"tolerance": 0})["verdict"], "Held")
        certificate = cdc.check_recurrence(tangent, {"tolerance": 2})["value"]
        fields = dataclasses.asdict(certificate)
        self.assertEqual(fields["tolerance"], 2)
        self.assertEqual(fields["norm"], "linf")
        self.assertEqual(fields["residual"], 1)
        returned = cdc.return_map(tangent, certificate)
        self.assertEqual(returned["recurrence_contract"]["tolerance"], 2)
        self.assertEqual(returned["recurrence_contract"]["residual"], 1)
        self.assertIn("not an exact", returned["recurrence_contract"]["claim"])

    def test_nonfinite_source_expectation_faults_after_the_step(self):
        source = ("field f gain=0\nmodule m field=f\n"
                  "cell c module=m theta=0 omega=1\n"
                  "flow step field=f duration=1 expect-theta=c:nan\n")
        result = cdc.execute_source(source)
        self.assertEqual(result["verdict"], "Fault")
        self.assertEqual(result["known_progress"], 1)
        self.assertEqual(result["state"]["cells"][0]["theta"], 1)

    def test_meaning_cannot_depend_transitively_on_execution_evidence(self):
        objects = {
            "meaning": {"kind": "meaning", "dependencies": ["profile"]},
            "profile": {"kind": "profile", "dependencies": ["execution"]},
            "execution": {"kind": "evidence"},
        }
        with self.assertRaises(ValueError):
            evidence.validate_dag(objects)

    def test_compiled_fragment_cannot_bypass_import_admission(self):
        module = parse('u "etellis.u/0.1"; use "missing-profile/1"; '
                       'def square(x: Int) -> Int = int.mul(x, x);')
        with self.assertRaises(ValueError):
            certify(module, "square", [[0, 2]])


if __name__ == "__main__":
    unittest.main()
