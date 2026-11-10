#!/usr/bin/env python3
"""Opt-in macOS lab: two isolated Decodium sessions over BlackHole, CAT disabled.

Requires numpy, sounddevice, a local build/decodium with offscreen Qt, and
BlackHole 2ch/16ch. Run with argument aligned or immediate. No RF proof.
"""
import os,sys,subprocess,time,json,sqlite3,secrets,tempfile
from pathlib import Path
import numpy as np
import sounddevice as sd
role=sys.argv[1] if len(sys.argv)>1 else 'aligned'
if role not in ('aligned','immediate'): raise SystemExit('expected aligned or immediate')
root=Path(tempfile.mkdtemp(prefix='decodium-bridge-ft8-'+role+'-'));settings=root/'settings';settings.mkdir()
suffix=secrets.token_hex(4)
def env(which):
 e=os.environ.copy();p=settings/which;p.mkdir(exist_ok=True);e.update(QT_QPA_PLATFORM=os.environ.get('DECODIUM_LAB_QPA_PLATFORM','offscreen'),QT_QUICK_BACKEND='software',DECODIUM_FORCE_STANDALONE_UDP='1',DECODIUM_LAB_SETTINGS_DIR=str(p))
 if which=='tx' and role=='aligned':e['DECODIUM_LAB_FTX_SLOT_TIMING']='1'
 else:e.pop('DECODIUM_LAB_FTX_SLOT_TIMING',None)
 return e
rxname='BridgePair'+role+'RX'+suffix;txname='BridgePair'+role+'TX'+suffix
common=[os.environ.get('DECODIUM_LAB_BINARY','./build/decodium'),'--test-mode','--lab-no-cat','--lab-mode','FT8','--lab-grid','JM75']
rxargs=common+['--rig-name',rxname,'--lab-audio-input','BlackHole 2ch','--lab-audio-output','BlackHole 16ch','--lab-callsign','IU8LMC','--lab-monitor-ms','2500','--lab-quit-ms','85000']
txargs=common+['--rig-name',txname,'--lab-audio-input','BlackHole 16ch','--lab-audio-output','BlackHole 2ch','--lab-callsign','9H1SR','--lab-tx-output-level','70','--lab-monitor-ms','2500','--lab-tx-period','0','--lab-sendtx-ms',os.environ.get('DECODIUM_LAB_SENDTX_MS','10000'),'--lab-sendtx-slot','6','--lab-quit-ms','65000']
device=next(i for i,d in enumerate(sd.query_devices()) if d['name']=='BlackHole 2ch' and d['max_input_channels']>=2)
with (root/('pair-'+role+'-rx.log')).open('w') as rxlog,(root/('pair-'+role+'-tx.log')).open('w') as txlog:
 rx=subprocess.Popen(rxargs,env=env('rx'),stdout=rxlog,stderr=subprocess.STDOUT)
 try:
  time.sleep(7);started=time.time();audio=sd.rec(70*48000,samplerate=48000,channels=2,device=device,dtype='float32')
  tx=subprocess.Popen(txargs,env=env('tx'),stdout=txlog,stderr=subprocess.STDOUT)
  try:txcode=tx.wait(timeout=100);sd.wait();rxcode=rx.wait(timeout=40)
  finally:
   if tx.poll() is None:tx.terminate();tx.wait(timeout=10)
 finally:
  if rx.poll() is None:rx.terminate();rx.wait(timeout=10)
step=4800;w=np.sqrt(np.mean(np.square(audio[:len(audio)//step*step].reshape(-1,step,2)),axis=(1,2)));mask=w>0.003
path=Path.home()/'.qttest/Library/Application Support/IU8LMC'/('Decodium - '+rxname+' - test')/'db.sqlite'
messages=[]
if path.exists():
 with sqlite3.connect('file:'+str(path)+'?mode=ro',uri=True) as db:messages=[r[0] for r in db.execute("select message from decodes where message like '%9H1SR%' order by id")]
out={'role':role,'txcode':txcode,'rxcode':rxcode,'peak':float(np.max(np.abs(audio))),'active_windows':int(mask.sum()),'first_active_mod15':float((started+np.flatnonzero(mask)[0]/10)%15) if mask.any() else None,'matching_messages':messages}
out['artifacts']=str(root)
(root/'result.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out),flush=True)
if txcode or rxcode or not mask.any() or not messages:raise SystemExit('loopback did not complete successfully')
