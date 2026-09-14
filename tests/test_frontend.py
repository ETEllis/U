from copy import deepcopy
from dataclasses import FrozenInstanceError
from pathlib import Path
import random
import unittest

from u.source import SourceError, parse, scan, format_source, semantic_ast
from u.checker import check
from u.graph import KERNEL_TAGS, DERIVED_RULES, KernelNode, elaborate
from u.graph_execution import GraphError, reconstruct, run_graph

ROOT = Path(__file__).resolve().parents[1]
EXAMPLES = ROOT / "examples" / "original"
HEADER = 'u "etellis.u/0.1"; use "u.standard/0.1";\n'


def module(body):
    return parse(HEADER + body)


def nodes(value):
    if isinstance(value, dict):
        if "tag" in value:
            yield value
        for item in value.values():
            yield from nodes(item)
    elif isinstance(value, list):
        for item in value:
            yield from nodes(item)


class ScannerParserTests(unittest.TestCase):
    def test_all_preserved_examples_share_one_parser(self):
        paths = sorted(EXAMPLES.glob("*.u"))
        self.assertEqual(len(paths), 22)
        for path in paths:
            with self.subTest(example=path.name):
                source = path.read_text()
                result = parse(source)
                self.assertEqual(result["source"], source)
                self.assertTrue(result["definitions"])
                self.assertEqual("".join(token.text for token in scan(source)), source)

    def test_lossless_unicode_comments_strings_and_spans(self):
        source = HEADER + '// αβ\ndef x: Text = "hello \\u03bb";\n'
        tokens = scan(source)
        self.assertEqual("".join(token.text for token in tokens), source)
        value = parse(source)["definitions"][0]["body"]
        self.assertEqual(value["value"], "hello λ")
        self.assertEqual(value["span"]["line"], 3)

    def test_contextual_header_u_can_be_a_parameter(self):
        self.assertEqual(check(module("def f(u: Int) -> Int = int.add(u, 1);"))["status"], "checked")

    def test_malformed_programs_fail_with_spans(self):
        invalid = ["def f(x: Int) -> Int = ;", "def a: Int = { let x = 1; };",
                   "def f(x: Int) -> Int = int.add(x 1);", "def a: Int = 1",
                   "def a: Int = #{x: 1, x: 2};", "def f(x: Int, x: Int) -> Int = x;",
                   "def a: Int = 1; def a: Int = 2;", "def a: Int = { let x = 1; let x = 2; yield x; };"]
        for body in invalid:
            with self.subTest(body=body), self.assertRaises(SourceError) as error:
                module(body)
            self.assertGreaterEqual(error.exception.span["line"], 1)
            self.assertGreaterEqual(error.exception.span["column"], 1)

    def test_lexical_errors(self):
        for body in ['def a: Text = "unterminated;', 'def a: Text = "\\q";',
                     'def a: Text = "\\ud800";', 'def a: Int = 1e;',
                     'def a: Int = 123word;', 'def a: F64 = 1e999;',
                     'def a: Int = @;', '/* unterminated']:
            with self.subTest(body=body), self.assertRaises(SourceError):
                module(body)

    def test_parser_bounds_nested_and_postfix_structures(self):
        for expression in ["(" * 200 + "0" + ")" * 200, "a" + ".a" * 200, "[" * 200 + "]" * 200]:
            with self.assertRaises(SourceError) as error:
                module(f"def a: Int = {expression};")
            self.assertEqual(error.exception.code, "nesting_limit")

    def test_version_and_duplicate_import_are_rejected(self):
        for text in ['u "unknown/1";', HEADER + 'use "u.standard/0.1";']:
            with self.assertRaises(SourceError):
                parse(text)

    def test_negative_numbers_unit_singletons_and_records(self):
        result = module('def x: Int = -2; def y: Unit = (); def z: Int = (3);')
        self.assertEqual(result["definitions"][0]["body"]["value"], -2)
        self.assertEqual(result["definitions"][1]["body"]["items"], [])
        self.assertEqual(result["definitions"][2]["body"]["kind"], "literal")

    def test_seeded_malformed_fuzz_is_bounded(self):
        rng = random.Random(824)
        alphabet = 'abc012()[]{}#,:;.="\\\n '
        for _ in range(250):
            source = HEADER + "".join(rng.choice(alphabet) for _ in range(rng.randrange(1, 120)))
            try:
                parse(source)
            except SourceError:
                pass


class FormatterTests(unittest.TestCase):
    def test_all_originals_idempotent_semantics_and_comments(self):
        for path in sorted(EXAMPLES.glob("*.u")):
            with self.subTest(example=path.name):
                source = path.read_text()
                formatted = format_source(source)
                self.assertEqual(semantic_ast(parse(source)), semantic_ast(parse(formatted)))
                self.assertEqual(format_source(formatted), formatted)
                original_comments = [token.text for token in scan(source) if token.kind == "comment"]
                self.assertEqual(original_comments, [token.text for token in scan(formatted) if token.kind == "comment"])

    def test_comments_do_not_change_meaning(self):
        plain = HEADER + "def f(x: Int) -> Int = int.add(x, -2);"
        commented = HEADER + "/* before */ def f(x: Int) -> Int = int.add(x, // keep x\n -2);"
        self.assertEqual(semantic_ast(parse(plain)), semantic_ast(parse(commented)))
        self.assertEqual(elaborate(parse(plain))["structural_digest"], elaborate(parse(commented))["structural_digest"])


class CheckerTests(unittest.TestCase):
    def assert_rejected(self, body, code):
        result = check(module(body))
        self.assertEqual(result["status"], "rejected", result)
        self.assertIn(code, [diagnostic["code"] for diagnostic in result["diagnostics"]])
        self.assertFalse(result["typechecked"])

    def test_core_originals_checked_with_no_type_holes(self):
        for number in (1, 2, 3, 4, 5, 17, 22):
            path = next(EXAMPLES.glob(f"{number:02}_*.u"))
            with self.subTest(example=path.name):
                result = check(parse(path.read_text()))
                self.assertEqual(result["status"], "checked", result)
                self.assertFalse(result["obligations"])

    def test_advanced_originals_preserve_uncertainty(self):
        for path in EXAMPLES.glob("*.u"):
            if int(path.name[:2]) in (1, 2, 3, 4, 5, 17, 22):
                continue
            with self.subTest(example=path.name):
                result = check(parse(path.read_text()))
                self.assertEqual(result["status"], "unsupported", result)
                self.assertTrue(result["obligations"])
                self.assertFalse(result["typechecked"])
                self.assertFalse(result["diagnostics"])

    def test_literal_and_return_type_errors(self):
        self.assert_rejected('def f: Int = "no";', "type_mismatch")
        self.assert_rejected('def f: Nat = -1;', "type_mismatch")
        self.assert_rejected('def f: U8 = 256;', "integer_range")
        self.assert_rejected('def f: Bool = int.add(1, 2);', "type_mismatch")

    def test_undefined_names_and_operators(self):
        self.assert_rejected('def f: Int = missing;', "undefined_name")
        self.assert_rejected('def f: Int = int.not_an_operator(1);', "undefined_operator")
        self.assert_rejected('def f: Missing = 1;', "undefined_type")

    def test_arity_and_callback_errors(self):
        self.assert_rejected('def f: Int = int.add(1);', "arity")
        self.assert_rejected('def f(xs: List(Int)) -> List(Int) = list.filter(xs, fn(x: Int) => x);', "type_mismatch")
        self.assert_rejected('def f: Int = 3(1);', "not_callable")

    def test_block_effects_cannot_be_erased(self):
        self.assert_rejected('def f(out: ConsoleCap) -> Int = { io.println(out, "effect"); yield 1; };', "effect_escape")
        good = 'def f(out: ConsoleCap) -> Effect(IO, Int) = { io.println(out, "effect"); yield 1; };'
        self.assertEqual(check(module(good))["status"], "checked")

    def test_structural_record_and_list_types(self):
        good = 'def R: Type = types.record(#{x: Int, y: Text}); def f: R = #{x: 1, y: "ok"};'
        self.assertEqual(check(module(good))["status"], "checked")
        self.assert_rejected(good.replace('y: "ok"', 'y: 2'), "type_mismatch")
        self.assert_rejected('def f: List(Int) = [1, "two"];', "type_mismatch")

    def test_parameter_kind_checks(self):
        self.assert_rejected('def f(x: List(3)) -> Int = 1;', "type_parameter_kind")
        self.assert_rejected('def f(n: Text, x: Buffer(U8, n)) -> Int = 1;', "type_index_kind")
        self.assert_rejected('def f(q: QReg(0)) -> QReg(0) = q;', "type_index_range")

    def test_mutual_recursion_cannot_claim_totality(self):
        result = check(module('def f(x: Int) -> Int = g(x); def g(x: Int) -> Int = f(x);'))
        self.assertEqual(result["status"], "unsupported")
        self.assertTrue(all(item["status"] == "unsupported" for item in result["definitions"]))

    def test_unchecked_dependency_does_not_gain_trust(self):
        result = check(module('def a: Int = prob.sample(dist.normal(0.0, 1.0)); def b: Int = a;'))
        self.assertNotEqual(result["definitions"][-1]["status"], "checked")

    def test_shadowing_is_lexical(self):
        body = 'def f(f: Int) -> Int = { let x = f; yield { let f = 7; yield int.add(x, f); }; };'
        self.assertEqual(check(module(body))["status"], "checked")

    def test_quantum_duplicate_discard_and_wrong_index(self):
        self.assert_rejected('def f(q: QReg(2)) -> Pair(QReg(2), QReg(2)) = (q, q);', "linear_reuse")
        self.assert_rejected('def f(q: QReg(2)) -> Int = 1;', "linear_discard")
        self.assert_rejected('def f(q: QReg(2)) -> QReg(2) = quantum.h(q, 3);', "quantum_index")
        self.assert_rejected('def f(q: QReg(2)) -> QReg(2) = quantum.cx(q, 0, 0);', "quantum_alias")

    def test_quantum_alias_and_capture_are_rejected(self):
        self.assert_rejected('def f(q: QReg(2)) -> QReg(2) = { let r = q; quantum.h(q, 0); yield r; };', "linear_reuse")
        self.assert_rejected('def f(q: QReg(2)) -> Fn(QReg(2)) = fn() => q;', "linear_capture")
        self.assert_rejected('def q: QReg(2) = quantum.zero(2);', "linear_global")

    def test_owned_borrow_cannot_escape(self):
        self.assert_rejected('def f(b: Owned(Buffer(U8, 1))) -> Owned(Buffer(U8, 1)) = mem.with_mut(b, fn(v: MutView(Buffer(U8, 1))) => #{view: v});', "lifetime_escape")
        self.assert_rejected('def f(v: MutView(Buffer(U8, 1))) -> MutView(Buffer(U8, 1)) = v;', "lifetime_escape")


class GraphTests(unittest.TestCase):
    def test_all_originals_deterministic_six_tag_lowering(self):
        for path in sorted(EXAMPLES.glob("*.u")):
            with self.subTest(example=path.name):
                source = parse(path.read_text())
                artifact = elaborate(source)
                self.assertEqual(artifact, elaborate(source))
                self.assertTrue({node["tag"] for node in nodes(artifact["graph"])} <= KERNEL_TAGS)
                self.assertFalse(artifact["checked"])

    def test_kernel_nodes_are_immutable_snapshots(self):
        node = KernelNode.make("wire", binder="b0", permutation=[0])
        with self.assertRaises(FrozenInstanceError):
            node.tag = "gen"
        snapshot = node.to_data()
        snapshot["permutation"].append(1)
        self.assertEqual(node.to_data()["permutation"], [0])

    def test_alpha_renaming_preserves_structure_and_binding_shadowing(self):
        left = module('def f(x: Int) -> Int = { let y = x; yield (fn(x: Int) => int.add(x, y))(3); };')
        right = module('def f(a: Int) -> Int = { let b = a; yield (fn(c: Int) => int.add(c, b))(3); };')
        a, b = elaborate(left), elaborate(right)
        self.assertEqual(a["structural_digest"], b["structural_digest"])
        self.assertEqual(a["semantic_digest"], b["semantic_digest"])
        self.assertNotEqual(a["bindings"], b["bindings"])

    def test_type_resource_and_operator_name_not_conflated(self):
        square = elaborate(module('def f(x: Int) -> Int = int.mul(x, x);'))
        witness = next(item for item in square["usage_requirements"] if item["capability"] == "Copy")
        self.assertEqual(witness["status"], "checked-classical-type-rule")
        q = elaborate(module('def f(q: QReg(2)) -> Pair(QReg(2), QReg(2)) = (q, q);'))
        self.assertTrue(any(item["kind"] == "resource-usage" for item in q["obligations"]))

    def test_derived_rule_change_invalidates_identity(self):
        source = parse((EXAMPLES / "09_sql_join.u").read_text())
        original = elaborate(source)
        old = DERIVED_RULES["sql.inner_join"]
        try:
            DERIVED_RULES["sql.inner_join"] = old + "; changed-observation-rule"
            changed = elaborate(source)
        finally:
            DERIVED_RULES["sql.inner_join"] = old
        self.assertNotEqual(original["semantic_digest"], changed["semantic_digest"])
        descriptor = next(item for item in original["operators"] if item["name"] == "sql.inner_join")
        self.assertEqual(descriptor["expansion_status"], "specified-unverified")

    def test_probability_search_mix_requires_admitted_order(self):
        source = module('def f: Int = prob.sample(logic.query("x", fn() => 1));')
        artifact = elaborate(source)
        self.assertFalse(artifact["profiles"][0]["admitted"])
        self.assertTrue(artifact["obligations"])

    def test_cdc_profile_binds_actual_numeric_contract(self):
        from u.cdc import NUMERIC_PROFILE
        source = parse((EXAMPLES / "18_cdc.u").read_text())
        profiles = elaborate(source)["profiles"]
        self.assertTrue(any(profile.get("numeric_contract") == NUMERIC_PROFILE for profile in profiles))

    def test_all_originals_reconstruct_from_graph_without_source_payload(self):
        for path in sorted(EXAMPLES.glob("*.u")):
            with self.subTest(example=path.name):
                artifact = elaborate(parse(path.read_text()))
                reconstructed = reconstruct(artifact)
                self.assertEqual(reconstructed["source"], "")
                self.assertEqual(elaborate(reconstructed)["structural_digest"], artifact["structural_digest"])

    def test_actual_graph_execution_classical_examples(self):
        cases = [("02_arithmetic.u", "square", [13], 169),
                 ("03_fibonacci.u", "fib", [15], 610),
                 ("04_map_filter_reduce.u", "sum_positive_squares", [[-2, 1, 3, 0]], 10)]
        for filename, entry, args, expected in cases:
            artifact = elaborate(parse((EXAMPLES / filename).read_text()))
            self.assertEqual(run_graph(artifact, entry, args), expected)

    def test_tampered_graph_or_dependencies_are_rejected(self):
        artifact = elaborate(module('def f(x: Int) -> Int = int.mul(x, x);'))
        for field in ("structural_digest", "semantic_digest"):
            changed = deepcopy(artifact)
            changed[field] = "forged"
            with self.assertRaises(GraphError):
                reconstruct(changed)
        changed = deepcopy(artifact)
        next(node for node in nodes(changed["graph"]) if node.get("operator_id") == "int.mul")["signature_digest"] = "forged"
        with self.assertRaises(GraphError):
            reconstruct(changed)

    def test_graph_rejects_dangling_binder(self):
        artifact = elaborate(module('def f(x: Int) -> Int = x;'))
        next(node for node in nodes(artifact["graph"]) if node["tag"] == "wire")["binder"] = "unbound"
        with self.assertRaises(GraphError):
            reconstruct(artifact)

    def test_graph_cannot_inject_nonfinite_source_literal(self):
        source = module('def x: F64 = 0.0;')
        source["definitions"][0]["body"]["value"] = float("inf")
        with self.assertRaises(GraphError):
            reconstruct(elaborate(source))


if __name__ == "__main__":
    unittest.main()
