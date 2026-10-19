#!/usr/bin/env python3
"""Check resolved PlatformIO profiles, including inherited board/layout settings."""
import json
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[2]
pio = sys.argv[1] if len(sys.argv)>1 else 'pio'
config = dict(json.loads(subprocess.check_output([pio,'project','config','--json-output'],cwd=root)))
rows = json.loads((root/'test/rx/targets.json').read_text())
for row in rows:
    rx = dict(config['env:'+row['environment']])
    dx = dict(config['env:'+row['source_environment']])
    flags = ' '.join(rx['build_flags'])
    assert 'LORAGOTCHI_RX=1' in flags and 'LORAFISH_UI' not in flags
    assert 'ui-new' not in flags and 'ui-new' not in ' '.join(rx['build_src_filter'])
    assert ('ENABLE_USB_INTERFACE' in flags) == (row['transport']=='usb')
    assert ('BLE_PIN_CODE' in flags) == (row['transport']=='ble')
    for key in ('board','board_build.ldscript','board_build.partitions','board_upload.maximum_size'):
        assert rx.get(key) == dx.get(key), (row['environment'],key)
    for flag in dx['build_flags']:
        if any(x in flag for x in ('LORA_', 'SX126', 'SX127', 'PIN_', 'GPS_', 'MAX_CONTACTS', 'MAX_GROUP_CHANNELS', 'AUTO_SHUTDOWN')):
            # Transport PIN is intentionally absent in USB builds.
            if 'BLE_PIN_CODE' in flag: continue
            assert flag in rx['build_flags'], (row['environment'],flag)
    assert not any(x in row['environment'].lower() for x in ('tdeck','cardputer'))
print(f'{len(rows)} RX profiles: role/transport isolation and original board/RF/layout settings verified')
