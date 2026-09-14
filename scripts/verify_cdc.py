"""Build the isolated original-CDC oracle and compare randomized source programs."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import platform
import random
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from u.cdc import execute_source, PROFILE

PIN = '1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--bidi', type=Path, default=ROOT/'compatibility/cdc/vendor')
    parser.add_argument('--cases', type=int, default=96)
    args = parser.parse_args()
    repo = args.bidi.resolve()
    git = lambda *a: subprocess.check_output(['git', '-C', str(repo), *a], text=True).strip()
    pin=json.loads((ROOT/'compatibility/cdc/SOURCE_PIN.json').read_text())
    if pin['sha'] != PIN: raise RuntimeError('oracle pin mismatch')
    for name,expected in pin['files'].items():
        if hashlib.sha256((repo/name).read_bytes()).hexdigest()!=expected:
            raise RuntimeError('oracle source hash mismatch: '+name)
    external=(repo/'.git').exists()
    before=git('status','--porcelain=v1','--untracked-files=no') if external else ''
    if external and (git('rev-parse','HEAD')!=PIN or before):
        raise RuntimeError('external oracle checkout must be clean and pinned')
    build = ROOT/'build'/'oracle'
    build.mkdir(parents=True, exist_ok=True)
    binary = build/'cdc-differential'
    units = ['cdc_variational.c', 'cdc_linalg.c', 'cdc_source.c', 'cdc_store.c', 'cdc_digest.c',
             'cdc_blake3.c', 'cdc_receipt.c', 'cdc_parser.c', 'cdc_ast.c', 'cdc_lexer.c', 'cdc_diagnostic.c']
    command = ['cc', '-std=c99', '-O2', '-pthread', '-I'+str(repo/'runtime'),
               str(ROOT/'compatibility/cdc/oracle_adapter.c'),
               *[str(repo/'runtime'/u) for u in units], '-lm', '-o', str(binary)]
    subprocess.run(command, check=True, capture_output=True, text=True)
    rng = random.Random(0xE7E1115)
    cases = []
    with tempfile.TemporaryDirectory(prefix='u-cdc-parity-') as folder:
        for i in range(args.cases):
            gain, deadband = rng.uniform(-2, 2), rng.choice([0.0, .1, .5])
            lines = [f'field f gain={gain!r} deadband={deadband!r}', 'field other gain=1.0',
                     'module parent field=f', 'module child field=f', 'module outside field=other']
            for c in range(6):
                module = 'parent' if c < 2 else 'child' if c < 5 else 'outside'
                lines.append(f'cell c{c} module={module} theta={rng.uniform(-4,4)!r} omega={rng.uniform(-2,2)!r}')
            for e in range(9):
                a, b = rng.randrange(6), rng.randrange(6)
                lines.append(f'channel c{a} -> c{b} weight={rng.uniform(-1,1)!r} angle={rng.uniform(-1,1)!r}')
            for step in range(4):
                lines += [f'flow f{step} field=f duration={rng.uniform(0,.5)!r}',
                          f'commit k{step} module=child', f'nest n{step} parent=parent child=child']
            text = '\n'.join(lines) + '\n'
            path = Path(folder)/f'case-{i}.cdc'
            path.write_text(text)
            oracle = json.loads(subprocess.check_output([str(binary), str(path)], text=True))
            actual = execute_source(text)
            assert actual['verdict'] == 'Done', actual
            for native, computed in zip(oracle['cells'], actual['state']['cells']):
                assert float.fromhex(native['theta']).hex() == computed['theta'].hex(), (i, native, computed)
                assert native['has_latch'] == computed['has_latch'] and native['latch'] == computed['latch'], (i, native, computed)
            for native, computed in zip(oracle['modules'], actual['state']['modules']):
                for key in ('belief', 'prior'):
                    assert float.fromhex(native[key]).hex() == float(computed[key]).hex(), (i, key, native, computed)
            for native, computed in zip(oracle['steps'], actual['trace']):
                if native['kind'] == 'commit':
                    assert native['status'] == computed['status']
                    assert native['trits'] == ''.join('+' if t==1 else '-' if t==-1 else '0' for t in computed['trits'])
            cases.append({'source_sha256': hashlib.sha256(text.encode()).hexdigest(), 'passed': True})
    after = git('status', '--porcelain=v1', '--untracked-files=no') if external else ''
    assert before == after
    receipt = {'schema': 'etellis.u.cdc-parity/1', 'profile': PROFILE, 'bidi_sha': PIN,
               'scope': 'finite well-formed field/module/cell/channel + flow/commit/nest; exact binary64 values and trit outcomes',
               'cases': cases, 'passed': len(cases), 'total': args.cases, 'host': platform.platform(),
               'python': sys.version, 'oracle_compiler': subprocess.check_output(['cc','--version'],text=True).splitlines()[0],
               'oracle_snapshot_hashes_verified':True, 'external_bidi_checkout_checked':external,
               'bidi_tracked_source_unchanged':True if external else None, 'full_cdc_compatibility': False}
    (ROOT/'artifacts/cdc-parity.json').write_text(json.dumps(receipt, indent=2)+'\n')
    print(f'CDC primitive differential parity: {len(cases)}/{args.cases}; exact binary64 and latch state; BiDi unchanged')


if __name__ == '__main__':
    main()
