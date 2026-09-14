"""Fresh, scoped verification and source/operator inventory; no historical test inflation."""
from datetime import datetime, timezone
import hashlib
import inspect
import io
import json
from pathlib import Path
import platform
import subprocess
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
from u.source import parse, format_source
from u.checker import check
from u.graph import elaborate
from u.graph_execution import reconstruct
from u.evaluator import Evaluator
from u.evidence import digest


class Results(unittest.TextTestResult):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs); self.records=[]
    def addSuccess(self,test):
        super().addSuccess(test); self.records.append({'test':test.id(),'verdict':'passed'})
    def addFailure(self,test,err):
        super().addFailure(test,err); self.records.append({'test':test.id(),'verdict':'failed'})
    def addError(self,test,err):
        super().addError(test,err); self.records.append({'test':test.id(),'verdict':'error'})
    def addSkip(self,test,reason):
        super().addSkip(test,reason); self.records.append({'test':test.id(),'verdict':'skipped','reason':reason})


def main():
    artifacts=ROOT/'artifacts'; artifacts.mkdir(exist_ok=True)
    preserved=json.loads((ROOT/'provenance/PRESERVATION.json').read_text())
    preservation=[]
    for record in preserved['files']:
        file=ROOT/'provenance/source'/record['name']
        actual=hashlib.sha256(file.read_bytes()).hexdigest()
        assert actual==record['sha256'],file
        preservation.append({'file':str(file.relative_to(ROOT)),'sha256':actual,'byte_exact':True})
    examples=[]
    for record in preserved['examples']:
        path=ROOT/'examples/original'/record['name']
        source=path.read_text()
        assert hashlib.sha256(path.read_bytes()).hexdigest()==record['receiptSha256'],path
        module=parse(source); graph=elaborate(module); reconstructed=reconstruct(graph)
        assert elaborate(reconstructed)['structural_digest']==graph['structural_digest']
        assert format_source(format_source(source))==format_source(source)
        status=check(module)
        examples.append({'name':path.name,'source_sha256':record['receiptSha256'],'parsed':True,
                         'formatted_idempotently':True,'graph_reconstruction':True,
                         'static_status':status['status'],'obligations':status['obligations'],
                         'definitions':status['definitions'], 'graph_obligations':graph['obligations']})
    suite=unittest.defaultTestLoader.discover(str(ROOT/'tests'))
    stream=io.StringIO()
    result=unittest.TextTestRunner(stream=stream,verbosity=2,resultclass=Results).run(suite)
    (artifacts/'tests.log').write_text(stream.getvalue())
    report={'schema':'etellis.u.verification/1','date_utc':datetime.now(timezone.utc).isoformat(),
            'python':sys.version,'platform':platform.platform(), 'tests_run':result.testsRun,
            'passed':sum(x['verdict']=='passed' for x in result.records),'failures':len(result.failures),
            'errors':len(result.errors),'skipped':len(result.skipped),'test_results':result.records,
            'preserved_sources':preservation,'examples':examples,
            'scope':'fresh local reference, frontend, graph, proof kernel, resource, CLI/LSP, packages, bounded native/WASM tests',
            'historical_audit_rerun':False,'full_U_complete':False}
    report['implementation_files']={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
                                    for p in sorted((ROOT/'u').rglob('*.py'))}
    report['identity']=digest('verification-report',report)
    (artifacts/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
    evaluator=Evaluator(parse('u "etellis.u/0.1";'))
    operators=[]
    for name,function in sorted(evaluator.registry.items()):
        try:
            file=Path(inspect.getsourcefile(function)); location=file.relative_to(ROOT).as_posix()
            line=inspect.getsourcelines(function)[1]
        except (TypeError,OSError,ValueError):
            location,line='u/evaluator.py',None
        operators.append({'name':name,'origin':'reference implementation','support':{'reference':True,'general_compiled':False,'physical_device':False},
                          'rule_source':location,'rule_line':line,
                          'rule_source_sha256':hashlib.sha256((ROOT/location).read_bytes()).hexdigest(),
                          'law_proof_status':'per-law review required; registration is not proof',
                          'trust_dependencies':['Python>=3.13',location],'assumptions':['declared admitted reference fragment'],
                          'native_subsumption':'not implied by operator registration'})
    ledger={'schema':'etellis.u.capabilities/1','operators':operators,
            'source_frontends':{'U':'implemented shared surface and bounded static checker','CDC':'finite primitive fragment; source residual for unsupported directives'},
            'foreign_frontends':{x:'not implemented; theory conformance is not source ingestion' for x in ['Assembly','C','Rust','Python','JavaScript','TypeScript','Lisp/Racket','Haskell','Prolog','SQL','APL','Stan','Erlang','TLA+','Lean','SystemVerilog','CUDA','OpenQASM']},
            'specialized_realizations':{'GPU':'CPU reference simulator','HDL':'two-state clocked reference simulator','QPU':'statevector/instrument simulator only','DAE':'affine index-1 backward Euler'},
            'self_hosting':'stage0 only','full_U_complete':False}
    (ROOT/'spec/capabilities.json').write_text(json.dumps(ledger,indent=2)+'\n')
    print(f"{report['passed']}/{result.testsRun} tests passed; {len(result.skipped)} skipped; {len(examples)}/22 original sources preserved/parsed/reconstructed; {len(operators)} reference operators")
    if not result.wasSuccessful():
        print(stream.getvalue()); return 1
    return 0


if __name__=='__main__': raise SystemExit(main())
