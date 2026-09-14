"""Qualified U command line, with explicit execution and evidence boundaries."""
from __future__ import annotations

import argparse
import contextlib
import dataclasses
import hashlib
import io
import json
from pathlib import Path
import platform
import sys
import unittest

from . import __version__
from .source import parse, format_source, SourceError
from .checker import check
from .graph import elaborate
from .evaluator import Evaluator, Closure, Native, Code
from .theories.base import RuntimeFault, Capability
from .evidence import digest, exact_source, receipt, strict_json, verify_receipt


def describe(value, depth=0):
    if depth > 200:
        raise ValueError('output nesting limit exceeded; no truncated artifact emitted')
    if value is None or type(value) in {str,bool,int,float}:
        return value
    if isinstance(value, complex):
        return {'real':value.real,'imaginary':value.imag}
    if isinstance(value, Capability):
        return {'kind':'Capability','name':value.name,'authority':'process-local opaque'}
    if isinstance(value, (Closure, Native)):
        return {'kind':type(value).__name__,'callable':True}
    if isinstance(value, Code):
        return {'kind':'Code','expression':value.expression,'stage':value.stage}
    if isinstance(value, (tuple,list)):
        return [describe(x,depth+1) for x in value]
    if isinstance(value, dict):
        return {str(k):describe(v,depth+1) for k,v in value.items()}
    if dataclasses.is_dataclass(value):
        return {'kind':type(value).__name__, **{f.name:describe(getattr(value,f.name),depth+1) for f in dataclasses.fields(value)
                if f.name not in {'evaluator','env','environment','authority','_authority','owner'}}}
    return {'kind':type(value).__name__,'representation':'opaque'}


def implementation_identity():
    root=Path(__file__).parent
    files={p.relative_to(root).as_posix():exact_source(p.read_bytes()) for p in sorted(root.rglob('*.py'))}
    return digest('stage0-realization',{'version':__version__,'python':platform.python_version(),
                                      'platform':platform.platform(),'files':files})


def emit(value):
    print(json.dumps(describe(value),ensure_ascii=False,sort_keys=True,indent=2,allow_nan=False))


def execution(args):
    source=Path(args.file).read_bytes()
    if Path(args.file).suffix=='.cdc':
        from .cdc import execute_source, path_tangent
        result=(path_tangent(source.decode()) if args.command=='prove' else execute_source(source.decode(),args.budget))
        emit(result)
        return 0 if result['verdict']=='Done' else 3 if result['verdict']=='Held' else 4
    module=parse(source.decode('utf-8'))
    checked=check(module)
    graph=elaborate(module)
    if checked['status']=='rejected':
        emit(checked); return 2
    reference=args.command=='eval' or args.reference or args.command=='prove'
    if checked['status']!='checked' and not reference:
        emit({'verdict':'Unsupported','reason':'static-domain-checking-incomplete','check':checked,
              'next':'Use eval or --reference for explicit reference evaluation with unresolved static obligations.'}); return 4
    if graph['obligations']:
        emit({'verdict':'Unsupported','reason':'unadmitted-theory-composition','obligations':graph['obligations']}); return 4
    from .graph_execution import reconstruct
    executable_module=reconstruct(graph)
    evaluator=Evaluator(executable_module,capabilities=args.cap,budget=args.budget)
    input_values=strict_json(args.args)
    if not isinstance(input_values,list):
        raise ValueError('--args must be a JSON array')
    definition=next((d for d in module['definitions'] if d['name']==args.entry),None)
    if definition and not input_values and definition['params']:
        types=[p['type'].get('name') for p in definition['params']]
        if types==['ConsoleCap'] and 'console' in evaluator.capabilities:
            input_values=[evaluator.capabilities['console']]
    observations=io.StringIO()
    with contextlib.redirect_stdout(observations):
        value=evaluator.run(args.entry,input_values)
    if args.command=='prove':
        from .proof import is_checked_receipt
        if not is_checked_receipt(value):
            raise ValueError('prove requires a receipt minted by the independent proof checker')
    result=describe(value)
    verdict=value.get('verdict','Done') if isinstance(value,dict) else 'Done'
    status=value.get('status') if isinstance(value,dict) else getattr(value,'status',None)
    if status in {'held','budget_exhausted','pending','cancelled'}:
        verdict='Held'
    elif status in {'failed','fault'}:
        verdict='Fault'
    if verdict not in {'Done','Held','Fault','Unsupported','Rejected','Indeterminate'}:
        verdict='Done'
    bound=receipt(scope='stage0 direct U reference evaluation', verdict=verdict,
                  meaning=graph['semantic_digest'], realization=implementation_identity(), source=exact_source(source),
                  inputs=describe(input_values),output=result,observations=observations.getvalue().splitlines(),
                  obligations=checked.get('obligations',[]), assumptions=['static domain obligations unresolved'] if checked['status']!='checked' else [])
    emit({'result':result,'console':observations.getvalue().splitlines(),'receipt':bound,
          'static_status':checked['status'],'remaining_budget':evaluator.remaining})
    return 0 if verdict=='Done' else 3 if verdict=='Held' else 4


def main(argv=None):
    parser=argparse.ArgumentParser(prog='etellis-u',description='U — law-bearing operations')
    parser.add_argument('--version',action='version',version='etellis-u '+__version__+' (stage0 reference)')
    subs=parser.add_subparsers(dest='command',required=True)
    for name in ('run','eval','prove'):
        p=subs.add_parser(name); p.add_argument('file'); p.add_argument('--entry',default='main'); p.add_argument('--args',default='[]')
        p.add_argument('--cap',action='append',default=[]); p.add_argument('--budget',type=int,default=100000)
        p.add_argument('--reference',action='store_true')
    for name in ('check','fmt','explain','lift','export','audit'):
        p=subs.add_parser(name); p.add_argument('file')
        if name=='fmt': p.add_argument('--write',action='store_true')
        if name=='export': p.add_argument('--output',required=True)
    p=subs.add_parser('build'); p.add_argument('file'); p.add_argument('--entry',required=True)
    p.add_argument('--target',choices=('native','wasm'),required=True); p.add_argument('--bounds',required=True,help='JSON [[min,max],...]')
    p.add_argument('--output',required=True)
    subs.add_parser('lsp'); subs.add_parser('test')
    p=subs.add_parser('package'); p.add_argument('operation',choices=('lock','install','verify')); p.add_argument('source',nargs='?',default='.')
    p.add_argument('--destination',default='.u-packages')
    args=parser.parse_args(argv)
    try:
        if args.command in ('run','eval','prove'):
            return execution(args)
        if args.command=='lsp':
            from .lsp import serve
            return serve()
        if args.command=='test':
            candidates=[Path(__file__).resolve().parents[1]/'tests',Path.cwd()/'tests']
            test_path=next((p for p in candidates if (p/'test_runtime.py').is_file() and (p.parent/'U.toml').is_file()),None)
            if test_path is None:
                emit({'verdict':'Unsupported','reason':'source test suite is not packaged in the runtime wheel','next':'Run etellis-u test from the U repository checkout'})
                return 4
            suite=unittest.defaultTestLoader.discover(str(test_path))
            return 0 if unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful() else 1
        if args.command=='package':
            from . import packages
            result=packages.install(args.source,args.destination) if args.operation=='install' else getattr(packages,args.operation)(args.source)
            emit(result); return 0
        raw=Path(args.file).read_bytes()
        if args.command=='audit':
            r=strict_json(raw.decode()); valid=verify_receipt(r.get('receipt',r))
            emit({'verdict':'Done' if valid else 'Rejected','scope':'receipt identity and dependency fields; not truth or signer authenticity'})
            return 0 if valid else 2
        if args.command=='export':
            envelope=strict_json(raw.decode()); residual=envelope['residual']
            if exact_source(residual.encode())!=envelope['source_identity']:
                raise ValueError('residual/source digest mismatch')
            if digest('lifted-graph',envelope['graph'])!=envelope['graph_identity']:
                raise ValueError('graph changed without a checked residual lens')
            output=Path(args.output)
            if output.exists(): raise ValueError('export target already exists')
            output.write_bytes(residual.encode()); emit({'verdict':'Done','scope':'exact preserved source reconstruction'}); return 0
        module=parse(raw.decode('utf-8'))
        if args.command=='check':
            result=check(module); graph=elaborate(module)
            result['composition_obligations']=graph['obligations']
            emit(result); return 0 if result['status']=='checked' and not graph['obligations'] else 2 if result['status']=='rejected' else 4
        if args.command=='fmt':
            formatted=format_source(raw.decode())
            if args.write:
                Path(args.file).write_text(formatted)
            else: sys.stdout.write(formatted)
            return 0
        if args.command=='explain':
            emit({'check':check(module),'elaboration':elaborate(module)}); return 0
        if args.command=='lift':
            graph=elaborate(module)
            emit({'schema':'etellis.u.envelope/1','source_identity':exact_source(raw),'residual':raw.decode(),
                  'graph':graph,'graph_identity':digest('lifted-graph',graph),
                  'provenance':{'frontend':'etellis.u/0.1','foreign_language_subsumption':False}}); return 0
        if args.command=='build':
            from .backend import build
            emit(build(module,args.entry,strict_json(args.bounds),Path(args.output),args.target)); return 0
    except SourceError as error:
        emit({'verdict':'Rejected','diagnostic':error.as_dict()}); return 2
    except RuntimeFault as error:
        verdict='Held' if error.code=='BUDGET_EXHAUSTED' else 'Unsupported' if error.code=='UNSUPPORTED_OPERATION' else 'Rejected'
        emit({'verdict':verdict,'code':error.code,'message':str(error),'details':error.details}); return 3 if verdict=='Held' else 4 if verdict=='Unsupported' else 2
    except (ValueError, OSError, KeyError, TypeError, OverflowError, RecursionError) as error:
        emit({'verdict':'Rejected','code':type(error).__name__,'message':str(error)}); return 2


if __name__=='__main__':
    raise SystemExit(main())
