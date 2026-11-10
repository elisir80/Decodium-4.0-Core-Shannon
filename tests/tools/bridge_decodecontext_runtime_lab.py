#!/usr/bin/env python3
"""Compare mode-change cleanup and recovery after real file-based decodes."""
import argparse,json,os,secrets,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,default=ROOT/'build/decodium');p.add_argument('--qml',type=Path,default=ROOT/'build/qml/decodium/Main.qml');a=p.parse_args();qml=a.qml.resolve();original=qml.read_text();marker='GlassApplicationWindow {'
if original.count(marker)!=1:raise RuntimeError('unexpected QML root')
wav1=ROOT/'tests/bridge_refactor_fixtures/ft8-k1abc-1500.wav';wav2=ROOT/'tests/bridge_refactor_fixtures/ft8-9h1sr-1300.wav'
probe='''
 Timer {interval:12000;running:true;repeat:false;onTriggered:{bridge.openWavForDecode(__WAV1__);waitDecode.start()}}
 Timer {id:waitDecode;interval:250;running:false;repeat:true;property int ticks:0;property int phase:0;property bool cleared:false;onTriggered:{
  var rows=bridge.decodeList;var wanted=phase===0?"CQ K1ABC FN42":"CQ 9H1SR JM75";
  for(var i=0;i<rows.length;i++){if(String(rows[i].message).indexOf(wanted)>=0){
   if(phase===0){bridge.mode="FT4";cleared=(bridge.decodeList.length===0 && bridge.rxDecodeList.length===0);bridge.mode="FT8";bridge.openWavForDecode(__WAV2__);phase=1;ticks=0;return;}
   console.log("[CONTEXT-PROBE] "+JSON.stringify({cleared:cleared,recovered:true,tx:bridge.transmitting}));stop();return;
  }}if(++ticks>=100){console.log("[CONTEXT-PROBE] timeout");stop()}
 }}
'''.replace('__WAV1__',json.dumps(str(wav1))).replace('__WAV2__',json.dumps(str(wav2)))
try:
 with tempfile.TemporaryDirectory(prefix='decodium-context-lab-') as tmp:
  qml.write_text(original.replace(marker,marker+probe,1));env=os.environ.copy();env.update(DECODIUM_LAB_SETTINGS_DIR=tmp,DECODIUM_DISABLE_QML_CACHE='1',DECODIUM_FORCE_STANDALONE_UDP='1')
  cmd=[str(a.binary.resolve()),'--test-mode','--rig-name','Context'+secrets.token_hex(5),'--lab-no-cat','--lab-no-monitor','--lab-callsign','IU8LMC','--lab-mode','FT8','--lab-quit-ms','50000']
  result=subprocess.run(cmd,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=100)
  lines=[x.split('[CONTEXT-PROBE] ',1)[1] for x in result.stdout.splitlines() if '[CONTEXT-PROBE] ' in x]
  if result.returncode or len(lines)!=1 or lines[0]=='timeout' or '[LAB] CAT auto-connect disabled' not in result.stdout:
   print(result.stdout[-10000:]);raise RuntimeError('context probe failed')
  v=json.loads(lines[0]);
  if v!={'cleared':True,'recovered':True,'tx':False}:raise RuntimeError('unexpected context state '+str(v))
  print(json.dumps(v,sort_keys=True))
finally:qml.write_text(original)
