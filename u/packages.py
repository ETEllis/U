"""Explicit local package snapshots. No package script is executed."""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import tempfile
import tomllib
from .evidence import digest, strict_json


def inventory(root):
    root = Path(root).resolve()
    manifest = root/'U.toml'
    if not manifest.is_file():
        raise ValueError('package requires U.toml')
    data = tomllib.loads(manifest.read_text())
    package = data.get('package', {})
    if not re.fullmatch(r'[A-Za-z0-9_.-]+', package.get('name', '')) or package['name'] in {'.', '..'}:
        raise ValueError('invalid package name')
    members = {}
    for path in sorted(root.rglob('*')):
        rel = path.relative_to(root)
        if any(part.startswith('.') or part in {'build', 'dist', 'provenance'} for part in rel.parts):
            continue
        if path.is_symlink():
            raise ValueError('symlink package members require an unsupported external-source contract')
        if path.is_file() and (path.suffix == '.u' or path.name == 'U.toml'):
            members[rel.as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    result = {'name': package['name'], 'version': package.get('version'), 'members': members,
              'scripts': 'never-executed', 'permissions': data.get('permissions', {})}
    result['identity'] = digest('package-snapshot', result)
    return result


def lock(root):
    root = Path(root)
    result = {'schema': 'etellis.u.lock/1', 'packages': [inventory(root)]}
    path = root/'U.lock'
    # CLI lock is an explicit request to refresh this workspace's lockfile.
    with tempfile.NamedTemporaryFile(mode='w', dir=root, prefix='.u-lock-', delete=False) as temp:
        json.dump(result, temp, indent=2)
        temp.write('\n')
        temp.flush()
        os.fsync(temp.fileno())
        temporary = Path(temp.name)
    os.replace(temporary, path)
    return result


def verify(root):
    root = Path(root)
    locked = strict_json((root/'U.lock').read_text())
    actual = inventory(root)
    entries = locked.get('packages', [])
    matches = [p for p in entries if p.get('name') == actual['name']]
    if len(matches) != 1 or matches[0] != actual:
        raise ValueError('package content differs from locked snapshot')
    return {'verdict': 'Done', 'identity': actual['identity'], 'scope': 'exact locked local package members'}


def install(source, destination):
    source, destination = Path(source).resolve(), Path(destination).resolve()
    verify(source)
    record = inventory(source)
    target = destination / (record['name'] + '-' + record['identity'].split(':')[1][:16])
    destination.mkdir(parents=True, exist_ok=True)
    if target.exists():
        verify(target)
        return {'verdict':'Done', 'path':str(target), 'reused':True}
    temporary = Path(tempfile.mkdtemp(prefix='.u-install-', dir=destination))
    try:
        for member, expected in record['members'].items():
            data = (source/member).read_bytes()
            if hashlib.sha256(data).hexdigest() != expected:
                raise ValueError('source changed during package installation')
            path = temporary/member
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        (temporary/'U.lock').write_text(json.dumps({'schema':'etellis.u.lock/1','packages':[record]}, indent=2)+'\n')
        verify(temporary)
        os.rename(temporary, target)
    except BaseException:
        shutil.rmtree(temporary)
        raise
    return {'verdict':'Done', 'path':str(target), 'identity':record['identity']}
