#!/usr/bin/env python3
"""Host-check production boot selection and DX PIN rendering, not radio entropy.

No firmware toolchain, hardware, pairing requests, or stored settings are used.
An optional main.cpp path permits checking that the pre-fix code fails.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
source = (Path(sys.argv[1]) if len(sys.argv) > 1 else
          root / "examples/companion_radio/main.cpp").read_text()
start = source.index("#ifdef BLE_PIN_CODE", source.index("void setup()"))
end = source.index("// add bluetooth interface", start)
selection = source[start:end]

prelude = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include "examples/companion_radio/LoRagotchiBLEPin.h"
struct Prefs { uint32_t ble_pin=0; };
struct Mesh {
  Prefs prefs; uint32_t pin=0;
  Prefs* getNodePrefs() { return &prefs; }
  void setBLEPin(uint32_t p) { pin=p; }
  uint32_t getBLEPin() { return pin; }
} the_mesh;
static int hardwareReads=0, softwareReads=0;
static std::vector<uint32_t> samples;
struct Radio {} radio;
struct RadioNoiseListener {
  explicit RadioNoiseListener(Radio&) {}
  void random(uint8_t* data, size_t len) {
    assert(len==sizeof(uint32_t)); assert(!samples.empty());
    ++hardwareReads;
    memcpy(data, &samples.front(), len); samples.erase(samples.begin());
  }
};
struct StdRNG {
  uint32_t nextInt(uint32_t low, uint32_t high) {
    ++softwareReads; assert(low==100000 && high==999999); return 345678;
  }
};
void boot(bool displayReady) {
  (void)displayReady;
#ifdef DISPLAY_CLASS
  int dummy=0;
  void* disp=displayReady ? &dummy : nullptr;
  (void)disp;
#endif
'''
tests = r'''
}
int main() {
  // Saved choices win even over build defaults, including deliberately saved
  // 123456 and small values which the display must zero-pad. Never rewrite prefs.
  for (bool displayReady : {false,true}) {
    for (uint32_t saved : {123456u,654321u,42u}) {
      the_mesh.prefs.ble_pin=saved; samples.clear();
      boot(displayReady);
#ifdef BLE_PIN_CODE
      assert(the_mesh.pin==saved);
#else
      assert(the_mesh.pin==0);
#endif
      assert(the_mesh.prefs.ble_pin==saved);
      assert(hardwareReads==0 && softwareReads==0);
    }
  }
  the_mesh.prefs.ble_pin=0;
  for (bool displayReady : {false,true}) {
#if defined(BLE_PIN_CODE) && BLE_PIN_CODE == 123456 && (defined(LORAFISH_UI) || defined(LORAGOTCHI_RX))
    // Rejection then lowest PIN, highest PIN, and another independent boot.
    samples={0,900000,899999,1000000};
    int before=hardwareReads;
    boot(displayReady); assert(the_mesh.pin==100000);
    assert(hardwareReads==before+2);
    boot(displayReady); assert(the_mesh.pin==999999);
    boot(displayReady); assert(the_mesh.pin==200000);
    assert(samples.empty() && hardwareReads==before+4 && softwareReads==0);
    // Valid extremes of the 32-bit source also remain six digits.
    samples={UINT32_MAX};
    boot(displayReady);
    assert(the_mesh.pin>=100000 && the_mesh.pin<=999999 && samples.empty());
#else
    boot(displayReady);
#ifndef BLE_PIN_CODE
    assert(the_mesh.pin==0);
#elif defined(DISPLAY_CLASS) && BLE_PIN_CODE == 123456
    assert(the_mesh.pin==(displayReady ? 345678u : 123456u));
#else
    assert(the_mesh.pin==BLE_PIN_CODE);
#endif
    assert(hardwareReads==0);
#endif
    assert(the_mesh.prefs.ble_pin==0); // automatic PIN is session state only
  }
}
'''


def function(text, marker):
    start = text.index(marker)
    opening = text.index("{", start)
    end, depth = opening + 1, 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


with tempfile.TemporaryDirectory(prefix="loragotchi-pin-") as directory:
    out = Path(directory)
    cpp = out / "boot.cpp"
    cpp.write_text(prelude + selection + tests)
    count = 0
    for role in ("LORAFISH_UI", "LORAGOTCHI_RX", None):
        for display in (False, True):
            for pin in (123456, 654321, 0, None):
                flags = []
                if role:
                    flags.append(f"-D{role}=1")
                if display:
                    flags.append("-DDISPLAY_CLASS=1")
                if pin is not None:
                    flags.append(f"-DBLE_PIN_CODE={pin}")
                binary = out / f"boot-{count}"
                subprocess.run(["g++", "-std=c++11", "-Wall", "-Wextra",
                                "-Werror", "-I" + str(root), *flags,
                                str(cpp), "-o", str(binary)], check=True)
                subprocess.run([str(binary)], check=True)
                count += 1
    # Exercise the real DX rendering function: the active value, never a literal
    # default or a second random selection, is what the user sees.
    ui = (root / "examples/companion_radio/ui-new/UITask.cpp").read_text()
    rendering = function(ui, "static void drawLoRaFishPairing(")
    cpp.write_text(prelude[:prelude.index("void boot(")] + r'''
struct DisplayDriver {
  std::vector<std::string> lines;
  int width(){return 128;}
  void setColor(int){} void setTextSize(int){}
  void drawTextCentered(int,int,const char* text){lines.emplace_back(text);}
};
namespace UIColor { const int primary_txt=1, secondary_txt=2; }
void drawLoRaFishHeader(DisplayDriver&,const char*,const char*){}
''' + rendering + r'''
int main() {
  for(uint32_t pin : {100000u,999999u,654321u,123456u,42u}) {
    DisplayDriver display; the_mesh.pin=pin;
    drawLoRaFishPairing(display);
    char expected[16]; snprintf(expected,sizeof(expected),"%06lu",(unsigned long)pin);
    assert(std::find(display.lines.begin(),display.lines.end(),expected)!=display.lines.end());
    assert(hardwareReads==0 && softwareReads==0);
  }
}
''')
    binary = out / "display"
    subprocess.run(["g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                    "-I" + str(root), str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print(f"PASS: {count} boot configurations and active-PIN rendering (host mocks)")
