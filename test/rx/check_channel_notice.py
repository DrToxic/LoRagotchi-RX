#!/usr/bin/env python3
"""Exercise the actual compact RX UI on the smallest 128x64 display."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="rx-channel-notice-") as directory:
    out = Path(directory)
    source = (root/"examples/companion_radio/ui-rx/UITask.h").read_text()
    for include in ("../AbstractUITask.h", "../LoRagotchiRXBridge.h"):
        source = source.replace(f'#include "{include}"', '#include "paper_mocks.h"')
    source = source.replace("../LoRagotchiChannelStatus.h",
                            "examples/companion_radio/LoRagotchiChannelStatus.h")
    (out/"UITask.h").write_text(source)
    for transport in ("usb", "ble"):
        binary = out/transport
        args = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-Wno-unused-parameter", "-Wno-misleading-indentation",
                "-I"+str(out), "-I"+str(root/"test/rx"),
                "-I"+str(root/"src"), "-I"+str(root)]
        if transport == "ble":
            args += ["-DBLE_PIN_CODE=123456"]
        subprocess.run(args+[str(root/"test/rx/check_channel_notice.cpp"),
                             "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
