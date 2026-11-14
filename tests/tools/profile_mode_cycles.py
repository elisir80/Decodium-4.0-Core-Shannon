#!/usr/bin/env python3
"""Measure repeated real FT8/FT4 file decodes and idle dwell (macOS, no RF).

Uses temporary QML instrumentation, restores it and test settings in finally.
Software rendering keeps this focused on decoder/resource lifecycle, not GPU
performance. Run without other Decodium instances or concurrent benchmarks.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import socket
import subprocess
import tempfile
import time

from measure_runtime_baseline import LAB_SANDBOX_POLICY, collect_diagnostics

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/decodium')
    parser.add_argument('--ft4-wav', type=Path, required=True)
    parser.add_argument('--cycles', type=int, default=6)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--probe-late-callbacks', action='store_true',
                        help='Exercise old and fresh Q65/WSPR/JT65 callbacks using the instrumented lab build')
    parser.add_argument('--expect-late-rejected', action='store_true',
                        help='Require all replayed old callbacks to be rejected')
    parser.add_argument('--interrupt-ft4', action='store_true',
                        help='Use the existing active-worker mode-switch probe on the third FT4 decode')
    args = parser.parse_args()
    if args.expect_late_rejected and not args.probe_late_callbacks:
        parser.error('--expect-late-rejected requires --probe-late-callbacks')
    if args.interrupt_ft4 and args.cycles < 3:
        parser.error('The active FT4 probe requires at least three cycles')
    if args.cycles < 1:
        parser.error('At least one cycle is required')
    if subprocess.run(['pgrep', '-x', 'decodium.*|Decodium4'], capture_output=True).returncode == 0:
        parser.error('Another Decodium instance is running')
    binary = args.binary.resolve(strict=True)
    qml = binary.parent / 'qml/decodium/Main.qml'
    original_qml = qml.read_bytes()
    marker = b'GlassApplicationWindow {'
    if original_qml.count(marker) != 1:
        parser.error('Unexpected QML root')
    fixtures = [ROOT / 'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav', args.ft4_wav.resolve(strict=True)]
    rig = 'PerfCycle' + secrets.token_hex(6)
    legacy = Path.home() / '.qttest/Library/Preferences/decodium4.ini'
    original_ini = legacy.read_bytes() if legacy.exists() else None
    args.output.mkdir(parents=True, exist_ok=False)
    probe = '''
    Timer {
      id: cycleStart; interval:12000; running:true; repeat:false
      property int step:0
      onTriggered: {
        if (step >= __STEPS__) {
          if (__LATE_CALLBACKS__) {
            bridge.mode="FT8";
            var result=bridge.logSstvQso({__contextProbe:true});
            result.event="late-callbacks";result.epoch=Date.now();
            console.log("[CYCLE] "+JSON.stringify(result));
          }
          Qt.quit(); return;
        }
        var mode = step % 2 === 0 ? "FT8" : "FT4";
        bridge.mode = mode;
        console.log("[CYCLE] " + JSON.stringify({event:"start",step:step,mode:mode,epoch:Date.now(),cleared:bridge.decodeList.length===0}));
        bridge.filtersBypassed = true;
        bridge.openWavForDecode(step % 2 === 0 ? __FT8__ : __FT4__);
        cycleWait.ticks=0; cycleWait.start();
      }
    }
    Timer {
      id:cycleWait; interval:100; repeat:true; property int ticks:0
      property bool interrupted:false; property bool restarted:false
      onTriggered: {
        if (__INTERRUPT__ && cycleStart.step === 5 && !restarted) {
          if (bridge.mode !== "FT4") {
            if (!interrupted) console.log("[CYCLE] "+JSON.stringify({event:"interrupted",step:5,epoch:Date.now()}));
            interrupted=true;
            if(++ticks>=300) { stop();Qt.quit(); }
            return;
          }
          if (interrupted) {
            restarted=true;ticks=0;bridge.openWavForDecode(__FT4__);return;
          }
        }
        var wanted = cycleStart.step % 2 === 0 ? "CQ K1ABC FN42" : "CQ 9H1SR JM75";
        var rows=bridge.decodeList;
        for(var i=0;i<rows.length;i++) {
          if(String(rows[i].message).trim()===wanted) {
            console.log("[CYCLE] "+JSON.stringify({event:"decoded",step:cycleStart.step,mode:bridge.mode,epoch:Date.now(),message:wanted,tx:bridge.transmitting}));
            stop();cycleStart.step++;cycleStart.interval=4000;cycleStart.start();return;
          }
        }
        if(++ticks>=300) {
          console.log("[CYCLE] "+JSON.stringify({event:"timeout",step:cycleStart.step,epoch:Date.now()}));
          stop();Qt.quit();
        }
      }
    }
'''.replace('__LATE_CALLBACKS__', str(args.probe_late_callbacks).lower()).replace('__INTERRUPT__', str(args.interrupt_ft4).lower()).replace('__STEPS__', str(args.cycles * 2)).replace('__FT8__', json.dumps(str(fixtures[0]))).replace('__FT4__', json.dumps(str(fixtures[1])))
    sinks, samples = [], []
    process = None
    timed_out = False
    udp_failure = False
    try:
        for port in (2237, 2239):
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sinks.append(sock)
            sock.bind(('127.0.0.1', port))
        legacy.parent.mkdir(parents=True, exist_ok=True)
        legacy.write_text('')
        qml.write_bytes(original_qml.replace(marker, marker + probe.encode(), 1))
        with tempfile.TemporaryDirectory(prefix='decodium-mode-profile-') as settings:
            env = {k: v for k, v in os.environ.items()
                   if not k.startswith(('DECODIUM_', 'QT_', 'QSG_', 'QML_', 'OMP_'))}
            env.update(DECODIUM_LAB_SETTINGS_DIR=settings, DECODIUM_FORCE_STANDALONE_UDP='1',
                       DECODIUM_DISABLE_QML_CACHE='1', QT_QUICK_BACKEND='software', QT_QPA_PLATFORM='cocoa')
            if args.interrupt_ft4:
                env.update(DECODIUM_HASH_LIFECYCLE_DIAGNOSTICS='1',
                           DECODIUM_LAB_SWITCH_DURING_FT4_DECODE='1')
            command = ['sandbox-exec', '-p', LAB_SANDBOX_POLICY, str(binary), '--test-mode',
                       '--rig-name', rig, '--lab-no-cat', '--lab-no-monitor', '--lab-mode', 'FT8',
                       '--lab-callsign', 'IU8LMC', '--lab-grid', 'JM75']
            log_path = args.output / 'stdout.log'
            with log_path.open('w') as log:
                process = subprocess.Popen(command, env=env, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
                start = time.monotonic()
                while process.poll() is None:
                    elapsed = time.monotonic() - start
                    udp_failure = 'QNativeSocketEngine::writeDatagram() was called not in' in log_path.read_text(errors='replace')
                    timed_out = elapsed > 30 + args.cycles * 70
                    if udp_failure or timed_out:
                        process.terminate()
                        break
                    values = subprocess.run(['ps', '-p', str(process.pid), '-o', 'rss=,%cpu='], capture_output=True, text=True, timeout=5).stdout.split()
                    if len(values) == 2:
                        samples.append({'epoch': time.time() * 1000, 'seconds': elapsed,
                                        'rss_kib': int(values[0]), 'cpu_percent': float(values[1])})
                    time.sleep(0.25)
                process.wait(timeout=15)
            diagnostics = collect_diagnostics(rig, args.output, 'cycles')
            log_text = log_path.read_text()
            events = [json.loads(line.split('[CYCLE] ', 1)[1]) for line in log_text.splitlines() if '[CYCLE] ' in line]
            starts = [e for e in events if e['event'] == 'start']
            decoded = [e for e in events if e['event'] == 'decoded']
            combined_log = log_text + diagnostics
            interrupt_verified = (not args.interrupt_ft4 or
                                  (sum(e['event'] == 'interrupted' for e in events) == 1
                                   and 'lab_ft4_decoder_switch_end ok= 1' in combined_log
                                   and 'lab_ft4_restore_end ok= 1' in combined_log))
            late = [e for e in events if e['event'] == 'late-callbacks']
            late_verified = (not args.probe_late_callbacks or
                             (len(late) == 1 and late[0].get('probeExecuted') is True
                              and late[0].get('tx') is False and late[0].get('cat') is False
                              and [c.get('mode') for c in late[0].get('cases', [])] == ['Q65', 'WSPR', 'JT65']
                              and all(c.get('freshAccepted') is True and c.get('tempCleaned') is True
                                      and (not args.expect_late_rejected or c.get('oldAccepted') is False)
                                      for c in late[0]['cases'])))
            valid = (late_verified and interrupt_verified and process.returncode == 0 and not timed_out and not udp_failure
                     and len(decoded) == args.cycles * 2 and len(starts) == len(decoded)
                     and all(e['cleared'] for e in starts) and all(not e['tx'] for e in decoded)
                     and '[LAB] CAT auto-connect disabled' in log_text)
            idle = []
            for event in decoded:
                window = [s['rss_kib'] / 1024 for s in samples if 2000 <= s['epoch'] - event['epoch'] <= 3500]
                idle.append({'step': event['step'], 'mode': event['mode'],
                             'idle_rss_mib': sum(window) / len(window) if window else None})
            result = {'valid': valid, 'exit': process.returncode, 'timed_out': timed_out,
                      'udp_failure': udp_failure, 'cycles': args.cycles,
                      'probe_late_callbacks': args.probe_late_callbacks,
                      'expect_late_rejected': args.expect_late_rejected,
                      'interrupt_ft4': args.interrupt_ft4, 'interrupt_verified': interrupt_verified, 'events': events,
                      'idle_memory': idle, 'samples': samples,
                      'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                      'qml_original_sha256': hashlib.sha256(original_qml).hexdigest(),
                      'fixture_sha256': [hashlib.sha256(p.read_bytes()).hexdigest() for p in fixtures],
                      'diagnostics_captured': bool(diagnostics),
                      'scope': 'real file decodes, alternating FT8/FT4, software-rendered UI, no CAT or RF'}
            (args.output / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
            print(json.dumps({k: v for k, v in result.items() if k not in ('events', 'samples')}), flush=True)
            return 0 if valid else 1
    finally:
        if process and process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        qml.write_bytes(original_qml)
        if original_ini is None:
            legacy.unlink(missing_ok=True)
        else:
            legacy.write_bytes(original_ini)
        for sock in sinks:
            sock.close()
        for category in ('Application Support', 'Caches'):
            owned = Path.home() / '.qttest/Library' / category / 'IU8LMC' / f'Decodium - {rig} - test'
            if owned.is_dir():
                shutil.rmtree(owned)


if __name__ == '__main__':
    raise SystemExit(main())
