"""Fail a new T096 tank build before its application can overlap RX ExtraFS.

Stock's linker range extends to 0xED000; the unchanged RX ExtraFS starts at
0xD4000. Do not silently change either layout: validate the actual ELF LOAD file
ranges, including initialized data's flash load address. No external packages.
"""
from pathlib import Path
import struct

APP_START=0x26000
EXTRAFS_START=0xD4000


def check_image(path):
    data=Path(path).read_bytes()
    if len(data)<52 or data[:7]!=b'\x7fELF\x01\x01\x01':
        raise ValueError('Expected a 32-bit little-endian ELF')
    header=struct.unpack_from('<16sHHIIIIIHHHHHH',data)
    if header[2]!=40: raise ValueError('Expected an ARM application ELF')
    offset,stride,count=header[5],header[9],header[10]
    if stride<32 or offset+stride*count>len(data): raise ValueError('Truncated ELF program headers')
    ranges=[]
    for i in range(count):
        kind,file_offset,virtual,physical,size,mem_size,flags,align=struct.unpack_from('<IIIIIIII',data,offset+i*stride)
        if kind!=1 or not size:continue
        if file_offset+size>len(data):raise ValueError('Truncated ELF load data')
        if physical>=0x10000000:continue  # non-flash load regions
        if physical<APP_START or physical+size>EXTRAFS_START:
            raise ValueError(f'Application flash [0x{physical:x},0x{physical+size:x}) overlaps reserved storage/boot area')
        ranges.append((physical,physical+size))
    if not ranges:raise ValueError('No application flash load ranges')
    return ranges


if 'Import' in globals():
    Import('env')
    def verify_repeater_flash(source,target,env):
        ranges=check_image(env.subst('$BUILD_DIR/${PROGNAME}.elf'))
        print(f'PASS repeater flash guard: application end 0x{max(end for _,end in ranges):x} <= ExtraFS 0x{EXTRAFS_START:x}')
    env.AddPostAction('$BUILD_DIR/${PROGNAME}.elf',verify_repeater_flash)
elif __name__=='__main__':
    import sys
    print(check_image(sys.argv[1]))
