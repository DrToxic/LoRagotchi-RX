#!/usr/bin/env python3
"""Run the actual Aquarium client/store against the production C++ RX core.

Flash/radio are mocked; framed pipe I/O, fragment reads and SQLite are real.
No installed graphics dependencies, radio access or application edits required.
"""
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
aquarium = Path(sys.argv[1] if len(sys.argv) > 1 else root.parent / 'loragotchi-community-tank').resolve()
if not (aquarium / 'tank/client.py').exists():
    raise SystemExit('Pass the path to a LoRagotchi-Aquarium v0.1.5 checkout')
sys.path.insert(0, str(aquarium))
from tank.client import Client
from tank.protocol import decode_event, encode_event
from tank.store import Store
from tank.transport import StreamTransport

with tempfile.TemporaryDirectory(prefix='loragotchi-rx-test-') as directory:
    temp = Path(directory)
    def compile_fixture(name):
        binary = temp / name
        subprocess.run(['g++', '-std=c++17', '-I', str(root),
                        str(root / 'test/rx' / (name + '.cpp')), '-o', str(binary)], check=True)
        return binary
    golden = temp / 'event.bin'
    subprocess.run([str(compile_fixture('check_receiver')), str(golden)], check=True)
    event = decode_event('03' + '00' * 31, golden.read_bytes())
    assert encode_event(event) == golden.read_bytes()
    assert (event.fish_id, event.tx_id, event.body, event.scales, event.eyes) == (8, 7, 1, 3, 5)
    print('PASS Aquarium decodes and round-trips production snapshot bytes')

    process = subprocess.Popen([str(compile_fixture('host_receiver'))],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    class Pipe(StreamTransport):
        def read(self):
            if select.select([process.stdout], [], [], .1)[0]:
                # Force partial transport frames, not just complete pipe reads.
                return os.read(process.stdout.fileno(), 3)
            return b''
        def write(self, data):
            process.stdin.write(data[:2]); process.stdin.flush()
            process.stdin.write(data[2:]); process.stdin.flush()
        def close(self):
            process.stdin.close()
    class CrashAfterSave(Store):
        fail_once = True
        def save(self, event):
            result = super().save(event)
            if self.fail_once:
                self.fail_once = False
                raise RuntimeError('simulated PC failure after durable save, before ACK')
            return result
    transport = Pipe()
    store = CrashAfterSave(temp / 'aquarium.sqlite')
    client = Client(transport, store, token=45)
    try:
        assert client.connect() == dict(capacity=4, transports=1, storage_ok=True)
        try:
            client.poll()
        except RuntimeError as exc:
            assert 'before ACK' in str(exc)
        else:
            raise AssertionError('save/ACK boundary was not exercised')
        assert len(store.roster()) == 1
        client.command(5)  # drop old lease; simulate a new PC client
        client = Client(transport, store, token=46)
        client.connect()
        count = 0
        while client.poll():
            count += 1
        assert count == 4 and len(store.roster()) == 2
        assert store.db.execute('SELECT COUNT(*) FROM events').fetchone()[0] == 4
        assert store.db.execute('SELECT COUNT(*) FROM returns WHERE status="delivery-confirmed"').fetchone()[0] == 2
        assert all((fish.body, fish.scales, fish.eyes) == (1, 3, 5) for fish in store.roster())
        client.poll_messages()
        assert len(store.roster()) == 2
        print('PASS fragmented LRX/1 handshake/read, two owners, durable replay without clones')
        print('PASS independent return statuses keep both copies; ordinary inbox remains separate')
    finally:
        client.close()
        process.wait(timeout=3)
        store.close()
    assert process.returncode == 0
