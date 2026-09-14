import copy
import json
import math
from pathlib import Path
import tempfile
import unittest

from u import admission, cdc, evidence, proof, resources


SOURCE = '''field f gain=1.0 deadband=0.1
module parent field=f belief=0.0 prior=0.0
module child field=f belief=0.0 prior=0.0
cell p module=parent theta=0.0 omega=0.0
cell c module=child theta=0.0 omega=1.0
flow s field=f duration=0.1
commit a module=child
nest n parent=parent child=child
'''


class IdentityTests(unittest.TestCase):
    def test_canonical_order_and_exact_bits(self):
        self.assertEqual(evidence.canonical_bytes({'b': 1, 'a': 2}), evidence.canonical_bytes({'a': 2, 'b': 1}))
        self.assertNotEqual(evidence.digest('n', 0.0), evidence.digest('n', -0.0))
        self.assertNotEqual(evidence.digest('n', 1), evidence.digest('m', 1))
        self.assertNotEqual(evidence.digest('n', 2**100), evidence.digest('n', 2**100 + 1))

    def test_duplicate_keys_rejected(self):
        with self.assertRaises(ValueError):
            evidence.strict_json('{"x":1,"x":2}')
        with self.assertRaises(ValueError):
            evidence.strict_json('{"x":NaN}')

    def test_receipt_binding_and_mutation(self):
        r = evidence.receipt(scope='s', verdict='Done', meaning='m', realization='r', source='s', inputs=[2], output=4)
        self.assertTrue(evidence.verify_receipt(r, meaning='m'))
        self.assertFalse(evidence.verify_receipt(r, meaning='other'))
        r['output'] = 5
        self.assertFalse(evidence.verify_receipt(r))

    def test_dependency_cycles_and_direction(self):
        evidence.validate_dag({'m': {'kind': 'meaning'}, 'r': {'kind': 'realization', 'dependencies': ['m']}, 'e': {'kind': 'evidence', 'dependencies': ['r']}})
        with self.assertRaises(ValueError):
            evidence.validate_dag({'m': {'kind': 'meaning', 'dependencies': ['e']}, 'e': {'kind': 'evidence'}})
        with self.assertRaises(ValueError):
            evidence.validate_dag({'a': {'dependencies': ['b']}, 'b': {'dependencies': ['a']}})

    def test_journal_torn_tail_and_corruption(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'history.jsonl'
            journal = evidence.Journal(path)
            journal.append({'state': 1})
            journal.append({'state': 2})
            self.assertEqual(len(journal.read()['records']), 2)
            raw = path.read_bytes()
            path.write_bytes(raw + b'{"partial":')
            self.assertEqual(journal.read()['tail']['verdict'], 'Indeterminate')
            with self.assertRaises(ValueError):
                journal.append({'state': 3})
            path.write_bytes(raw.replace(b'genesis', b'changed'))
            with self.assertRaises(ValueError):
                journal.read()


class ResourceTests(unittest.TestCase):
    def test_borrow_suspends_owner_and_cannot_escape(self):
        memory = resources.Memory()
        owner = memory.alloc(1)
        with self.assertRaises(ValueError):
            memory.read(owner, 0)
        view = memory.borrow_mut(owner)
        memory.write(view, 0, 7)
        with self.assertRaises(ValueError):
            memory.write(owner, 0, 1)
        with self.assertRaises(ValueError):
            memory.free(owner)
        memory.return_borrow(view)
        self.assertEqual(memory.read(owner, 0), 7)
        with self.assertRaises(ValueError):
            memory.read(view, 0)
        memory.free(owner)
        with self.assertRaises(ValueError):
            memory.read(owner, 0)

    def test_forged_handle_rejected(self):
        memory = resources.Memory()
        owner = memory.alloc(1)
        forged = resources.Handle(owner.allocation, owner.epoch, owner.token, owner.mode)
        with self.assertRaises(ValueError):
            memory.write(forged, 0, 9)

    def test_independence(self):
        read = resources.Access('a', 0, 0, 4, 'shared-read')
        write = resources.Access('a', 0, 0, 4, 'write')
        other = resources.Access('b', 0, 0, 4, 'write')
        self.assertTrue(resources.independence([read], [read])['admitted'])
        self.assertTrue(resources.independence([write], [other])['admitted'])
        self.assertFalse(resources.independence([write], [write])['admitted'])
        self.assertFalse(resources.independence([read], [write])['admitted'])
        self.assertFalse(resources.independence([write], [other], causal_edges=[('left', 'right')])['admitted'])

    def test_lease_scope_replay_and_expiry(self):
        authority = resources.Authority('test')
        lease = authority.issue('write', 'frame', 10)
        with self.assertRaises(ValueError):
            authority.admit(lease, action='write', frame='other', now=0)
        authority.admit(lease, action='write', frame='frame', now=0)
        with self.assertRaises(ValueError):
            authority.admit(lease, action='write', frame='frame', now=0)
        expired = authority.issue('write', 'frame', 1)
        with self.assertRaises(ValueError):
            authority.admit(expired, action='write', frame='frame', now=1)

    def test_composite_order_is_semantic(self):
        self.assertFalse(admission.check_composition('VRP', ['u.standard/0.1'])['admitted'])
        before = admission.check_composition('VRP', ['u.standard/0.1'], ordering='before')
        after = admission.check_composition('VRP', ['u.standard/0.1'], ordering='after')
        self.assertTrue(before['admitted'])
        self.assertTrue(after['admitted'])
        self.assertNotEqual(before['profile'], after['profile'])


class ProofTests(unittest.TestCase):
    def theorem(self):
        v = lambda x: ('var', x)
        eq = lambda x: ('eq', proof.NAT, ('add', ('nat', 0), x), x)
        motive = ('lam', 'k', proof.NAT, eq(v('k')))
        succ = ('lam', 'x', proof.NAT, ('succ', v('x')))
        step = ('lam', 'k', proof.NAT, ('lam', 'ih', eq(v('k')), ('cong', proof.NAT, proof.NAT, succ, v('ih'))))
        term = ('lam', 'n', proof.NAT, ('ind', v('n'), motive, ('refl', proof.NAT, ('nat', 0)), step))
        return term, ('pi', 'n', proof.NAT, eq(v('n')))

    def test_induction_checked_independently(self):
        term, typ = self.theorem()
        self.assertIsInstance(proof.check(term, typ), proof.CheckedProof)

    def test_bad_induction_step_rejected(self):
        term, typ = self.theorem()
        ind = term[3]
        step = ind[4]
        broken_step = ('lam', step[1], step[2], ('lam', 'ih', step[3][2], ('var', 'ih')))
        broken = ('lam', 'n', proof.NAT, ('ind', ind[1], ind[2], ind[3], broken_step))
        with self.assertRaises(ValueError):
            proof.check(broken, typ)

    def test_no_type_in_type_or_partial_conversion(self):
        with self.assertRaises(ValueError):
            proof.check(('sort', 0), ('sort', 0))
        for tag in ('fix_partial', 'foreign', 'io', 'solver_success'):
            with self.assertRaises(ValueError):
                proof.check((tag,))

    def test_capture_avoidance(self):
        term = ('lam', 'y', proof.NAT, ('var', 'x'))
        result = proof.substitute(term, 'x', ('var', 'y'))
        self.assertNotEqual(result[1], 'y')
        self.assertEqual(proof.free(result), {'y'})


class CdcTests(unittest.TestCase):
    def test_executed_three_operations(self):
        result = cdc.execute_source(SOURCE)
        self.assertEqual(result['verdict'], 'Done')
        self.assertEqual(result['state']['cells'][1]['theta'], .1)
        self.assertEqual(result['state']['modules'][0]['belief'], 1.)
        self.assertEqual(result['state']['modules'][1]['prior'], 1.)

    def test_hold_does_not_rollback_earlier_or_skip_later(self):
        source = SOURCE.replace('theta=0.0 omega=1.0', 'theta=3.141592653589793 omega=1.0')
        result = cdc.execute_source(source)
        self.assertEqual(result['trace'][1]['verdict'], 'Held')
        self.assertFalse(result['state']['cells'][1]['has_latch'])
        self.assertEqual(result['state']['modules'][0]['belief'], -1.)
        self.assertEqual(result['state']['cells'][1]['theta'], math.pi + .1)

    def test_prefix_order_and_aperture(self):
        state = cdc.parse_cdc(SOURCE)['state']
        state['cells'] = [dict(state['cells'][1], name='x', theta=0.), dict(state['cells'][1], name='y', theta=math.pi)]
        self.assertEqual(cdc.commit(state, 'child')['status'], 'accepted')
        state['cells'].reverse()
        self.assertEqual(cdc.commit(state, 'child')['status'], 'held')
        self.assertEqual(cdc.quantize(math.pi/2, .1), 0)

    def test_non_semigroup(self):
        state = cdc.make_state({'fields':[{'name':'f','gain':1.}], 'modules':[{'name':'m','field':'f'}],
            'cells':[{'name':'source','module':'m','theta':math.pi/2}, {'name':'target','module':'m','theta':0.}],
            'channels':[{'source':'source','target':'target'}]})
        first = cdc.flow(state, 'f', 1)['state']
        twice = cdc.flow(first, 'f', 1)['state']['cells'][1]['theta']
        doubled = cdc.flow(state, 'f', 2)['state']['cells'][1]['theta']
        self.assertNotAlmostEqual(twice, doubled)

    def test_native_first_registry_last(self):
        parsed = cdc.parse_cdc(SOURCE.replace('gain=1.0', 'gain=1.0 gain=2.0'))
        self.assertEqual(parsed['statements'][0]['native_first']['gain'], '1.0')
        self.assertEqual(parsed['statements'][0]['registry_last']['gain'], '2.0')
        self.assertEqual(parsed['state']['fields'][0]['gain'], 1.)

    def test_post_step_fault_retains_progress(self):
        result = cdc.execute_source(SOURCE.replace('duration=0.1', 'duration=0.1 expect-theta=c:77'))
        self.assertEqual(result['verdict'], 'Fault')
        self.assertEqual(result['state']['cells'][1]['theta'], .1)
        self.assertEqual(result['known_progress'], 1)

    def test_nest_actual_jacobian(self):
        state = cdc.parse_cdc(SOURCE)['state']
        j = cdc.jacobian(state, 'nest', {'parent':'parent','child':'child'})
        manifest = cdc.coordinates(state)
        self.assertEqual(j[manifest.index('prior:child')], j[manifest.index('belief:parent')])
        for column in range(len(manifest)):
            perturbed = copy.deepcopy(state)
            if column < len(state['cells']):
                perturbed['cells'][column]['theta'] += 1e-6
            else:
                i, k = divmod(column-len(state['cells']), 2)
                perturbed['modules'][i][('belief', 'prior')[k]] += 1e-6
            base = cdc.vector(cdc.nest(state, 'parent','child')['state'])
            changed = cdc.vector(cdc.nest(perturbed, 'parent','child')['state'])
            for row in range(len(manifest)):
                self.assertAlmostEqual((changed[row]-base[row])/1e-6, j[row][column], places=6)

    def test_recurrence_cannot_be_forged_or_rebound(self):
        held = cdc.path_tangent(SOURCE)
        self.assertEqual(cdc.check_recurrence(held, {})['verdict'], 'Held')
        still = SOURCE.split('flow s')[0] + 'flow still field=f duration=0.0\n'
        tangent = cdc.path_tangent(still)
        cert = cdc.check_recurrence(tangent, {})['value']
        returned = cdc.return_map(tangent, cert)
        spectral = cdc.spectral(returned, {})
        self.assertEqual(spectral['verdict'], 'Held')
        self.assertEqual(spectral['earned_artifacts'], [returned])
        self.assertEqual(cdc.check_recurrence(dict(tangent), {})['verdict'], 'Held')
        tangent['matrix'][0][0] = 2.
        with self.assertRaises(ValueError):
            cdc.return_map(tangent, cert)

    def test_tangent_multiplication_order(self):
        a, b = [[1, 1], [0, 1]], [[1, 0], [1, 1]]
        self.assertEqual(cdc.matmul(b, a), [[1,1],[1,2]])
        self.assertNotEqual(cdc.matmul(a, b), cdc.matmul(b,a))


if __name__ == '__main__':
    unittest.main()
