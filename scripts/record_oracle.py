"""Record exact vendored oracle bytes after checking the upstream checkout."""
from pathlib import Path
import hashlib
import json
import subprocess

ROOT=Path(__file__).resolve().parents[1]
PIN='1307f2a7f32ab5beb64fe5cd0c0faf13ec9c8157'
source=ROOT.parent/'BiDi'
assert subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()==PIN
assert not subprocess.check_output(['git','-C',str(source),'status','--porcelain=v1','--untracked-files=no'],text=True).strip()
vendor=ROOT/'compatibility/cdc/vendor'
files={}
for file in sorted(vendor.rglob('*')):
    if not file.is_file(): continue
    relative=file.relative_to(vendor)
    original=source/relative
    assert original.read_bytes()==file.read_bytes(),relative
    files[relative.as_posix()]=hashlib.sha256(file.read_bytes()).hexdigest()
(ROOT/'compatibility/cdc/SOURCE_PIN.json').write_text(json.dumps({
    'repository':'https://github.com/ETEllis/BiDi','sha':PIN,'files':files,
    'purpose':'isolated independent oracle only; never imported by U execution',
    'license':'MIT; copyright Edward Ellis 2026','verified_byte_exact':True},indent=2)+'\n')
print(f'{len(files)} exact oracle source/license files pinned')
