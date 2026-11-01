#!/usr/bin/env python3
"""Exercise the production ELF guard at the ExtraFS boundary and on malformed input."""
from pathlib import Path
import importlib.util
import struct
import tempfile

root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('flash_guard',root/'scripts/check_repeater_flash.py')
guard=importlib.util.module_from_spec(spec);spec.loader.exec_module(guard)


def image(address,size,machine=40):
    identity=b'\x7fELF\x01\x01\x01'+bytes(9)
    header=struct.pack('<16sHHIIIIIHHHHHH',identity,2,machine,1,guard.APP_START,52,0,0,52,32,1,0,0,0)
    load=struct.pack('<IIIIIIII',1,84,address,address,size,size,5,4)
    return header+load+bytes(size)


with tempfile.TemporaryDirectory(prefix='lrx-flash-guard-') as d:
    file=Path(d)/'test.elf'
    def check(data,okay):
        file.write_bytes(data)
        try:result=guard.check_image(file)
        except ValueError:
            assert not okay
        else:
            assert okay and result
    check(image(guard.APP_START,guard.EXTRAFS_START-guard.APP_START),True)
    check(image(guard.APP_START,guard.EXTRAFS_START-guard.APP_START+1),False)
    check(image(guard.EXTRAFS_START,4),False)
    check(image(guard.APP_START-1,4),False)
    check(image(0x20006000,4),False)
    check(image(guard.APP_START,4,machine=3),False)
    check(image(guard.APP_START,4)[:-1],False)
    check(image(guard.APP_START,4)[:60],False)
    check(b'not an ELF',False)
print('PASS production flash guard: exact ExtraFS boundary, overlap +1, below-app, missing flash, wrong architecture and truncation')
