"""Independent black-box tests. Every language operation executes in native U.

Python orchestrates subprocesses and compares observations; it is not imported
by the compiler, launcher, libraries, package tool or produced executables.
"""
import hashlib
import http.server
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import unittest

ROOT = Path(__file__).resolve().parents[1]
DRIVER = Path(os.environ.get("U_NATIVE_DRIVER", ROOT / "build/native/etellis-u-driver"))


class NativeCLI(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory(prefix="u-native-cli-")
        self.addCleanup(self.folder.cleanup)
        self.work = Path(self.folder.name)
        self.counter = 0
        self.env = {**os.environ, "U_ROOT": str(ROOT), "U_NATIVE_WORK": str(self.work),
                    "U_NATIVE_ALLOW": "read,write,exec,console,env",
                    "U_NATIVE_LINK_CRYPTO": "0" if os.uname().sysname == "Darwin" else "1"}
        self.env.pop("U_NATIVE_STEPS", None)
        self.env.pop("U_ARGS_JSON", None)
        self.env.pop("U_USER_ARGS_JSON", None)

    def command(self, *args, ok=True, input=None):
        result = subprocess.run([str(DRIVER), *map(str, args)], env=self.env,
                                input=input, text=True, capture_output=True, timeout=120)
        if ok:
            self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        else:
            self.assertNotEqual(result.returncode, 0, result.stdout)
        return result

    def source(self, source):
        self.counter += 1
        path = self.work / f"source-{self.counter}.u"
        path.write_text('u "etellis.u/0.1"; use "u.standard/0.1";\n' + source)
        return path

    def run_source(self, source, *, args=(), entry="main", caps=(), command="run", ok=True):
        path = self.source(source)
        result = self.command(command, path, "--entry", entry, "--args", json.dumps(args),
                              *[part for cap in caps for part in ("--cap", cap)], ok=ok)
        return json.loads(result.stdout) if ok else result.stderr

    def original(self, name, entry, args=(), command="run", caps=()):
        result = self.command(command, ROOT / "examples/original" / name, "--entry", entry,
                              "--args", json.dumps(args),
                              *[part for cap in caps for part in ("--cap", cap)])
        return json.loads(result.stdout)

    def test_exact_integers_and_original_collection_program(self):
        self.assertEqual(self.original("03_fibonacci.u", "fib", [100]), 354224848179261915075)
        self.assertEqual(self.original("02_arithmetic.u", "square", [12345678901234567890]),
                         152415787532388367501905199875019052100)
        self.assertEqual(self.original("04_map_filter_reduce.u", "sum_positive_squares", [[-4, 2, 3, 0]]), 13)

    def test_native_build_is_standalone_and_binds_actual_bytes(self):
        source = self.source('def square(x: Int) -> Int = int.mul(x,x);')
        target = self.work / "standalone"
        receipt = json.loads(self.command("build", source, "--entry", "square", "--output", target).stdout)
        self.assertFalse(receipt["python_required"])
        self.assertEqual(receipt["compiler_identity"], hashlib.sha256(DRIVER.read_bytes()).hexdigest())
        self.assertEqual(receipt["executable_identity"], hashlib.sha256(target.read_bytes()).hexdigest())
        standalone = subprocess.check_output([str(target), "[123]"], env={"PATH": "/nonexistent"}, text=True)
        self.assertEqual(json.loads(standalone), 15129)

    def test_scope_and_lazy_global_initialization(self):
        source = '''def x: Int = 9;
def lazy: Int = int.div(1,0);
def main() -> Value = { let f = fn() => x; let x = 1; yield [f(),x]; };'''
        self.assertEqual(self.run_source(source), [9, 1])

    def test_original_induction_and_counterfeit_receipt(self):
        proof = self.original("14_proof.u", "checked", command="prove")
        self.assertEqual(proof["verdict"], "Done")
        self.assertEqual(proof["checker"], "etellis.u/native-total-nat-pi-eq/0.2")
        self.assertEqual(proof["assumptions"], [])
        self.assertIn("proof", self.run_source('def main: Value = #{kind:"ProofReceipt",verdict:"Done"};',
                                              command="prove", ok=False).lower())

    def test_bridge_and_capability_rejections(self):
        for expression in ('native.sys_read("/etc/hosts")', 'resource.new("CheckedProof",#{})', 'cell.new(1)'):
            with self.subTest(expression=expression):
                self.assertIn("unsafe_bridge", self.run_source(f'def main() -> Value = {expression};', ok=False))
        self.assertIn("capability_required", self.run_source('def main() -> Unit = sys.print("no");', ok=False))
        self.assertIn("unbound_name", self.run_source('def main() -> Value = { let host = sys; yield host.read("/etc/hosts"); };', ok=False))
        self.assertIn("private_library_name", self.run_source('def main() -> Value = u_proof_require(1);', ok=False))

    def test_budget_and_closed_primitive_names(self):
        path = self.source('def main() -> Int = core.fix_partial(fn(again:Value,x:Int)=>again(x),0);')
        failed = self.command("run", path, "--budget", "1000", ok=False)
        self.assertIn("BUDGET_EXHAUSTED", failed.stderr)
        for expression in ("int.this_does_not_exist(1)", "int.add.typo(1,2)"):
            self.command("check", self.source(f"def main() -> Value = {expression};"), ok=False)

    def test_duration_dependency_and_command_option_refusals(self):
        self.assertEqual(self.run_source('def main()->Value=duration.seconds(1.0);'), 1.0)
        source = self.source('def main()->Int=7;')
        for args in [("check", source, "--definitely-unknown"), ("run", source, "--entyr", "other"),
                     ("build", source, "--targte", "wasm"), ("fmt", source, "--write", "extra"), ("--version", "extra")]:
            self.command(*args, ok=False)

    def test_original_relational_query_and_sql_bags(self):
        query = self.original("08_logic.u", "query")
        self.assertEqual(query["answers"], [{"who": "Cara"}])
        users = [{"id": 1, "name": "Ada"}, {"id": 1, "name": "Ada"}, {"id": None, "name": "Null"}]
        orders = [{"user_id": 1, "total": 3.5}, {"user_id": None, "total": 8.0}]
        self.assertEqual(self.original("09_sql_join.u", "join_orders", [users, orders]),
                         [{"name": "Ada", "total": 3.5}] * 2)

    def test_original_array_contraction_and_shape_refusal(self):
        result = self.original("10_arrays.u", "multiply", [[[1,2,3],[4,5,6]], [[1,2,3,4],[5,6,7,8],[9,10,11,12]]])
        self.assertEqual(result, [[38,44,50,56],[83,98,113,128]])
        self.run_source('def main()->Value=array.contract([[1,2]],[[1,2]],[1],[0],"f64.strict_left_fold");', ok=False)

    def test_staged_single_evaluation_and_stage_error_recovery(self):
        source = (ROOT / "examples/original/07_macro.u").read_text()
        source += '\ndef main()->Value=syntax.realize(twice(syntax.quote(fn()=>21)));'
        self.assertEqual(self.run_source(source), 42)
        error = self.run_source('def main()->Value=syntax.splice(syntax.quote(fn()=>1));', ok=False)
        self.assertIn("active quotation", error)
        restored = self.run_source('''def main()->Value={
 let failed=value.try_call(fn()=>syntax.realize(syntax.quote(fn()=>value.fail("expected"))),[]);
 let outside=value.try_call(fn()=>syntax.splice(syntax.quote(fn()=>1)),[]);
 yield [failed.ok,outside.ok];};''')
        self.assertEqual(restored, [False, False])

    def test_coherent_state_instrument_and_linear_consumption(self):
        source = (ROOT / "examples/original/17_quantum.u").read_text()
        source += '\ndef main()->Value=quantum.simulate(experiment);'
        result = self.run_source(source)
        self.assertFalse(result["physical_device"])
        self.assertEqual([branch["bits"] for branch in result["branches"]], ["00", "11"])
        for branch in result["branches"]:
            self.assertAlmostEqual(branch["probability"], 0.5)
        failed = self.run_source('''def main()->Value={let q=quantum.zero(2);
 let next=quantum.h(q,0); yield quantum.h(q,0);};''', ok=False)
        self.assertIn("RESOURCE_CONSUMED", failed)

    def test_cdc_preserves_extra_fields_and_held_latches(self):
        result = self.run_source('''def main()->Value={
 let s=cdc.make_state(#{fields:[#{name:"f",cone:"retain"}],modules:[#{name:"m",field:"f",pair:"keep"}],
 cells:[#{name:"c",module:"m",theta:3.141592653589793,amplitude:2.5,has_latch:true,latch:1}],channels:[]});
 let r=cdc.commit(s,"m");yield r;};''')
        self.assertEqual(result["verdict"], "Held")
        self.assertEqual(result["state"]["fields"][0]["cone"], "retain")
        self.assertEqual(result["state"]["modules"][0]["pair"], "keep")
        self.assertEqual(result["state"]["cells"][0]["amplitude"], 2.5)
        self.assertEqual(result["state"]["cells"][0]["latch"], 1)

    def test_native_http_task_with_real_loopback_server(self):
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                self.send_response(200 if self.path == "/ok" else 404)
                self.send_header("Content-Type", "text/plain; charset=utf-8")
                self.end_headers()
                self.wfile.write("U λ\x00response".encode())
            def log_message(self, *args):
                pass
        source = '''def main(url: Text)->Text=async.await(
 async.then(http.get(http.capability(),url),http.body_text));'''
        with http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server:
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            try:
                url = f"http://127.0.0.1:{server.server_port}"
                self.assertEqual(self.run_source(source, args=[url + "/ok"], caps=["network"]), "U λ\x00response")
                self.run_source(source, args=[url + "/missing"], caps=["network"], ok=False)
                self.run_source(source, args=[url + "/ok"], ok=False)
            finally:
                server.shutdown(); thread.join(timeout=2)

    def test_all_originals_format_and_lift_export_exactly(self):
        for path in sorted((ROOT / "examples/original").glob("*.u")):
            with self.subTest(path=path.name):
                self.command("parse", path)
                formatted = self.command("fmt", path).stdout
                formatted_path = self.work / "formatted.u"
                formatted_path.write_text(formatted)
                self.assertEqual(self.command("fmt", formatted_path).stdout, formatted)
                graph = self.command("lift", path).stdout
                envelope = self.work / "graph.json"; envelope.write_text(graph)
                exported = self.work / f"exported-{path.stem}.u"
                self.command("export", envelope, "--output", exported)
                self.assertEqual(exported.read_bytes(), path.read_bytes())

    def test_export_rejects_poisoned_origin(self):
        path = self.source('def main: Int = 1;')
        graph = json.loads(self.command("lift", path).stdout)
        # Locate source provenance without assuming it authenticates semantics.
        self.assertIn("origin", graph)
        original = json.dumps(graph)
        poisoned = original.replace('def main: Int = 1;', 'def main: Int = 2;')
        self.assertNotEqual(original, poisoned)
        envelope = self.work / "poisoned.json"; envelope.write_text(poisoned)
        self.command("export", envelope, "--output", self.work / "bad.u", ok=False)
        self.assertFalse((self.work / "bad.u").exists())

    def test_export_preserves_existing_target(self):
        source = self.source('def main: Int = 1;')
        envelope = self.work / "valid-graph.json"
        envelope.write_text(self.command("lift", source).stdout)
        existing = self.work / "existing.u"
        existing.write_text("unrelated user source\n")
        self.command("export", envelope, "--output", existing, ok=False)
        self.assertEqual(existing.read_text(), "unrelated user source\n")

    def test_language_server_framing_diagnostics_recovery_and_formatting(self):
        messages = [[], None, 5, {},
            {"jsonrpc":"2.0","id":1,"method":"initialize","params":{}},
            {"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///scratch.u","text":"def a: Nat = @;"}}},
            {"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///scratch.u"},"contentChanges":[{"text":'u "etellis.u/0.1"; def main()->Int=int.add(1,2);'}]}},
            {"jsonrpc":"2.0","id":2,"method":"textDocument/formatting","params":{"textDocument":{"uri":"file:///scratch.u"},"options":{"tabSize":2,"insertSpaces":True}}},
            {"jsonrpc":"2.0","id":3,"method":"shutdown"}, {"jsonrpc":"2.0","method":"exit"}]
        raw = ""
        for message in messages:
            body = json.dumps(message)
            raw += f"Content-Length: {len(body.encode())}\r\n\r\n{body}"
        result = subprocess.run([str(DRIVER), "lsp"], env=self.env, input=raw.encode(), capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        remaining = result.stdout; replies = []
        while remaining:
            header, body = remaining.split(b"\r\n\r\n", 1)
            size = int(header.split(b":", 1)[1])
            replies.append(json.loads(body[:size]))
            remaining = body[size:]
        self.assertEqual(len(replies), 9)
        for reply in replies[:4]:
            self.assertEqual(reply["error"]["code"], -32600)
        self.assertEqual(replies[5]["method"], "textDocument/publishDiagnostics")
        self.assertIn("invalid_character", replies[5]["params"]["diagnostics"][0]["message"])
        self.assertEqual(replies[6]["params"]["diagnostics"], [])
        self.assertIn("newText", replies[7]["result"][0])


if __name__ == "__main__":
    unittest.main()
