"""Exercise compiled U code through a native compiler, never the Python evaluator.

Set U_COMPILER to a compiled compiler executable. The test harness is deliberately
independent of the implementation; it invokes the same public command protocol.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
COMPILER = Path(os.environ.get("U_COMPILER", str(ROOT / "build" / "native" / "uc")))
HEADER = 'u "etellis.u/0.1";\n'


@unittest.skipUnless(COMPILER.is_file(), "build the native U compiler or set U_COMPILER")
class NativeCompilerTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="u-selfhost-test-")
        self.directory = Path(self.temporary.name)
        self.env = dict(os.environ, U_NATIVE_ALLOW="read,write,exec,console,env", PATH=str(self.directory / "no-interpreters"))

    def tearDown(self):
        self.temporary.cleanup()

    def invoke(self, *args, success=True):
        result = subprocess.run(
            [str(COMPILER), *map(str, args)], capture_output=True, text=True,
            env=self.env, timeout=180,
        )
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
        return result

    def source(self, text):
        path = self.directory / "input.u"
        path.write_text(HEADER + text)
        return path

    def execute(self, text, arguments=None, success=True):
        source = self.source(text)
        target = self.directory / "output.c"
        executable = self.directory / "output"
        self.invoke("emit", source, target)
        compiled = subprocess.run(
            ["cc", "-std=c11", "-O0", "-ffp-contract=off", "-I", str(ROOT / "native"),
             str(target), str(ROOT / "native/runtime.c"), "-lm",
             *(["-lcrypto"] if sys.platform.startswith("linux") else []), "-o", str(executable)],
            capture_output=True, text=True, timeout=180,
        )
        self.assertEqual(compiled.returncode, 0, compiled.stderr)
        cmd = [str(executable)]
        if arguments is not None:
            cmd.append(json.dumps(arguments))
        result = subprocess.run(cmd, capture_output=True, text=True, env=self.env, timeout=30)
        if not success:
            self.assertNotEqual(result.returncode, 0)
            return result.stderr
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout) if result.stdout.strip() else None

    def test_original_22_sources_parse_unchanged(self):
        examples = sorted((ROOT / "examples/original").glob("*.u"))
        self.assertEqual(len(examples), 22)
        for example in examples:
            with self.subTest(example=example.name):
                report = json.loads(self.invoke("parse", example).stdout)
                self.assertEqual(report["status"], "parsed")
                self.assertGreater(report["definitions"], 0)

    def test_direct_closure_capture_and_record(self):
        result = self.execute('''
def make(n: Int) -> Fn(Int, Int) = fn(x: Int) => int.add(n, x);
def main() -> Value = { let f = make(37); yield #{answer: f(5), pair: (3, 4)}; };
''')
        self.assertEqual(result, {"answer": 42, "pair": [3, 4]})

    def test_left_to_right_argument_evaluation(self):
        result = self.execute('''
def main() -> Value = {
  let state = cell.new(0);
  let next = fn() => { let old = cell.get(state); cell.set(state, int.add(old, 1)); yield old; };
  yield [next(), next(), next()];
};
''')
        self.assertEqual(result, [0, 1, 2])

    def test_unused_global_effect_is_not_executed(self):
        result = self.execute('''
def unused: Unit = sys.print("this must never be printed");
def main() -> Int = 42;
''')
        self.assertEqual(result, 42)

    def test_global_value_is_forced_once(self):
        result = self.execute('''
def counter: Value = cell.new(0);
def once: Int = { cell.set(counter, int.add(cell.get(counter), 1)); yield cell.get(counter); };
def main() -> Value = { let first = once; let second = once; yield [first, second, cell.get(counter)]; };
''')
        self.assertEqual(result, [1, 1, 1])

    def test_cyclic_global_forcing_is_rejected(self):
        result = self.execute('''
def first: Int = second;
def second: Int = first;
def main() -> Int = first;
''', success=False)
        self.assertIn("INITIALIZATION_CYCLE", result)

    def test_unicode_and_embedded_nul_survive_lowering(self):
        result = self.execute(r'def main() -> Text = "U: λ ∞ \u0000 \" \\";')
        self.assertEqual(result, 'U: λ ∞ \x00 " \\')

    def test_c_trigraphs_remain_literal_text(self):
        self.assertEqual(self.execute('def main() -> Text = "??/ ??= ??< ??!";'), '??/ ??= ??< ??!')

    def test_type_declarations_preserve_syntax_without_executing_it(self):
        result = self.execute('''
def Shape: Type = unsupported.type_constructor(#{dimension: 3});
def main() -> Value = Shape;
''')
        self.assertEqual(result["kind"], "type")
        self.assertEqual(result["name"], "Shape")
        self.assertEqual(result["syntax"]["kind"], "call")

    def test_lexical_shadowing_preserves_earlier_closure(self):
        result = self.execute('''
def main() -> Value = {
  let x = 3;
  let original = fn() => x;
  let inside = { let x = 7; yield (original(), x); };
  yield (inside, original());
};
''')
        self.assertEqual(result, [[3, 7], 3])

    def test_later_local_binding_does_not_rebind_captured_global(self):
        result = self.execute('''
def x: Int = 9;
def main() -> Value = {
  let original = fn() => x;
  let x = 1;
  yield [original(), x];
};
''')
        self.assertEqual(result, [9, 1])

    def test_callee_is_evaluated_before_arguments(self):
        result = self.execute('''
def main() -> Value = {
  let log = buffer.new();
  let argument = fn() => { buffer.push(log, 2); yield 42; };
  let function = fn() => { buffer.push(log, 1); yield fn(x: Int) => x; };
  let answer = function()(argument());
  yield #{answer: answer, order: buffer.freeze(log)};
};
''')
        self.assertEqual(result, {"answer": 42, "order": [1, 2]})

    def test_foreign_entry_cannot_violate_natural_annotation(self):
        result = self.execute('def main(n: Nat) -> Nat = n;', [-1], success=False)
        self.assertIn("NATIVE_ERROR", result)

    def test_dynamic_result_still_checks_declared_scalar_type(self):
        result = self.execute('def main() -> Bool = value.select(true, 1, 2);', success=False)
        self.assertIn("NATIVE_ERROR", result)

    def test_dynamic_call_still_checks_declared_parameter_type(self):
        result = self.execute('''
def accept(n: Nat) -> Nat = n;
def main() -> Value = { let callable = value.select(true, accept, accept); yield callable(-1); };
''', success=False)
        self.assertIn("NATIVE_ERROR", result)

    def test_indexed_annotations_preserve_parameter_scope_and_obligations(self):
        source = self.source('''
def signal(clk: Clock, reset: Signal(Bit, clk)) -> Signal(Bits(8), clk) = reset;
def gpu(n: Nat, input: GpuRead(F32, n)) -> GpuRead(F32, n) = input;
def owned(value: Owned(Buffer(U8, 1))) -> Owned(Buffer(U8, 1)) = value;
def async_value(value: Task(Text)) -> Task(Text) = value;
''')
        report = json.loads(self.invoke("check", source).stdout)
        self.assertIn("theory obligation: Signal", report["obligations"])
        self.assertIn("theory obligation: Owned", report["obligations"])
        self.assertTrue(any("index obligation" in item for item in report["obligations"]))

    def test_invalid_type_indices_and_constructor_arity_are_rejected(self):
        cases = [
            ('def bad(n: Text, x: GpuRead(F32, n)) -> Value = x;', 'type_index_kind'),
            ('def bad(n: Nat, x: Signal(Bit, n)) -> Value = x;', 'clock_index_kind'),
            ('def bad(x: Signal(Bit, missing)) -> Value = x;', 'unbound_index'),
            ('def bad(x: Bits(0)) -> Value = x;', 'type_index_range'),
            ('def bad(x: Array(F64, [2, -1])) -> Value = x;', 'type_index_range'),
            ('def bad(x: List(Int, Text)) -> Value = x;', 'type_arity'),
            ('def bad(n: Nat, x: List(n)) -> Value = x;', 'type_parameter_kind'),
        ]
        for source, code in cases:
            with self.subTest(code=code, source=source):
                self.assertIn(code, self.invoke("check", self.source(source), success=False).stderr)

    def test_dependent_callable_is_not_reported_as_a_proof(self):
        source = self.source('''
def candidate: Pi(Nat, fn(n: Nat) => Eq(Nat, n, n)) = fn(n: Nat) => n;
def main() -> Value = candidate;
''')
        report = json.loads(self.invoke("check", source).stdout)
        self.assertTrue(any("dependent obligation" in item for item in report["obligations"]))
        self.assertNotIn("proved", report)

    def test_types_have_inert_first_class_descriptors(self):
        self.assertEqual(self.execute('def main() -> Value = Nat;'), {"kind": "type", "name": "Nat"})

    def test_original_induction_example_reaches_the_u_proof_kernel(self):
        modules = [ROOT / "stdlib/core.u", ROOT / "stdlib/proof.u", ROOT / "examples/original/14_proof.u"]
        source = "\n".join(path.read_text() for path in modules)
        result = self.execute(source + '\ndef main() -> Value = #{verdict: checked.verdict, checker: checked.checker};\n')
        self.assertEqual(result["verdict"], "Done")
        self.assertEqual(result["checker"], "etellis.u/native-total-nat-pi-eq/0.2")

    def test_dependent_annotation_cannot_substitute_for_proof_checking(self):
        source = (ROOT / "stdlib/core.u").read_text() + "\n" + (ROOT / "stdlib/proof.u").read_text()
        source += '''
def candidate: Pi(Nat, fn(n: Nat) => Eq(Nat, n, n)) = fn(n: Nat) => n;
def checked: ProofReceipt = proof.check(candidate);
def main() -> Value = checked.verdict;
'''
        self.assertIn("proof", self.execute(source, success=False).lower())

    def test_unbounded_integer_arithmetic(self):
        result = self.execute('def main() -> Int = int.mul(123456789012345678901234567890, 98765432109876543210);')
        self.assertEqual(result, 123456789012345678901234567890 * 98765432109876543210)

    def test_global_namespace_can_be_shadowed(self):
        result = self.execute('''
def main() -> Value = {
  let int = #{add: fn(a: Text, b: Text) => text.concat(a, b)};
  yield int.add("a", "b");
};
''')
        self.assertEqual(result, "ab")

    def test_concatenated_module_headers(self):
        result = self.execute('''
def first() -> Int = 41;
u "etellis.u/0.1";
use "u.standard/0.1";
def main() -> Int = int.add(first(), 1);
''')
        self.assertEqual(result, 42)

    def test_rejects_duplicate_and_unbound_names(self):
        cases = [
            ('def main() -> Int = absent;', 'unbound_name'),
            ('def main() -> Int = 0; def main() -> Int = 1;', 'duplicate_definition'),
            ('def main() -> Value = #{x: 1, x: 2};', 'duplicate_field'),
            ('def f(x: Int, x: Int) -> Int = x;', 'duplicate_parameter'),
            ('def main() -> Int = { let x = 1; let x = 2; yield x; };', 'duplicate_binding'),
        ]
        for source, code in cases:
            with self.subTest(code=code):
                result = self.invoke("check", self.source(source), success=False)
                self.assertIn(code, result.stderr)

    def test_rejects_wrong_core_types_and_arity(self):
        cases = [
            ('def main() -> Bool = 7;', 'return_type'),
            ('def f(x: Int) -> Int = x; def main() -> Int = f();', 'call_arity'),
            ('def f(x: Int) -> Int = x; def main() -> Int = f("wrong");', 'argument_type'),
            ('def main() -> Int = int.add("wrong", 1);', 'argument_type'),
            ('def main() -> Int = int.add(1);', 'call_arity'),
            ('def main() -> Int = 3();', 'not_callable'),
            ('def main() -> ImpossibleType = 3;', 'unknown_type'),
        ]
        for source, code in cases:
            with self.subTest(code=code):
                result = self.invoke("check", self.source(source), success=False)
                self.assertIn(code, result.stderr)

    def test_missing_closed_native_members_fail_check_and_emit(self):
        for expression in ["int.this_does_not_exist(1)", "native.sys_typo()", "int.add.typo(1)", "native.int_add.typo(1)"]:
            with self.subTest(expression=expression):
                source = self.source(f"def main() -> Int = {expression};")
                self.invoke("check", source, success=False)
                output = self.directory / "missing-native.c"
                self.invoke("emit", source, output, success=False)
                self.assertFalse(output.exists())

    def test_ordinary_global_record_is_not_a_closed_native_namespace(self):
        result = self.execute('''
def int: Value = #{this_does_not_exist: fn(value: Text) => value};
def main() -> Text = int.this_does_not_exist("ordinary record");
''')
        self.assertEqual(result, "ordinary record")

    def test_rejects_malformed_lexical_input(self):
        cases = [
            ('def main() -> Int = 1e;', 'invalid_number'),
            ('def main() -> Int = 12abc;', 'invalid_number'),
            ('def main() -> Text = "missing;', 'unterminated_string'),
            ('/* unfinished', 'unterminated_comment'),
            ('def main() -> Int = { let x = 2; };', 'missing_yield'),
        ]
        for source, code in cases:
            with self.subTest(code=code):
                result = self.invoke("parse", self.source(source), success=False)
                self.assertIn(code, result.stderr)

    def test_lexical_span_skipping_preserves_strings_and_comments(self):
        source = r'''
/* A block containing // line markers and "quotes". */
def main() -> Text = "/* literal */ // literal \"quote\" \u03bb"; // final comment
'''
        self.assertEqual(self.execute(source), '/* literal */ // literal "quote" λ')

    def test_long_identifier_and_comment_spans(self):
        name = "source_binding_" + "x" * 500
        source = "/*" + ("ordinary comment content " * 500) + "*/\n"
        source += f"def {name}(x: Int) -> Int = x;\ndef main() -> Int = {name}(42);\n"
        self.assertEqual(self.execute(source), 42)

    def test_metadata_is_available_without_ast_interpretation(self):
        result = self.execute('''
def main() -> Value = value.describe(fn(x: Int) => int.add(x, 1));
''')
        self.assertEqual(result["params"][0]["name"], "x")
        self.assertEqual(result["body"]["kind"], "call")

    def test_emission_is_deterministic(self):
        source = self.source('def main() -> Int = int.add(20, 22);')
        first, second = self.directory / "first.c", self.directory / "second.c"
        self.invoke("emit", source, first)
        self.invoke("emit", source, second)
        self.assertEqual(first.read_bytes(), second.read_bytes())
        self.assertNotIn("Py_", first.read_text())
        self.assertNotIn("eval(", first.read_text())

    def test_complete_library_bundle_compiles_in_default_arena(self):
        modules = [
            "stdlib/core.u", "stdlib/numeric.u", "stdlib/relational.u", "stdlib/syntax.u",
            "stdlib/probability.u", "stdlib/proof.u", "stdlib/processes.u", "stdlib/dynamics.u",
            "stdlib/inspection.u", "stdlib/evidence.u", "cdc/primitives.u", "cdc/analysis.u",
            "examples/original/03_fibonacci.u",
        ]
        source = "\n".join((ROOT / path).read_text() for path in modules)
        source += "\ndef main() -> Nat = fib(100);\n"
        self.env["U_NATIVE_MEMORY_MB"] = "1024"
        self.assertEqual(self.execute(source), 354224848179261915075)


if __name__ == "__main__":
    unittest.main()
