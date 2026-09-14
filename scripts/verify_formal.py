"""Verify the pinned Lean artifact and record exact source/command evidence."""
from pathlib import Path
import hashlib
import json
import subprocess

root=Path(__file__).resolve().parents[1]
command=['lean','+leanprover/lean4:v4.31.0','formal/U.lean']
result=subprocess.run(command,cwd=root,text=True,capture_output=True)
if result.returncode:
    print(result.stdout); print(result.stderr); raise SystemExit(result.returncode)
lines=[line for line in result.stdout.splitlines() if 'does not depend on any axioms' in line]
if len(lines)!=23:
    raise RuntimeError('unexpected theorem/axiom evidence; inspect the Lean output')
record={'schema':'etellis.u.formal/1','command':command,'exit_code':result.returncode,
        'source_sha256':hashlib.sha256((root/'formal/U.lean').read_bytes()).hexdigest(),
        'toolchain':subprocess.check_output(['lean','+leanprover/lean4:v4.31.0','--version'],text=True).strip(),
        'theorem_count':len(lines),'theorem_dependency_results':lines,
        'scope':'explicit abstract and finite definitions in formal/U.lean; not whole runtime correctness'}
(root/'artifacts/formal.json').write_text(json.dumps(record,indent=2)+'\n')
print(f'{len(lines)} Lean theorems checked; no axiom dependencies')
