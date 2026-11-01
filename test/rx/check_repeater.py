#!/usr/bin/env python3
"""Compile the actual TankMesh handlers against a bounded stock/radio/flash mock.

Production .cpp is copied unchanged; only its header's two dependency includes
are redirected. The same LRX receiver/journal and wire/auth classes are used on
hardware. The mock hash is not cryptographic or on-air verification.
"""
from pathlib import Path
import json
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[2]
source=root/'examples/repeater_tank'
with tempfile.TemporaryDirectory(prefix='lrx-repeater-') as directory:
    temp=Path(directory)
    header=(source/'TankMesh.h').read_text().replace(
        '#include "../simple_repeater/MyMesh.h"', '#include "repeater_mock.h"').replace(
        '#include "../companion_radio/LoRagotchiRXJournal.h"',
        '#include "examples/companion_radio/LoRagotchiRXJournal.h"')
    (temp/'TankMesh.h').write_text(header)
    (temp/'TankMesh.cpp').write_bytes((source/'TankMesh.cpp').read_bytes())
    binary=temp/'check'
    subprocess.run(['g++','-std=c++17','-g','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer','-I',str(temp),'-I',str(root),
                    '-I',str(root/'test/rx'),'-I',str(source),
                    str(temp/'TankMesh.cpp'),str(root/'test/rx/check_repeater.cpp'),
                    '-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
    if len(sys.argv)>2:
        import os, select
        aquarium=Path(sys.argv[2]).resolve();sys.path.insert(0,str(aquarium))
        from tank.client import Client
        from tank.store import Store
        from tank.transport import StreamTransport
        from tank.radio import RadioSession
        process=subprocess.Popen([str(binary),'serve'],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
        class Pipe(StreamTransport):
            def read(self):
                return os.read(process.stdout.fileno(),3) if select.select([process.stdout],[],[],.1)[0] else b''
            def write(self,data):
                process.stdin.write(data[:2]);process.stdin.flush()
                process.stdin.write(data[2:]);process.stdin.flush()
            def close(self): process.stdin.close()
        class CrashAfterSave(Store):
            crash=True
            def save(self,event):
                result=super().save(event)
                if self.crash:
                    self.crash=False;raise RuntimeError('crash before durable upload ACK')
                return result
        transport=Pipe();store=CrashAfterSave(temp/'tank.sqlite');client=Client(transport,store,token=45)
        try:
            assert client.connect()==dict(capacity=4,transports=132,storage_ok=True)
            assert client.water and client.lrx_only
            session=RadioSession(client,lambda event:None,network=True)
            assert not session.supported and not session.writable
            try:session.schedule_refresh()
            except ValueError as exc:assert 'Repeater' in str(exc)
            else:raise AssertionError('Companion API must remain disabled')
            client.poll_messages() # must not issue any companion command
            try:client.poll()
            except RuntimeError as exc:assert 'before durable' in str(exc)
            else:raise AssertionError('Crash boundary not reached')
            client.command(5);client=Client(transport,store,token=46);client.connect()
            count=0
            while client.poll():count+=1
            assert count==4 and len(store.roster())==2
            fish=sorted(store.roster(),key=lambda fish:fish.fish_id)
            assert fish[0].species_id==13 and fish[0].body_gene==220
            assert not fish[1].species_present and not fish[1].genes_present
            assert store.db.execute('SELECT COUNT(*) FROM events').fetchone()[0]==4
            assert store.db.execute('SELECT COUNT(*) FROM returns WHERE status="delivery-confirmed"').fetchone()[0]==2
            print('PASS real TankMesh -> fragmented LRX/1 -> Aquarium SQLite: Water/legacy, crash/replay, independent returns and LRX-only management isolation')
        finally:
            client.close();store.close();process.wait(timeout=5)

# Stock routing/admin remains the exact accepted stable source, with only one
# compile-guarded protected default-scope send hook for asynchronous returns.
stock=subprocess.check_output(['git','show','bf20da1:examples/simple_repeater/MyMesh.cpp'],cwd=root)
assert (root/'examples/simple_repeater/MyMesh.cpp').read_bytes()==stock
for folder in ('src','variants/heltec_v3','variants/heltec_v4','variants/heltec_t096'):
    assert not subprocess.check_output(['git','diff','bf20da1','--',folder],cwd=root)
print('PASS stock repeater implementation, shared MeshCore and original board/RF/partition sources unchanged')

if len(sys.argv)>1:
    config=dict(json.loads(subprocess.check_output([sys.argv[1],'project','config','--json-output'],cwd=root)))
    targets=json.loads((root/'test/rx/repeater-targets.json').read_text())
    for row in targets:
        env=dict(config['env:'+row['environment']]);stock=dict(config['env:'+row['source_environment']])
        flags=' '.join(env['build_flags']);filters=' '.join(env['build_src_filter'])
        assert 'LORAFISH_UI' not in flags and 'ENABLE_WIFI_INTERFACE' not in flags and 'BLE_PIN_CODE' not in flags
        assert 'LORAGOTCHI_REPEATER_TANK=1' in flags
        assert sum('LRX_TANK_'+kind.upper()+'=1' in flags for kind in ('usb','ble','wifi'))==1
        assert 'examples/companion_radio/*.cpp' not in filters and 'examples/simple_repeater>' not in filters
        for key in ('board','board_build.ldscript','board_build.partitions','board_upload.maximum_size'):
            assert env.get(key)==stock.get(key),(row['environment'],key)
        for flag in stock['build_flags']:
            if any(s in flag for s in ('LORA_','SX126','SX127','PIN_','DISPLAY_','GPS_')):
                assert flag in env['build_flags'],(row['environment'],flag)
        assert row['board']!='Heltec_t096' or row['transport']!='wifi'
    print(f'PASS {len(targets)} resolved repeater profiles: exclusive carriers, stock hardware/RF/partitions, no companion source')
