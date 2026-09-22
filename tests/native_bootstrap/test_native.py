"""Independent native ABI checks. The produced programs never load Python."""
from __future__ import annotations

import importlib.util
import json
import os
from pathlib import Path
import random
import platform
import hashlib
import http.server
import threading
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('u_seed_test', ROOT / 'bootstrap/seed.py')
SEED = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SEED)


class NativeBridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cc = shutil.which('cc')
        if not cls.cc:
            raise unittest.SkipTest('C11 compiler unavailable')
        cls.temporary = tempfile.TemporaryDirectory(prefix='u-native-tests-')
        cls.directory = Path(cls.temporary.name)
        cls.runtime = cls.directory / 'runtime.o'
        subprocess.run([cls.cc, '-std=c11', '-O1', '-ffp-contract=off', '-c',
                        str(ROOT / 'native/runtime.c'), '-o', str(cls.runtime)], check=True,
                       capture_output=True, text=True)
        cls.sequence = 0
        cls.libraries = ['-lm'] + ([] if platform.system() == 'Darwin' else ['-lcrypto'])

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def program(self, source, *, environment=None, args=(), input_text=None):
        type(self).sequence += 1
        stem = self.directory / f'case-{self.sequence}'
        src = stem.with_suffix('.c')
        src.write_text(SEED.Seed().emit(SEED.parse('u "etellis.u/0.1";\n' + source)))
        subprocess.run([self.cc, '-std=c11', '-O1', '-ffp-contract=off', '-I',
                        str(ROOT / 'native'), str(src), str(self.runtime), *self.libraries, '-o', str(stem)],
                       check=True, capture_output=True, text=True)
        env = dict(os.environ)
        # No host interpreter or executable search path is available to the program.
        env['PATH'] = str(self.directory / 'no-interpreters')
        env.pop('U_NATIVE_ALLOW', None)
        env.pop('U_ARGS_JSON', None)
        env.pop('U_NATIVE_STEPS', None)
        if environment:
            env.update(environment)
        return subprocess.run([str(stem), *args], env=env, capture_output=True, text=True, input=input_text, timeout=30)

    def result(self, expression, expected, definitions=''):
        result = self.program(definitions + '\ndef main() -> Value = ' + expression + ';')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout), expected)

    def fault(self, expression, code):
        result = self.program('def main() -> Value = ' + expression + ';')
        self.assertEqual(result.returncode, 70, result.stdout + result.stderr)
        self.assertIn('U_NATIVE_ERROR ' + code, result.stderr)

    def test_integer_arithmetic_is_arbitrary_precision(self):
        rng = random.Random(159)
        calls, expected = [], []
        for _ in range(20):
            a = rng.randrange(-(10**120), 10**120)
            b = rng.randrange(-(10**55), 10**55) or 1
            quotient = abs(a) // abs(b) * (-1 if (a < 0) != (b < 0) else 1)
            for operation, value in [('add', a+b), ('sub', a-b), ('mul', a*b),
                                     ('div', quotient), ('mod', a-quotient*b)]:
                calls.append(f'int.{operation}({a},{b})')
                expected.append(value)
        self.result('[' + ','.join(calls) + ']', expected)

    def test_exact_zero_and_natural_boundaries(self):
        self.result('[nat.zero(),nat.succ(999999999999999999999999),nat.ceil_div(100,3),int.div(-7,3),int.mod(-7,3)]',
                    [0,1000000000000000000000000,34,-2,-1])

    def test_direct_lexical_closures_and_scope_shadowing(self):
        self.result('{ let x = 10; let f = fn(y: Int) => int.add(x,y); yield (f(3), { let x = 99; yield f(x); }); }', [13,109])

    def test_tuple_member_projections(self):
        self.result('{let pair=(11,29);yield (pair.first,pair.second,tuple.length(pair));}',[11,29,2])

    def test_namespaces_can_be_owned_by_u(self):
        self.result('int.add(2,3)', 6,
                    'def custom(a: Int,b: Int) -> Int = native.int_mul(a,b); def int: Value = #{add:custom};')

    def test_evaluation_order_is_left_to_right(self):
        self.result('{ let c = cell.new(0); let next = fn() => { cell.set(c,int.add(cell.get(c),1)); yield cell.get(c); }; yield (next(),next(),next()); }', [1,2,3])

    def test_collections_and_structural_recursion(self):
        self.result('(list.map([1,2,3],fn(x:Int)=>int.mul(x,2)),list.filter([1,2,3],fn(x:Int)=>int.gt(x,1)),list.fold_left([1,2,3],0,fn(a:Int,b:Int)=>int.add(a,b)),nat.rec(10,0,fn(i:Nat,a:Int)=>int.add(a,i)))',
                    [[2,4,6],[2,3],6,45])

    def test_buffer_snapshot_and_table_update(self):
        self.result('{ let b = buffer.new(); buffer.push(b,3); let snapshot = buffer.freeze(b); buffer.set(b,0,8); let t = table.new(); table.set(t,"x",7); table.set(t,"x",9); yield (snapshot,buffer.freeze(b),table.get(t,"x",0),table.get(t,"missing",11),table.has(t,"missing")); }',
                    [[3],[8],9,11,False])

    def test_record_construction_and_update(self):
        self.result('{ let r = record.make(["x","y"],[1,2]); yield (record.put(r,"x",9),record.put(r,"z",3),record.keys(r),record.has(r,"z")); }',
                    [{'x':9,'y':2},{'x':1,'y':2,'z':3},['x','y'],False])

    def test_unicode_scalars_and_embedded_nul(self):
        self.result('(text.length("a🙂é"),bytes.length("a🙂é"),text.at("a🙂é",1),text.slice("a🙂é",1,3),text.code("🙂"),text.from_code(128578),text.at("abc",3),text.concat("a\\u0000","b"))',
                    [3,7,'🙂','🙂é',128578,'🙂','','a\0b'])

    def test_json_codec_preserves_large_ints_and_scalar_escapes(self):
        self.result('json.parse("{\\\"x\\\":123456789012345678901234567890,\\\"s\\\":\\\"\\\\ud83d\\\\ude42\\\",\\\"n\\\":null}")',
                    {'x':123456789012345678901234567890,'s':'🙂','n':None})

    def test_explicit_fma_is_distinct_from_split_rounding(self):
        self.result('(math.fma(1.0000000000000002,0.9999999999999998,-1.0),f64.add(f64.mul(1.0000000000000002,0.9999999999999998),-1.0),f32.from_real(1.00000001))',
                    [-2**-104,0.0,1.0])

    def test_tail_fixed_point_has_no_c_stack_growth(self):
        self.result('partial.value(core.fix_partial(fn(again:Value,n:Int)=>value.select(int.lt(n,20000),fn()=>again(int.add(n,1)),fn()=>partial.done(n))(),0))',20000)

    def test_non_tail_continuation_is_rejected(self):
        self.fault('core.fix_partial(fn(again:Value,n:Int)=>partial.done(again(n)),0)', 'CONTINUATION_CONTEXT')

    def test_continuation_cannot_be_hidden_in_a_binding(self):
        self.fault('core.fix_partial(fn(again:Value,n:Int)=>{let ignored=again(n);yield partial.done(n);},0)', 'CONTINUATION_CONTEXT')

    def test_arity_type_and_bounds_faults(self):
        for expression,code in [('int.add(1)','ARITY'),('int.add(true,2)','TYPE'),
                                ('list.get([1],9)','INDEX'),('nat.add(-1,2)','NATURAL'),
                                ('int.div(1,0)','DIVISION_ZERO'),('text.from_code(55296)','UNICODE_SCALAR')]:
            with self.subTest(expression=expression):
                self.fault(expression,code)

    def test_bad_json_and_duplicate_records_are_rejected(self):
        for expression,code in [('json.parse("01")','JSON'),
                                ('record.make(["x","x"],[1,2])','DUPLICATE_FIELD'),
                                ('json.parse("\\\"\\\\ud800\\\"")','JSON')]:
            with self.subTest(expression=expression):
                self.fault(expression,code)

    def test_platform_read_requires_explicit_capability(self):
        self.fault('sys.read("/a/file/that/is/not/opened")', 'CAPABILITY')

    def test_native_file_io_only_with_granted_capabilities(self):
        path = self.directory / 'a filename with spaces.txt'
        source = 'def main() -> Text = { sys.write(' + json.dumps(str(path)) + ',"hello🙂"); yield sys.read(' + json.dumps(str(path)) + '); };'
        result = self.program(source,environment={'U_NATIVE_ALLOW':'read,write'})
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),'hello🙂')

    def test_process_invocation_uses_real_argv(self):
        source = 'def main() -> Int = sys.exec(["/usr/bin/printf","%s","a; echo not-a-shell"]);'
        result = self.program(source,environment={'U_NATIVE_ALLOW':'exec'})
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(result.stdout,'a; echo not-a-shell0\n')

    def test_resources_are_opaque_and_consumption_is_checked(self):
        self.result('{ let r = resource.new("token",7); yield (value.kind(r),resource.take(r,"token")); }',['resource',7])
        self.fault('{ let r = resource.new("token",7); resource.take(r,"token"); yield resource.peek(r,"token"); }','RESOURCE_CONSUMED')
        self.fault('resource.peek(#{kind:"token",payload:7},"token")','TYPE')

    def test_closure_description_is_data_not_execution(self):
        self.result('{ let f = fn(x:Int)=>int.add(x,1); yield (value.arity(f),value.describe(f).params, value.call(f,[9])); }',
                    [1,[{'name':'x','type':{'kind':'name','name':'Int'}}],10])

    def test_entry_json_arguments(self):
        result=self.program('def main(x:Int,y:Int)->Int=int.mul(x,y);', args=('[12345678901234567890,3]',))
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),37037036703703703670)

    def test_unit_entry_is_silent(self):
        result=self.program('def main()->Unit=();')
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(result.stdout,'')

    def test_search_primitives_respect_byte_offsets(self):
        self.result('(bytes.find("🙂abcabc","abc",1),bytes.find("abc","z",0),bytes.find_any("🙂ab",[97,98],0))',[4,-1,4])

    def test_bounded_allocation_fault_is_explicit(self):
        source='def main()->Unit={let b=buffer.new();control.while(fn()=>true,fn()=>buffer.push(b,[100000000000000000000000000]));yield ();};'
        result=self.program(source,environment={'U_NATIVE_MEMORY_MB':'1'})
        self.assertEqual(result.returncode,70)
        self.assertIn('U_NATIVE_ERROR MEMORY_LIMIT',result.stderr)

    def test_child_process_does_not_inherit_compiler_capabilities(self):
        result=self.program('def main()->Int=sys.exec(["/usr/bin/env"]);', environment={'U_NATIVE_ALLOW':'read,write,exec'})
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertNotIn('U_NATIVE_ALLOW=',result.stdout)

    def test_runtime_under_address_and_undefined_sanitizers(self):
        source='u "etellis.u/0.1"; def main()->Value={let b=buffer.new();buffer.push(b,int.div(123456789012345678901234567890,999999999999));let t=table.new();table.set(t,"x",buffer.freeze(b));yield (table.get(t,"x",[]),json.parse("[1,2,3]"),text.slice("🙂abcd",0,2));};'
        c=self.directory/'sanitized.c'
        binary=self.directory/'sanitized'
        c.write_text(SEED.Seed().emit(SEED.parse(source)))
        built=subprocess.run([self.cc,'-std=c11','-O1','-g','-ffp-contract=off','-fsanitize=address,undefined',
                              '-I',str(ROOT/'native'),str(c),str(ROOT/'native/runtime.c'),*self.libraries,'-o',str(binary)],
                             capture_output=True,text=True)
        if built.returncode and ('unsupported' in built.stderr or 'not supported' in built.stderr):
            self.skipTest('compiler sanitizer support unavailable')
        self.assertEqual(built.returncode,0,built.stderr)
        env=dict(os.environ,UBSAN_OPTIONS='halt_on_error=1')
        result=subprocess.run([str(binary)],env=env,capture_output=True,text=True,timeout=30)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),[[123456789012469135],[1,2,3],'🙂a'])

    def test_sha256_uses_vetted_system_implementation(self):
        self.result('(crypto.sha256(""),crypto.sha256("abc"),crypto.sha256("a\\u0000b"))',[
            'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
            'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad',
            hashlib.sha256(b'a\0b').hexdigest()])

    def test_binary64_bits_preserve_signed_zero(self):
        self.result('(f64.bits(0.0),f64.bits(-0.0),f64.bits(1.0),f64.bits(-1.0))',
                    ['0000000000000000','8000000000000000','3ff0000000000000','bff0000000000000'])

    def test_isolated_workers_preserve_types_and_private_cells(self):
        self.result('{let c=cell.new(1);let p=process.spawn(fn()=>{cell.set(c,9);yield (123456789012345678901234567890,[true],-0.0);});let out=process.wait(p,-1);yield (out.state,value.kind(out.value),value.kind(out.value.second),out.value,cell.get(c),f64.bits(tuple.get(out.value,2)));}',
                    ['completed','tuple','list',[123456789012345678901234567890,[True],-0.0],1,'8000000000000000'])

    def test_worker_failure_is_an_observable_outcome(self):
        result=self.program('def main()->Text={let p=process.spawn(fn()=>value.fail("worker-fixture"));yield process.wait(p,-1).state;};')
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),'failed')
        self.assertIn('worker-fixture',result.stderr)

    def test_worker_capture_cannot_duplicate_opaque_authority(self):
        result=self.program('def main()->Text={let r=resource.new("authority",7);let p=process.spawn(fn()=>resource.peek(r,"authority"));yield process.wait(p,-1).state;};')
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),'failed')
        self.assertIn('RESOURCE_PROCESS',result.stderr)

    def test_worker_result_cannot_transport_resource_handles(self):
        result=self.program('def main()->Text={let p=process.spawn(fn()=>resource.new("authority",7));yield process.wait(p,-1).state;};')
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),'failed')
        self.assertIn('TRANSPORT_CARRIER',result.stderr)

    def test_large_worker_result_does_not_deadlock(self):
        self.result('{let p=process.spawn(fn()=>text.join(list.map([1,2,3,4,5,6,7,8,9,10],fn(i:Int)=>text.join(list.map([1,2,3,4,5,6,7,8,9,10],fn(j:Int)=>text.join(list.map([1,2,3,4,5,6,7,8,9,10],fn(k:Int)=>"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"),"")),"")),""));yield text.length(process.wait(p,-1).value);}',78000)

    def test_shared_results_support_multiple_independent_readers(self):
        self.result('{let p=process.spawn(fn()=>{process.sleep(20);yield (1,[2,3]);});let reading=process.share(p);let a=process.spawn(fn()=>process.result(reading,-1).value);let b=process.spawn(fn()=>process.result(reading,-1).value);yield (process.wait(a,1000).value,process.wait(b,1000).value,process.wait(p,1000).value);}',
                    [[1,[2,3]],[1,[2,3]],[1,[2,3]]])

    def test_dead_worker_wakes_subscriber_without_owner_polling(self):
        self.result('{let p=process.spawn(fn()=>sys.exit(3));let r=process.share(p);let subscriber=process.spawn(fn()=>process.result(r,-1).state);yield process.wait(subscriber,1000).value;}', 'failed')

    def test_worker_cancellation_wakes_all_readers(self):
        self.result('{let p=process.spawn(fn()=>{process.sleep(5000);yield 7;});let r=process.share(p);let subscriber=process.spawn(fn()=>process.result(r,-1).state);process.cancel(p);yield process.wait(subscriber,1000).value;}', 'cancelled')

    def test_reading_right_does_not_grant_process_ownership(self):
        result=self.program('def main()->Text={let p=process.spawn(fn()=>{process.sleep(5000);yield 7;});let wrong=process.spawn(fn()=>process.cancel(p));let status=process.wait(wrong,1000).state;process.cancel(p);yield status;};')
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),'failed')
        self.assertIn('PROCESS_OWNER',result.stderr)

    def test_standard_streams_are_exact_and_report_eof(self):
        result=self.program('def main()->Value={let line=sys.stdin_line();let body=sys.stdin_read(5);let eof=sys.stdin_line();sys.stdout_write("prefix");yield (line,body,eof);};',input_text='hello\r\nabcde',environment={'U_NATIVE_ALLOW':'console'})
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(result.stdout,'prefix["hello","abcde",null]\n')

    def test_worker_explicit_grants_cannot_escalate(self):
        self.fault('process.spawn_with_caps(fn()=>sys.has_cap("network"),["network"])','CAPABILITY')
        result=self.program('def main()->Value={let worker=process.spawn_with_caps(fn()=>(sys.has_cap("network"),sys.has_cap("read")),["network"]);yield process.wait(worker,-1).value;};',environment={'U_NATIVE_ALLOW':'network,read'})
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),[True,False])

    def test_console_and_environment_require_runtime_authority(self):
        self.fault('{let alias=sys;yield alias.print("should not print");}','CAPABILITY')
        self.fault('{let alias=sys;yield alias.getenv("U_TEST_PRIVATE_VALUE");}','CAPABILITY')
        result=self.program('def main()->Text=sys.getenv("U_TEST_PRIVATE_VALUE");',environment={'U_NATIVE_ALLOW':'env','U_TEST_PRIVATE_VALUE':'fixture-only'})
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),'fixture-only')

    def test_dispatch_budget_exhaustion_is_an_explicit_fault(self):
        result=self.program('def main()->Value=core.fix_partial(fn(again:Value,n:Int)=>again(int.add(n,1)),0);',environment={'U_NATIVE_STEPS':'30'})
        self.assertEqual(result.returncode,70,result.stdout+result.stderr)
        self.assertIn('BUDGET_EXHAUSTED',result.stderr)
        self.assertEqual(result.stdout,'')

    def test_worker_budget_exhaustion_cannot_be_reported_as_success(self):
        result=self.program('def main()->Value={let p=process.spawn(fn()=>core.fix_partial(fn(again:Value,n:Int)=>again(int.add(n,1)),0));yield process.wait(p,-1);};',environment={'U_NATIVE_STEPS':'50'})
        self.assertEqual(result.returncode,70,result.stdout+result.stderr)
        self.assertIn('BUDGET_EXHAUSTED',result.stderr)
        self.assertEqual(result.stdout,'')

    def test_unused_global_values_do_not_execute(self):
        result=self.program('def unused:Value=value.fail("must stay inert");def main()->Int=7;')
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),7)

    def test_lazy_globals_memoize_and_detect_cycles(self):
        self.result('{let first=value_once;let second=value_once;yield (first,second,cell.get(counter));}',[1,1,1],
                    'def counter:Value=cell.new(0);def value_once:Int={cell.set(counter,int.add(cell.get(counter),1));yield cell.get(counter);};')
        result=self.program('def x:Int=y;def y:Int=x;def main()->Int=x;')
        self.assertEqual(result.returncode,70,result.stderr)
        self.assertIn('INITIALIZATION_CYCLE',result.stderr)

    def test_later_block_bindings_do_not_change_prior_closure_capture(self):
        self.result('{let f=fn()=>x;let x=1;yield f();}',9,'def x:Int=9;')

    def test_try_call_restores_control_state_and_nests(self):
        self.result('{let first=value.try_call(fn()=>int.add(true,1),[]);let nested=value.try_call(fn()=>value.try_call(fn()=>value.fail("inner"),[]),[]);let ok=value.try_call(fn(x:Int)=>int.add(x,1),[8]);yield (first.ok,first.code,nested.ok,nested.value.ok,nested.value.message,ok.value);}',
                    [False,'TYPE',True,False,'inner',9])

    def test_many_caught_faults_do_not_accumulate_call_depth(self):
        self.result('nat.rec(3000,0,fn(i:Int,n:Int)=>{value.try_call(fn()=>value.fail("expected"),[]);yield int.add(n,1);})',3000)

    def test_caught_json_errors_release_transient_codec_storage(self):
        broken='"'+'a'*8192
        source='def broken:Text='+json.dumps(broken)+';def main()->Int=nat.rec(200,0,fn(i:Int,n:Int)=>{let out=value.try_call(fn()=>json.parse(broken),[]);yield int.add(n,value.select(out.ok,0,1));});'
        result=self.program(source,environment={'U_NATIVE_MEMORY_MB':'1'})
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),200)

    def test_try_call_restores_failed_lazy_initialization(self):
        self.result('{let failed=value.try_call(fn()=>retry,[]);yield (failed.ok,retry,cell.get(attempts));}',[False,7,2],
                    'def attempts:Value=cell.new(0);def retry:Int={cell.set(attempts,int.add(cell.get(attempts),1));yield value.select(int.eq(cell.get(attempts),1),fn()=>value.fail("first"),fn()=>7)();};')

    def test_try_call_does_not_roll_back_caller_resources(self):
        self.result('{let r=resource.new("fixture",3);let caught=value.try_call(fn()=>{resource.take(r,"fixture");yield value.fail("after consume");},[]);let access=value.try_call(fn()=>resource.peek(r,"fixture"),[]);yield (caught.ok,access.ok,access.code);}',[False,False,'RESOURCE_CONSUMED'])

    def test_try_call_cannot_catch_budget_or_continuation_traps(self):
        result=self.program('def main()->Value=value.try_call(fn()=>core.fix_partial(fn(again:Value,n:Int)=>again(n),0),[]);',environment={'U_NATIVE_STEPS':'30'})
        self.assertEqual(result.returncode,70,result.stderr)
        self.assertIn('BUDGET_EXHAUSTED',result.stderr)
        self.fault('value.try_call(fn()=>core.fix_partial(fn(again:Value,n:Int)=>partial.done(again(n)),0),[])','CONTINUATION_CONTEXT')

    def test_try_call_does_not_cross_worker_process_boundary(self):
        self.result('value.try_call(fn()=>{let p=process.spawn(fn()=>value.fail("child"));yield process.wait(p,-1).state;},[])',{'ok':True,'value':'failed'})

    def test_indexed_global_scope_preserves_forward_bindings(self):
        definitions='\n'.join(f'def name_{i}:Int={i};' for i in range(200))
        self.result('(name_0,name_31,name_99,name_199)',[0,31,99,199],definitions)

    def test_http_platform_transport_is_capability_gated(self):
        calls=[]
        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                calls.append(self.path)
                body='hello🙂'.encode()
                self.send_response(200)
                self.send_header('Content-Type','text/plain; charset=utf-8')
                self.send_header('Content-Length',str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            def log_message(self,*args):
                pass
        server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
        thread=threading.Thread(target=server.serve_forever,daemon=True)
        thread.start()
        try:
            url=f'http://127.0.0.1:{server.server_port}/data?literal=one;two'
            source='def main()->Value={let result=http.transport('+json.dumps(url)+');yield (result.status,bytes.from_hex(result.body_hex));};'
            denied=self.program(source)
            self.assertEqual(denied.returncode,70,denied.stderr)
            self.assertIn('CAPABILITY',denied.stderr)
            self.assertEqual(calls,[])
            allowed=self.program(source,environment={'U_NATIVE_ALLOW':'network'})
            self.assertEqual(allowed.returncode,0,allowed.stderr)
            self.assertEqual(json.loads(allowed.stdout),[200,'hello🙂'])
            self.assertEqual(calls,['/data?literal=one;two'])
        finally:
            server.shutdown()
            server.server_close()
            thread.join()

    def test_binary_file_hash_preserves_invalid_utf8_and_nul(self):
        target=self.directory/'binary-hash-fixture.bin'
        source='def main()->Text={sys.write_bytes_new('+json.dumps(str(target))+',[0,255,128,1,2,10]);yield crypto.sha256_file('+json.dumps(str(target))+');};'
        result=self.program(source,environment={'U_NATIVE_ALLOW':'read,write'})
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),hashlib.sha256(bytes([0,255,128,1,2,10])).hexdigest())
        self.assertEqual(target.read_bytes(),bytes([0,255,128,1,2,10]))

    def test_exclusive_text_write_does_not_replace_an_existing_file(self):
        target=self.directory/'exclusive-text.u'
        quoted=json.dumps(str(target))
        source='def main()->Value={sys.write_new('+quoted+',"original");let collision=value.try_call(fn()=>sys.write_new('+quoted+',"replacement"),[]);yield (collision.ok,collision.code,sys.read('+quoted+'));};'
        result=self.program(source,environment={'U_NATIVE_ALLOW':'read,write'})
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),[False,'IO_WRITE','original'])

    def test_binary_writer_checks_all_bytes_before_creation(self):
        target=self.directory/'invalid-byte.bin'
        source='def main()->Value=sys.write_bytes_new('+json.dumps(str(target))+',[1,256]);'
        result=self.program(source,environment={'U_NATIVE_ALLOW':'write'})
        self.assertEqual(result.returncode,70,result.stderr)
        self.assertIn('BYTE',result.stderr)
        self.assertFalse(target.exists())

    def test_one_level_filesystem_operations_and_physical_cwd(self):
        folder=self.directory/'filesystem-native'
        old=folder/'old.u'
        new=folder/'new.u'
        source='def main()->Value={sys.mkdir('+json.dumps(str(folder))+');sys.write_new('+json.dumps(str(old))+',"fixture");sys.rename('+json.dumps(str(old))+','+json.dumps(str(new))+');yield (sys.file_kind('+json.dumps(str(folder))+'),sys.file_kind('+json.dumps(str(old))+'),sys.list_dir('+json.dumps(str(folder))+'),sys.cwd());};'
        result=self.program(source,environment={'U_NATIVE_ALLOW':'read,write'})
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),['directory','missing',[{'name':'new.u','kind':'file'}],os.path.realpath(os.getcwd())])
        link=folder/'symbolic'
        link.symlink_to('new.u')
        result=self.program('def main()->Text=sys.file_kind('+json.dumps(str(link))+');',environment={'U_NATIVE_ALLOW':'read'})
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout),'symlink')

    def test_filesystem_helpers_require_explicit_capabilities(self):
        self.fault('sys.list_dir(".")','CAPABILITY')
        self.fault('crypto.sha256_file("/no-such-file")','CAPABILITY')
        self.fault('sys.cwd()','CAPABILITY')


if __name__ == '__main__':
    unittest.main()
