# LoRagotchi-RX

Receiver companion firmware for [LoRagotchi Aquarium](https://github.com/DrToxic/LoRagotchi-Aquarium).
Derived from [DrToxic/LoRagotchi-DX](https://github.com/DrToxic/LoRagotchi-DX),
with LoRagotchi DX by Robert Rayner / M0YNW and the MeshCore transport by
Scott Powell / Ripple Radios. Existing MIT notices and source history are retained.

RX has **no local pet**. It accepts a normal fish visit, durably captures its
appearance for the PC aquarium, and sends the original fish home after a
four-second hold. The aquarium keeps the display copy. Return does not depend on
the PC being connected or acknowledging the upload.

**Initial receiver build — hardware testing pending.** Compiling and host tests
are not over-the-air, pairing, or power-cut proof. Host-side DX changes are a
separate project; this repository does not introduce a new sender wire format.

## What it does

- Four concurrent intake/backlog/recovery slots, bound to full owner public keys.
- Existing LFG1 REQ/ACK/COMMIT/ARRIVED/RECALL/RETURN protocol; RETURN uses colour
  mode `C` even though the receiver's own screen is monochrome.
- Durable snapshot outbox, replay after reconnect, and independent return status.
- Four bounded return attempts, at least at least ten seconds apart (or the longer radio timeout); unresolved jobs remain visible.
- Dedicated status screen instead of aquarium, care, breeding, or pet setup.
- Ordinary MeshCore contact/channel inbox and radio configuration remain available.

Four slots are **not** a limit on the aquarium's population. Successfully returned
and PC-acknowledged transactions can be reclaimed for new visitors. With the PC
disconnected, four queued copies fill the backlog; new visits receive BUSY.

## Receiver targets

Every profile has separate `_usb` and `_ble` builds. T-Deck and Cardputer
standalone devices are excluded. USB is serial at 115200 baud; BLE is Nordic UART.

| Board | Environment prefix (append `_usb` or `_ble`) | Image |
| --- | --- | --- |
| Heltec V3 | `Heltec_v3_LoRagotchi_RX` | ESP32-S3 application BIN |
| Heltec Wireless Paper V1.1 / V1.1.1 / V1.2 | `Heltec_Wireless_Paper_LoRagotchi_RX` | ESP32-S3 application BIN |
| Heltec V4 OLED | `heltec_v4_LoRagotchi_RX` | ESP32-S3 application BIN |
| ThinkNode M2 | `ThinkNode_M2_LoRagotchi_RX` | ESP32-S3 application BIN |
| Heltec T096 | `Heltec_t096_LoRagotchi_RX` | nRF52840 UF2 |
| Heltec T114 with display | `Heltec_t114_LoRagotchi_RX` | nRF52840 UF2 |
| T-Beam SX1262 (experimental in DX) | `Tbeam_SX1262_LoRagotchi_RX` | ESP32 application BIN |
| T-Beam SX1276 (experimental in DX) | `Tbeam_SX1276_LoRagotchi_RX` | ESP32 application BIN |

Choose one transport image. BLE uses pairing and small LRX/1 fragments that fit
the default ATT payload. The aquarium's authenticated USB/BLE-to-TCP bridge can
carry this connection over **WLAN/LAN**; RX does **not** yet provide onboard Wi-Fi.
These profiles preserve their original DX board pins, RF defaults and layouts.
Inherited DX/MeshCore environments are not RX products. Build support is not a
claim of physical validation on every board; see the artifact manifest.

## Wireless Paper status display

The Wireless Paper profiles reuse the existing E213 panel auto-detection for the
V1.1 and V1.1.1/V1.2 family. Original V1.0 panels are not covered by these images.
The screen is a static connector dashboard, not an animated aquarium: queued
copies, pending/failed returns, connection method/PIN, storage health and the last
received fish ID. The last-fish line remains after return and PC upload, including
a restart while the journal is retained. It is not a claim that the PC has saved it.

Only changed display state causes a redraw, at most once every 15 seconds during
bursts. Idle screens are untouched. Updates run in a separate, single-owner
display task with a one-snapshot mailbox; slow panel BUSY waits do not run inside
the mesh loop. If the display task cannot be allocated, reception continues
without a synchronously blocking display fallback. No animation or countdown.
The other boards retain their existing UI. Physical refresh quality, ghosting,
USB/BLE pairing and concurrent RF operation still need device validation.

## Automatic community channel

At startup, LoRagotchi automatically joins **`#loragotchi-dx`** using the standard
MeshCore hashtag key. Saved channels load first. If already joined (including an
alias with the same key), no slot or flash write is added. Otherwise the first
empty slot is used and saved. Existing names, keys, Public and RF settings stay
unchanged; the firmware does not send a join message.

Hashtag keys are case-sensitive: use the exact lowercase spelling above on other
radios. If all slots are occupied, the firmware leaves every channel untouched
and displays a **No free channel slot** warning for `#loragotchi-dx`.
It never clears a slot automatically. On RX, the warning stays visible alongside queue,
pairing and storage status (including Wireless Paper). If you choose to free a slot in your MeshCore
management app, restart afterward to retry joining. A channel already named
exactly `#loragotchi-dx` but using a different key
is preserved instead of overwritten; export it before correcting or removing it
in the app, then restart. Removing the automatic channel makes it return at the
next startup if space is available. A failed storage write may require another
startup to persist it. Ordinary channel chat is separate from fish transfers.


## Build and connect

```sh
python3 -m venv .venv
# Linux/macOS; on Windows use .venv\Scripts\activate
. .venv/bin/activate
python -m pip install platformio==6.1.19
pio run -e Heltec_v3_LoRagotchi_RX_usb -j 1
# Or BLE: pio run -e Heltec_v3_LoRagotchi_RX_ble -j 1
# nRF52: pio run -e Heltec_t096_LoRagotchi_RX_usb -t create_uf2 -j 1
```

Output: `.pio/build/<environment>/firmware.bin` for ESP32 or `firmware.uf2`
for nRF52. BIN files are **application-only**, not offset-zero installers. Use the
artifact's partition-derived instructions and preserve a known-good rollback image
and backup. RX requires existing mountable SPIFFS (ESP32) or InternalFS/ExtraFS
(nRF52) filesystems and never auto-formats a failed mount. See [receiver setup and hardware checks](COMMUNITY-RX.md).

Using normal MeshCore management tools, configure the same RF settings as the
participants and save their companion contacts. Saved settings take precedence
over defaults. The fresh name is `Community Tank 🐟`; ensure the actual RX name
contains a fish marker and send an advert so DX participants can discover it.
Disconnect the management client, then select USB or BLE under **RX Radio** in
the aquarium application. Only one active aquarium consumer is supported.

## Development

```sh
mkdir -p .local-dev
python3 test/channels/check_autojoin.py # g++ and OpenSSL development headers
g++ -std=c++17 -Wall -Wextra -Werror -Wno-misleading-indentation -I . \
  test/rx/check_receiver.cpp -o .local-dev/check_receiver
.local-dev/check_receiver
g++ -std=c++17 -I . test/rx/check_journal.cpp -o .local-dev/check_journal
.local-dev/check_journal
python3 test/rx/check_targets.py
python3 test/rx/check_channel_notice.py
python3 test/rx/check_paper.py
python3 test/rx/check_aquarium.py ../loragotchi-community-tank
pio test -e native -e native_kiss_modem
```

[LRX/1 protocol](docs/RX-PROTOCOL.md) is the versioned PC handoff contract used by
Aquarium v0.1.5. Source directories: `examples/companion_radio/LoRagotchiRX*`
(state machine and storage/radio adapter), `ui-rx/` (status screen), and `test/rx/`.
Shared MeshCore `src/`, board pins, RF defaults, and partition layouts are unchanged.
The original pet-firmware README is preserved as [README-DX.md](README-DX.md).
Inherited release/site automation is archived under `.github/inherited-workflows/`;
active CI checks only RX and native tests, with no release or deployment step.

## DX community-return marker

For a durably captured aquarium visit, RX sends
`LFG1|RETURN|tx|fish|C|AQUARIUM1`, matching the DX host-return contract. Legacy
owners ignore the suffix; updated DX owns reward eligibility and cooldown. RX
does not calculate or promise a reward. Cancelled reservations and returns before
any successful snapshot save use plain `C`, without `AQUARIUM1`. A validated
recovered snapshot remains eligible even if a later status write fails. The PC
need not be connected: this marker claims receiver acceptance, not PC persistence.

The existing LRX/1 PC event schema and version-1 journal layout are unchanged.
