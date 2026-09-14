"""Semantic fixtures and adversarial boundaries of the stage-0 runtime."""
import contextlib
import io
import math
from pathlib import Path
import threading
import unittest

from u.source import parse
from u.evaluator import Evaluator, Code, RuntimeFault, Capability
from u.theories.base import Bits, OwnedBuffer, Sum, Partial, Option
from u.theories.relational import LogicVar, unify
from u.theories.numeric import Model, Measure, QuantumProgram, DAE
from u.theories.processes import (Clock, InputSignal, Register, GpuRead, Task,
                                  Behavior, Temporal, Fairness)

EXAMPLES = Path(__file__).resolve().parents[1] / "examples" / "original"


def example(number, **options):
    path = next(EXAMPLES.glob(f"{number:02d}_*.u"))
    return Evaluator(parse(path.read_text()),**options)


def source(body,**options):
    return Evaluator(parse('u "etellis.u/0.1"; use "u.standard/0.1"; '+body),**options)


def operation(evaluator,name,*args):
    return evaluator.invoke(evaluator.registry[name],list(args))


class CoreRuntimeTests(unittest.TestCase):
    def test_arbitrary_precision_and_nat_structure(self):
        self.assertEqual(example(2).run("square",[2**150]),2**300)
        self.assertEqual(example(3).run("fib",[100]),354224848179261915075)
        with self.assertRaisesRegex(RuntimeFault,"TYPE_NAT"):
            example(3).run("fib",[-1])

    def test_lexical_closures_and_shadowing(self):
        evaluator = source('def main() -> Int = { let x = 5; let f = fn(y: Int) => int.add(x,y); yield { let x = 100; yield f(3); }; };')
        self.assertEqual(evaluator.run("main"),8)

    def test_map_filter_reduce(self):
        self.assertEqual(example(4).run("sum_positive_squares",[[-4,3,0,2,-1]]),13)

    def test_console_capability_cannot_be_forged(self):
        evaluator = example(1,capabilities=["console"])
        with self.assertRaisesRegex(RuntimeFault,"CAPABILITY_REQUIRED"):
            evaluator.run("main",[Capability("console",object())])
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            self.assertIsNone(evaluator.run("main",[evaluator.capabilities["console"]]))
        self.assertEqual(output.getvalue(),"Hello, world!\n")

    def test_ungranted_console_is_denied(self):
        with self.assertRaisesRegex(RuntimeFault,"CAPABILITY_REQUIRED"):
            example(1).run("main",[None])

    def test_mutable_borrow_restores_owner_and_expires_view(self):
        evaluator = example(5)
        buffer = OwnedBuffer([Bits(0,8)])
        self.assertIs(evaluator.run("change_first",[buffer]),buffer)
        self.assertEqual(buffer.data,[Bits(7,8)])
        views = []
        operation(evaluator,"mem.with_mut",buffer,lambda view: views.append(view) or view)
        with self.assertRaisesRegex(RuntimeFault,"LIFETIME_ENDED"):
            operation(evaluator,"mem.read",views[0],0)

    def test_borrow_escape_nested_borrow_and_await_rejected(self):
        evaluator = example(5)
        buffer = OwnedBuffer([0])
        with self.assertRaisesRegex(RuntimeFault,"BORROW_RETURN"):
            operation(evaluator,"mem.with_mut",buffer,lambda view: [view])
        with self.assertRaisesRegex(RuntimeFault,"RESOURCE_BORROWED"):
            operation(evaluator,"mem.with_mut",buffer,lambda view: operation(evaluator,"mem.with_mut",buffer,lambda v:v))
        with self.assertRaisesRegex(RuntimeFault,"BORROW_ACROSS_AWAIT"):
            operation(evaluator,"mem.with_mut",buffer,lambda view: operation(evaluator,"async.await",Task(lambda:1)))
        self.assertFalse(buffer.borrowed)

    def test_free_consumes_handle(self):
        evaluator = example(5)
        buffer = OwnedBuffer([0])
        operation(evaluator,"mem.free",buffer)
        with self.assertRaisesRegex(RuntimeFault,"RESOURCE_CONSUMED"):
            operation(evaluator,"mem.read",buffer,0)

    def test_hygienic_binding_evaluates_effectful_argument_once(self):
        evaluator = example(7)
        calls = []
        evaluator.register("testing.next",lambda: calls.append(1) or len(calls))
        expression = {"kind":"call","callee":{"kind":"member","object":{"kind":"name","name":"testing"},"name":"next"},"args":[]}
        argument = Code(expression,{})
        result = evaluator.run("twice",[argument])
        self.assertEqual(calls,[])
        self.assertEqual(evaluator.realize_code(result),2)
        self.assertEqual(calls,[1])

    def test_splice_cannot_escape_stage(self):
        evaluator = example(7)
        with self.assertRaisesRegex(RuntimeFault,"STAGE_ESCAPE"):
            operation(evaluator,"syntax.splice",Code({"kind":"literal","value":3},{}))

    def test_turing_construction_moves_and_terminates(self):
        evaluator = example(22)
        transitions = {0:Sum("right",(1,7)),1:Sum("left",(2,9)),2:Sum("halt")}
        initial = {"state":0,"tape":{"left":[],"head":3,"right":[4]}}
        result = evaluator.run("execute",[lambda state,symbol:transitions[state],initial])
        self.assertEqual(result.status,"done")
        self.assertEqual(result.value,{"state":2,"tape":{"left":[],"head":7,"right":[9]}})
        self.assertEqual(initial["tape"]["head"],3)

    def test_partial_nontermination_reports_budget_not_halt(self):
        evaluator = example(22,budget=2000)
        initial = {"state":0,"tape":{"left":[],"head":0,"right":[]}}
        result = evaluator.run("execute",[lambda state,symbol:Sum("right",(0,0)),initial])
        self.assertEqual(result.status,"budget_exhausted")
        self.assertGreater(result.steps,0)

    def test_unknown_operator_is_explicit(self):
        evaluator = source('def main() -> Int = int.unknown(2);')
        with self.assertRaisesRegex(RuntimeFault,"UNSUPPORTED_OPERATION"):
            evaluator.run("main")

    def test_unguarded_function_recursion_cannot_hide_partiality(self):
        evaluator = source('def loop(x: Nat) -> Nat = loop(x);')
        with self.assertRaisesRegex(RuntimeFault,"EXPLICIT_PARTIALITY_REQUIRED"):
            evaluator.run("loop",[1])

    def test_non_tail_partial_context_is_refused_not_silently_dropped(self):
        evaluator = source('def main() -> Partial(Nat) = core.fix_partial(fn(again: Fn(Nat, Partial(Nat)), n: Nat) => { let result = again(1); yield partial.done(99); }, 0);')
        with self.assertRaisesRegex(RuntimeFault,"UNSUPPORTED_OPERATION"):
            evaluator.run("main")


class RelationalTests(unittest.TestCase):
    def test_original_logic_query(self):
        report = example(8).run("query")
        self.assertEqual(report["answers"],[{"who":"Cara"}])
        self.assertEqual(report["status"],"complete")

    def test_occurs_check_and_typed_unification(self):
        x = LogicVar("x","Text")
        self.assertIsNone(unify(x,[x]))
        self.assertIsNone(unify(x,42))
        self.assertEqual(unify(x,"Ada"),{x:"Ada"})
        self.assertIsNone(unify(True,1))

    def test_sql_bags_multiply_multiplicity_and_null_does_not_join(self):
        users = [{"id":1,"name":"Ada"},{"id":1,"name":"Ada"},{"id":None,"name":"Unknown"}]
        orders = [{"user_id":1,"total":5},{"user_id":1,"total":5},{"user_id":None,"total":100}]
        self.assertEqual(example(9).run("join_orders",[users,orders]),[{"name":"Ada","total":5}]*4)

    def test_sql_three_valued_conjunction(self):
        evaluator = example(9)
        self.assertIsNone(operation(evaluator,"sql.and",True,None))
        self.assertIs(operation(evaluator,"sql.and",False,None),False)


class NumericTests(unittest.TestCase):
    def test_rectangular_matrix_contraction(self):
        evaluator = example(10)
        result = evaluator.run("multiply",[[[1,2,3],[4,5,6]],[[1,2,3,4],[5,6,7,8],[9,10,11,12]]])
        self.assertEqual(result,[[38.0,44.0,50.0,56.0],[83.0,98.0,113.0,128.0]])

    def test_strict_reduction_order_and_shape_failure(self):
        evaluator = example(10)
        self.assertEqual(operation(evaluator,"array.contract",[1e16,1,-1e16],[1,1,1],[0],[0],"f64.strict_left_fold"),0.0)
        with self.assertRaisesRegex(RuntimeFault,"CONTRACTION_SHAPE"):
            operation(evaluator,"array.contract",[1,2],[1],[0],[0],"f64.strict_left_fold")

    def test_array_argument_retains_declared_shape(self):
        with self.assertRaisesRegex(RuntimeFault,"ARRAY_INTERFACE_SHAPE"):
            example(10).run("multiply",[[[1,2,3]],[[1],[2],[3]]])

    def test_measure_construction_does_not_draw(self):
        evaluator = example(11)
        model = evaluator.run("model",[[1.0,2.0]])
        self.assertIsInstance(model,Model)
        self.assertEqual(len(model.bindings),1)
        self.assertEqual(model.observations[0][1],(1.0,2.0))
        with self.assertRaisesRegex(RuntimeFault,"MEASURE_REGION_REQUIRED"):
            operation(evaluator,"prob.sample",operation(evaluator,"dist.normal",0,1))

    def test_hmc_is_reproducible_and_data_influences_posterior(self):
        def inference(data):
            evaluator = example(11)
            model = evaluator.run("model",[data])
            return operation(evaluator,"prob.infer",model,"hmc",{"seed":7,"chains":2,"draws":100,"warmup":100})
        a,b = inference([2.0]*8),inference([2.0]*8)
        self.assertEqual(a,b)
        posterior = sum(sum(chain) for chain in a["chains"])/200
        self.assertLess(abs(posterior-16/9),0.25)
        self.assertFalse(a["diagnostics"]["convergence_claim"])

    def test_zero_mass_is_held_and_densities_may_exceed_one(self):
        evaluator = example(11)
        measure = operation(evaluator,"prob.dirac",3)
        self.assertEqual(operation(evaluator,"prob.weight",2,measure).atoms,((3,2.0),))
        report = operation(evaluator,"prob.normalize",operation(evaluator,"prob.weight",0,measure))
        self.assertEqual(report["status"],"held")

    def test_quantum_program_requires_explicit_realization(self):
        evaluator = example(17)
        program = evaluator.run("experiment")
        self.assertIsInstance(program,QuantumProgram)
        instrument = operation(evaluator,"quantum.simulate",program)
        outcomes = {branch["bits"]:branch["probability"] for branch in instrument.branches}
        self.assertEqual(set(outcomes),{"00","11"})
        self.assertAlmostEqual(outcomes["00"],0.5)
        self.assertAlmostEqual(outcomes["11"],0.5)

    def test_quantum_handle_consumption_and_interference(self):
        evaluator = example(17)
        q = operation(evaluator,"quantum.zero",1)
        h = operation(evaluator,"quantum.h",q,0)
        with self.assertRaisesRegex(RuntimeFault,"RESOURCE_CONSUMED"):
            operation(evaluator,"quantum.h",q,0)
        hh = operation(evaluator,"quantum.h",h,0)
        self.assertAlmostEqual(abs(hh.amplitudes[0])**2,1.0)
        self.assertEqual(hh.amplitudes[1],0j)

    def test_complex_unitary_and_kraus_completeness(self):
        evaluator = example(17)
        q = operation(evaluator,"quantum.h",operation(evaluator,"quantum.zero",1),0)
        phase = operation(evaluator,"quantum.unitary",q,[[1,0],[0,1j]])
        self.assertAlmostEqual(phase.amplitudes[1].imag,1/math.sqrt(2))
        instrument = operation(evaluator,"quantum.instrument",phase,{"zero":[[1,0],[0,0]],"one":[[0,0],[0,1]]})
        self.assertEqual({branch["outcome"] for branch in instrument.branches},{"zero","one"})
        self.assertAlmostEqual(sum(branch["probability"] for branch in instrument.branches),1.0)
        q = operation(evaluator,"quantum.zero",1)
        with self.assertRaisesRegex(RuntimeFault,"QUANTUM_TRACE_CONTRACT"):
            operation(evaluator,"quantum.unitary",q,[[1,1],[0,1]])
        self.assertTrue(q.live)

    def test_acausal_rc_solver_satisfies_equations_and_converges(self):
        evaluator = example(21)
        dae = evaluator.run("rc",[1.0,1.0,1.0])
        self.assertIsInstance(dae,DAE)
        def solve(dt,steps):
            return operation(evaluator,"dynamics.solve",dae,"backward_euler.affine_index1",{"dt":dt,"steps":steps})
        coarse,fine = solve(0.1,10),solve(0.05,20)
        vname,iname = [variable.name for variable in dae.variables]
        exact = 1-math.exp(-1)
        coarse_error = abs(coarse["trace"][-1]["state"][vname]-exact)
        fine_error = abs(fine["trace"][-1]["state"][vname]-exact)
        self.assertLess(fine_error,coarse_error)
        self.assertLess(fine["max_step_residual"],1e-12)
        for sample in fine["trace"]:
            self.assertAlmostEqual(sample["state"][vname]+sample["state"][iname],1.0)
        self.assertIsNone(fine["global_error_bound"])

    def test_dae_requires_explicit_admitted_method(self):
        evaluator = example(21)
        dae = evaluator.run("rc",[1.0,1.0,1.0])
        with self.assertRaisesRegex(RuntimeFault,"UNSUPPORTED_OPERATION"):
            operation(evaluator,"dynamics.solve",dae,"magic_exact",{})
        with self.assertRaisesRegex(RuntimeFault,"TYPE_POSITIVE_REAL"):
            evaluator.run("rc",[-1.0,1.0,1.0])


class ProcessTests(unittest.TestCase):
    def test_task_completion_failure_and_cancellation(self):
        evaluator = example(6)
        completed = operation(evaluator,"async.then",Task(lambda:3),lambda value:value+4)
        self.assertEqual(completed.result(),7)
        failed = Task(lambda:(_ for _ in ()).throw(RuntimeFault("FIXTURE_FAILURE","failed")))
        with self.assertRaisesRegex(RuntimeFault,"FIXTURE_FAILURE"):
            failed.result()
        cancelled = Task(lambda:99)
        self.assertTrue(cancelled.cancel())
        with self.assertRaisesRegex(RuntimeFault,"TASK_CANCELLED"):
            cancelled.result()

    def test_task_can_run_before_observation(self):
        entered,release = threading.Event(),threading.Event()
        task = Task(lambda:(entered.set(),release.wait(2),17)[2]).start()
        self.assertTrue(entered.wait(2))
        self.assertEqual(task.state,"running")
        self.assertFalse(task.cancel())
        self.assertTrue(task.cancellation_requested)
        release.set()
        self.assertEqual(task.result(),17)

    def test_http_requires_authority_without_network_call(self):
        with self.assertRaisesRegex(RuntimeFault,"CAPABILITY_REQUIRED"):
            example(6).run("fetch_text",[None,"https://example.com"])

    def test_actor_supervision_and_restart_intensity(self):
        evaluator = example(12,capabilities=["actors"])
        supervisor = evaluator.run("start",[evaluator.capabilities["actors"]])
        for message in [Sum("inc"),Sum("inc"),Sum("crash"),Sum("inc")]:
            operation(evaluator,"actor.send",supervisor,"counter",message)
        report = operation(evaluator,"actor.run",supervisor)
        self.assertEqual(report["states"]["counter"],1)
        self.assertEqual([event["kind"] for event in report["events"]],["handled","handled","restarted","handled"])
        for _ in range(3):
            operation(evaluator,"actor.send",supervisor,"counter",Sum("crash"))
        self.assertEqual(operation(evaluator,"actor.run",supervisor)["status"],"failed")

    def test_temporal_fairness_changes_liveness_verdict(self):
        evaluator = example(13)
        self.assertEqual(evaluator.run("verification")["status"],"verified")
        system = evaluator.run("system")
        unfair = Behavior(system.initial,system.action,True,())
        report = operation(evaluator,"temporal.check",unfair,evaluator.run("live"),[0,1,2,3])
        self.assertEqual(report["status"],"refuted")

    def test_temporal_closed_state_certificate_is_checked(self):
        evaluator = example(13)
        with self.assertRaisesRegex(RuntimeFault,"MODEL_NOT_CLOSED"):
            operation(evaluator,"temporal.check",evaluator.run("system"),evaluator.run("safe"),[0,1,2])

    def test_hardware_reset_and_wrapping(self):
        evaluator = example(15)
        clock = Clock("fixture")
        signal = InputSignal(clock,[False,False,True,False])
        counter = evaluator.run("counter",[clock,signal])
        trace = operation(evaluator,"hw.simulate",clock,[counter],4)
        self.assertEqual([row[0].value for row in trace["trace"]],[0,1,2,0,1])
        clock2 = Clock("wrap")
        counter2 = evaluator.run("counter",[clock2,InputSignal(clock2,[False])])
        counter2.value = Bits(255,8)
        self.assertEqual(operation(evaluator,"hw.simulate",clock2,[counter2],1)["trace"][-1][0].value,0)

    def test_hardware_registers_publish_simultaneously(self):
        evaluator = example(15)
        clock = Clock("pair")
        first = Register(clock,1,None)
        second = Register(clock,2,None)
        first.update = lambda old:operation(evaluator,"hw.at_tick",second)
        second.update = lambda old:operation(evaluator,"hw.at_tick",first)
        trace = operation(evaluator,"hw.simulate",clock,[first,second],2)["trace"]
        self.assertEqual(trace,[[1,2],[2,1],[1,2]])

    def test_gpu_partition_masks_padding_and_consumes_owner(self):
        evaluator = example(16)
        output = OwnedBuffer([1.0,2.0,3.0])
        task = evaluator.run("saxpy",[3,2.0,GpuRead((2.0,3.0,4.0)),output])
        result = task.result()
        self.assertEqual(result.data,[5.0,8.0,11.0])
        with self.assertRaisesRegex(RuntimeFault,"RESOURCE_CONSUMED"):
            output.usable()

    def test_cancelled_gpu_task_restores_reserved_owner(self):
        evaluator = example(16)
        output = OwnedBuffer([1.0])
        task = evaluator.run("saxpy",[1,2.0,GpuRead((2.0,)),output])
        self.assertTrue(output.borrowed)
        self.assertTrue(task.cancel())
        output.usable()
        self.assertEqual(output.data,[1.0])

    def test_gpu_dependent_length_is_checked(self):
        with self.assertRaisesRegex(RuntimeFault,"GPU_INTERFACE_LENGTH"):
            example(16).run("saxpy",[1,2.0,GpuRead((2.0,3.0)),OwnedBuffer([1.0])])


if __name__ == "__main__":
    unittest.main()
