#!/usr/bin/env python3
"""Package the Windows A/B harness and frozen corpus; never invent Windows builds."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import os

ROOT = Path(__file__).resolve().parents[2]
BASE = '3529dae3e218ef978b77347795572d96a39e9a55'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--before', type=Path)
    p.add_argument('--after', type=Path)
    args = p.parse_args()
    if bool(args.before) != bool(args.after):
        p.error('Provide both Windows portable bundles, or neither for a staging kit')
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = ROOT / 'tests/tools/windows_performance'
    for path in source.iterdir():
        if path.is_file():
            shutil.copy2(path, out / path.name)
    fixtures = []
    inputs = [
        ('clean-ft8', 'FT8', ROOT / 'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav', 'CQ K1ABC FN42'),
        ('busy-ft8', 'FT8', ROOT / 'build/performance-baseline/ft8-corpus/busy-902.wav', None),
        ('weak-ft8', 'FT8', ROOT / 'build/performance-baseline/ft8-corpus/mixed_weak-902.wav', None),
        ('clean-ft4', 'FT4', ROOT / 'build/performance-baseline/ft4-cycle-9h1sr.wav', 'CQ 9H1SR JM75'),
        ('reference-wspr', 'WSPR', ROOT / 'build/performance-baseline/wspr-reference-150426_0918.wav', 'ND6P DM04 30'),
    ]
    (out / 'fixtures').mkdir()
    for label, mode, path, expected in inputs:
        target = out / 'fixtures' / (label + '.wav')
        shutil.copy2(path, target)
        fixtures.append(dict(id=label, mode=mode, file=target.relative_to(out).as_posix(),
                             sha256=sha(target), expected=expected,
                             source=('https://downloads.sourceforge.net/project/wsjt/samples/WSPR/150426_0918.wav'
                                     if mode == 'WSPR' else 'Local frozen test fixture')))
    # A temporary Git index captures tracked and new source files without touching
    # the user's index, making commits, or including build artifacts.
    with tempfile.TemporaryDirectory(prefix='decodium-kit-index-') as temporary:
        env = dict(os.environ, GIT_INDEX_FILE=str(Path(temporary) / 'index'))
        subprocess.run(['git', 'read-tree', 'HEAD'], cwd=ROOT, env=env, check=True)
        paths = subprocess.check_output(['git', 'ls-files', '-m', '-o', '--exclude-standard', '-z'], cwd=ROOT).split(b'\0')
        paths = [os.fsdecode(path) for path in paths if path]
        if paths:
            subprocess.run(['git', 'add', '--', *paths], cwd=ROOT, env=env, check=True)
        patch = subprocess.check_output(['git', 'diff', '--cached', '--binary', BASE], cwd=ROOT, env=env)
    (out / 'candidate.patch').write_bytes(patch)
    builds = {}
    for name, directory in [('before', args.before), ('after', args.after)]:
        target = out / 'versions' / name
        if directory:
            directory = directory.resolve(strict=True)
            for required in ('decodium.exe', 'qml/decodium/Main.qml'):
                if not (directory / required).is_file():
                    p.error(f'Missing {required} in {directory}')
            if not list(directory.glob('*.dll')):
                p.error(f'No runtime DLLs in {directory}')
            shutil.copytree(directory, target)
            files = {f.relative_to(target).as_posix(): sha(f) for f in sorted(target.rglob('*')) if f.is_file()}
            builds[name] = dict(sha256=sha(target / 'decodium.exe'), files=files)
        else:
            target.mkdir(parents=True)
            builds[name] = dict(sha256=None, files={})
    manifest = dict(schema=1, ready=bool(args.before and args.after), baseline_commit=BASE,
                    candidate_patch_sha256=sha(out / 'candidate.patch'), builds=builds, fixtures=fixtures,
                    target='Windows 11 / third-generation Intel i5 / 8 GiB RAM',
                    validation='Windows execution pending; see worklog for local QML and parser checks')
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    archive = shutil.make_archive(str(out), 'zip', out.parent, out.name)
    print(json.dumps(dict(archive=archive, ready=manifest['ready'], sha256=sha(Path(archive)))))

if __name__ == '__main__':
    main()
