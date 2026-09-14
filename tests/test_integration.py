import contextlib
import io
import json
from pathlib import Path
import random
import shutil
import subprocess
import sys
import tempfile
import threading
import http.server
import unittest

from u import backend, packages
from u.cli import main
from u.evaluator import Evaluator
from u.source import parse
from u.lsp import serve

ROOT = Path(__file__).resolve().parents[1]


class IntegrationTests(unittest.TestCase):
    def command(self, *args):
        output=io.StringIO()
        with contextlib.redirect_stdout(output):
            code=main(list(args))
        return code,output.getvalue()

    def test_cli_arbitrary_precision_and_proof(self):
        code,text=self.command('run',str(ROOT/'examples/original/03_fibonacci.u'),'--entry','fib','--args','[100]')
        self.assertEqual(code,0,text)
        self.assertEqual(json.loads(text)['result'],354224848179261915075)
        code,text=self.command('prove',str(ROOT/'examples/original/14_proof.u'),'--entry','checked')
        self.assertEqual(code,0,text)
        self.assertEqual(json.loads(text)['result']['kind'],'ProofReceipt')
        code,text=self.command('prove',str(ROOT/'examples/original/02_arithmetic.u'),'--entry','square','--args','[2]')
        self.assertEqual(code,2,text)

    def test_receipt_roundtrip_and_changed_graph_export(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            code,text=self.command('lift',str(ROOT/'examples/original/03_fibonacci.u'))
            self.assertEqual(code,0,text)
            (root/'envelope.json').write_text(text)
            code,text=self.command('export',str(root/'envelope.json'),'--output',str(root/'out.u'))
            self.assertEqual(code,0,text)
            self.assertEqual((root/'out.u').read_bytes(),(ROOT/'examples/original/03_fibonacci.u').read_bytes())
            value=json.loads((root/'envelope.json').read_text()); value['graph']['version']='changed'
            (root/'envelope.json').write_text(json.dumps(value))
            code,text=self.command('export',str(root/'envelope.json'),'--output',str(root/'changed.u'))
            self.assertEqual(code,2,text)
            self.assertFalse((root/'changed.u').exists())

    def test_local_package_lock_install_tamper_and_no_scripts(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder); source=root/'source'; source.mkdir()
            (source/'U.toml').write_text('[package]\nname="sample"\nversion="1"\n[scripts]\ninstall="touch NEVER"\n')
            (source/'main.u').write_text('u "etellis.u/0.1"; def main: Int = 1;')
            packages.lock(source)
            result=packages.install(source,root/'installed')
            self.assertEqual(packages.verify(result['path'])['verdict'],'Done')
            self.assertFalse((source/'NEVER').exists())
            (Path(result['path'])/'main.u').write_text('changed')
            with self.assertRaises(ValueError): packages.verify(result['path'])

    def test_lsp_real_framing_and_diagnostics(self):
        messages=[{'jsonrpc':'2.0','id':1,'method':'initialize','params':{}},
                  {'jsonrpc':'2.0','method':'textDocument/didOpen','params':{'textDocument':{
                      'uri':'file:///scratch.u','text':'u "etellis.u/0.1"; def a: Nat = @;'}}},
                  {'jsonrpc':'2.0','id':2,'method':'shutdown'}, {'jsonrpc':'2.0','method':'exit'}]
        raw=b''
        for message in messages:
            encoded=json.dumps(message).encode(); raw+=f'Content-Length: {len(encoded)}\r\n\r\n'.encode()+encoded
        out=io.BytesIO()
        self.assertEqual(serve(io.BytesIO(raw),out),0)
        data=out.getvalue()
        self.assertIn(b'publishDiagnostics',data)
        self.assertIn(b'hoverProvider',data)
        self.assertIn(b'invalid_character',data)

    def test_actual_http_task_with_loopback_capability(self):
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                self.send_response(200); self.end_headers(); self.wfile.write(b'local U task')
            def log_message(self,*args): pass
        with http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler) as server:
            thread=threading.Thread(target=server.serve_forever,daemon=True); thread.start()
            try:
                e=Evaluator(parse((ROOT/'examples/original/06_async.u').read_text()),capabilities=['network'])
                result=e.run('fetch_text',[e.capabilities['network'],f'http://127.0.0.1:{server.server_port}/'])
                self.assertEqual(result.result(),'local U task')
            finally:
                server.shutdown(); thread.join(timeout=2)

    @unittest.skipUnless(shutil.which('cc') and shutil.which('node'),'C compiler and Node required for real native/WASM parity')
    def test_native_wasm_and_reference_parity_with_bounds(self):
        source='u "etellis.u/0.1"; use "u.standard/0.1"; def polynomial(x: Int, y: Int) -> Int = int.add(int.mul(x,x),int.sub(y,3));'
        module=parse(source)
        rng=random.Random(841)
        cases=[[rng.randrange(-1000,1001),rng.randrange(-1000,1001)] for _ in range(32)]
        cases += [[-1000,1000],[1000,-1000],[0,0]]
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder)
            backend.build(module,'polynomial',[[-1000,1000],[-1000,1000]],root/'native','native')
            backend.build(module,'polynomial',[[-1000,1000],[-1000,1000]],root/'module.wasm','wasm')
            script='const fs=require("fs"); const cases=JSON.parse(process.argv[2]); WebAssembly.instantiate(fs.readFileSync(process.argv[1])).then(({instance})=>{const f=instance.exports.polynomial;const out=cases.map(x=>f(...x.map(BigInt)).toString());let trapped=false;try{f(1001n,0n)}catch(e){trapped=true}console.log(JSON.stringify({out,trapped}));});'
            wasm=json.loads(subprocess.check_output(['node','-e',script,str(root/'module.wasm'),json.dumps(cases)],text=True))
            self.assertTrue(wasm['trapped'])
            e=Evaluator(module)
            for arguments,web_result in zip(cases,wasm['out']):
                native=int(subprocess.check_output([str(root/'native'),*map(str,arguments)],text=True))
                self.assertEqual(native,e.run('polynomial',arguments))
                self.assertEqual(native,int(web_result))
            self.assertNotEqual(subprocess.run([str(root/'native'),'1001','0']).returncode,0)
            with self.assertRaises(ValueError): backend.certify(module,'polynomial',[[-2**62,2**62],[0,0]])


if __name__=='__main__': unittest.main()
