#!/usr/bin/env python3
"""Exercise the Windows kit's exact QML probe in the isolated local macOS lab.
This validates probe logic, not the Windows runner or Windows binaries.
"""
import argparse
import json
from pathlib import Path
import subprocess
from measure_runtime_baseline import run_case, ROOT

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--kit', type=Path, required=True)
p.add_argument('--binary', type=Path, default=ROOT / 'build/decodium')
a = p.parse_args()
if subprocess.run(['pgrep', '-x', 'decodium.*|Decodium4'], capture_output=True).returncode == 0:
    p.error('Another Decodium instance is running')
a.output.mkdir(parents=True, exist_ok=False)
manifest = json.loads((a.kit / 'manifest.json').read_text())
fixtures = [dict(id=f['id'], mode=f['mode'], path=str((a.kit / f['file']).resolve())) for f in manifest['fixtures']]
probe = (a.kit / 'Probe.qml.inc').read_text().replace('__FIXTURES__', json.dumps(fixtures)).replace('__THREADS__', '2').replace('__LOWCPU__', 'false')
binary = a.binary.resolve(strict=True)
qml = binary.parent / 'qml/decodium/Main.qml'
original = qml.read_bytes()
marker = b'GlassApplicationWindow {'
assert original.count(marker) == 1
try:
    qml.write_bytes(original.replace(marker, marker + b'\n' + probe.encode(), 1))
    valid = run_case(binary, a.output, 'native', 1, 450)
    log = (a.output / 'native-1.log').read_text()
    events = [json.loads(line.split('[PERFTEST] ', 1)[1]) for line in log.splitlines() if '[PERFTEST] ' in line]
    starts = [e for e in events if e['event'] == 'start']
    results = [e for e in events if e['event'] == 'result']
    valid &= len(starts) == len(results) == len(fixtures)
    valid &= sum(e['event'] == 'done' for e in events) == 1
    valid &= all(e['cleared'] and e['threads'] == 2 and not e['lowCpu'] for e in starts)
    valid &= all(not e['tx'] and not e['timeout'] and not e['stillDecoding'] for e in results)
    for fixture in manifest['fixtures']:
        rows = [e for e in results if e['caseId'] == fixture['id']]
        valid &= len(rows) == 1
        if rows and fixture['expected']:
            valid &= any(r['message'] == fixture['expected'] for r in rows[0]['rows'])
    report = dict(valid=valid, events=events, scope='QML probe on macOS; not Windows runner validation')
    (a.output / 'probe-result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(dict(valid=valid, results=[dict(caseId=e['caseId'], count=len(e['rows'])) for e in results])))
    if not valid:
        raise SystemExit(1)
finally:
    qml.write_bytes(original)
