#!/usr/bin/env python3
"""Compile the production paper UI with only hardware boundaries substituted.

The deterministic scheduler proves the radio-loop path never calls the panel;
it does not claim physical FreeRTOS, SPI, panel timing, or ghosting validation.
"""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='lrx-paper-') as directory:
    out = Path(directory)
    source = (root/'examples/companion_radio/ui-rx-paper/UITask.h').read_text()
    for include in ('../AbstractUITask.h', '../LoRagotchiRXBridge.h'):
        needle = f'#include "{include}"'
        assert source.count(needle) == 1
        source = source.replace(needle, '#include "paper_mocks.h"')
    (out/'UITask.h').write_text(source)
    (out/'freertos').mkdir()
    for header in ('FreeRTOS.h', 'queue.h', 'task.h'):
        (out/'freertos'/header).write_text('#pragma once\n#include "paper_mocks.h"\n')
    for transport in ('usb', 'ble'):
        binary = out/transport
        args = ['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                '-Wno-unused-parameter', '-Wno-misleading-indentation',
                '-DLORAGOTCHI_RX_EINK_TASK=1', '-I'+str(out),
                '-I'+str(root/'test/rx'), '-I'+str(root/'src'),
                '-I'+str(root/'examples/companion_radio/ui-rx-paper'), '-I'+str(root)]
        if transport == 'ble': args += ['-DBLE_PIN_CODE=123456']
        subprocess.run(args+[str(root/'test/rx/check_paper.cpp'), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
