"""Runtime observations suggested by the small Lean invariants.

These tests do not prove a refinement relation between Lean and Python.
They challenge the live operators on independent inputs, rather than copying
their implementations into a second test-only evaluator.
"""
import itertools
import math
import unittest

from u import cdc
from u.evaluator import Evaluator


def state_for_trits(trits):
    phases = {-1:math.pi, 0:math.pi/2, 1:0.0}
    return cdc.make_state({
        "fields":[{"name":"f","deadband":0.1}],
        "modules":[{"name":"m","field":"f"}],
        "cells":[{"name":f"c{i}","module":"m","theta":phases[trit],
                  "has_latch":True,"latch":1 if i%2 else -1}
                 for i,trit in enumerate(trits)],
    })


class FormalInspiredRuntimeTests(unittest.TestCase):
    def test_both_polarities_only_admit_aperture_exhaustively_through_six_cells(self):
        checked = 0
        for length in range(1,7):
            for trits in itertools.product((-1,0,1),repeat=length):
                positive = cdc.commit(state_for_trits(trits),"m")
                negative = cdc.commit(state_for_trits(tuple(-t for t in trits)),"m")
                both = positive["status"] == negative["status"] == "accepted"
                self.assertEqual(both,all(trit == 0 for trit in trits),trits)
                checked += 1
        self.assertEqual(checked,1092)

    def test_held_prefix_does_not_overwrite_existing_latches(self):
        for tail in itertools.product((-1,0,1),repeat=3):
            state = state_for_trits((-1,)+tail)
            before = [(cell["latch"],cell["has_latch"]) for cell in state["cells"]]
            held = cdc.commit(state,"m")
            self.assertEqual(held["verdict"],"Held")
            self.assertEqual([(cell["latch"],cell["has_latch"]) for cell in held["state"]["cells"]],before)

    def test_hold_stops_analysis_without_manufacturing_new_value(self):
        evaluator = Evaluator({"definitions":[]})
        prior = {"verdict":"Held","reason":"no_recurrence","earned_artifacts":[{"path":"original"}]}
        def forbidden(_):
            self.fail("A held analysis must not invoke a downstream constructor")
        result = evaluator.invoke(evaluator.registry["analysis.then"],[prior,forbidden])
        self.assertIs(result,prior)
        self.assertNotIn("value",result)

    def test_later_hold_retains_earlier_artifacts_in_order(self):
        evaluator = Evaluator({"definitions":[]})
        earlier = {"verdict":"Done","value":7,"earned_artifacts":[{"identity":"a"},{"identity":"b"}]}
        later = {"verdict":"Held","reason":"backend_unavailable","earned_artifacts":[{"identity":"c"}]}
        result = evaluator.invoke(evaluator.registry["analysis.then"],[earlier,lambda value:later])
        self.assertEqual(result["earned_artifacts"],[{"identity":"a"},{"identity":"b"},{"identity":"c"}])
        self.assertEqual(later["earned_artifacts"],[{"identity":"c"}])
        self.assertNotIn("value",result)


if __name__ == "__main__":
    unittest.main()
