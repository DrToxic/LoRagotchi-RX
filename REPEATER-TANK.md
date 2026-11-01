# Stock repeater + durable Aquarium connector

Local feature, based on MeshCore stable **1.18.0** and RX Water bf20da1
(includes unpublished busy-mesh 04271a5 and Water 27e59c1). Not a release,
physical qualification, flash recommendation or upstream change.

## Architecture

The radio runs the **actual stock simple_repeater/MyMesh.cpp**, byte-identical to
MeshCore 81dbae592ffad40b4212bc0b37e82b38da08f4bc. Stock forwarding, region rules,
repeater advertisement role, ACL, over-radio admin and USB CLI remain in that class.
One guarded protected default-scope send helper is the only change to its header;
shared MeshCore, board pins, RF defaults and partitions are untouched.

TankMesh subclasses it with a separate 16-entry signed-advert/ECDH peer cache,
never adding application peers to the admin ACL. Durable-owner keys are pinned
and recovered from the journal after reboot without another advert. Only plain
LFG1 text reaches the existing production receiver. Other traffic from an existing
ACL peer still reaches the stock admin handler exactly once. Same-prefix collisions
are resolved by full-secret decryption, never names/short-prefix fallback. PATH
packets learn return routes; reboot uses unknown routes and the default flood
scope. Incoming flood ACK/path replies use the stock request scope. Application
output backpressure leaves capacity for normal forwarding/admin. Existing bounded
return-attempt semantics, including failed enqueue attempts, remain unchanged.

The production Water/legacy receiver and verified alternating journals are reused
**unchanged**: four intake/outbox/return slots, save-before-ARRIVED and export,
four-second hold, full owner/transaction/fish binding, bounded retries, AQUARIUM1
only after durable capture, recent tombstones and v1 journal migration. Tank
connectivity and upload ACKs are never prerequisites for returning a captured pet.
Returned copies remain until durable PC ACK. Four slots limit offline backlog,
not PC population. Unresolved returns are retained, never silently evicted.

## Discovery and sender compatibility

The endpoint honestly advertises **ADV_TYPE_REPEATER**, not a chat companion.
Default name: "LRX1 Tank 🐟". Updated DX selection accepts saved fish-marked chat
peers as before, plus fish-marked repeaters with the exact "LRX1 " prefix. The
small sender change is separate in DrToxic's DX fork, local branch
feature/repeater-tank-destinations (base 856d420). It must be integrated/built for
the sender before its normal picker can select this endpoint. Unmodified legacy
DX UI filters repeaters out; wire compatibility alone does not fix its picker.
Names are opt-in discovery hints, not security attestation; visits still bind to
the selected full key and use the normal LFG1 handshake.

Keep the marker/emoji when renaming. If an old name is retained, use the stock
USB CLI command "set name LRX1 Tank 🐟" and advertise normally. Square brackets
were avoided because the stock name CLI rejects them. The receiver must hear the
sender's signed chat advert before the first visit, as with ordinary companion
peer discovery; active owner keys survive cache churn and reboot.

## Explicit target matrix

Append the listed suffix to the prefix; one compilation job only:

| Board | Environment prefix | Suffixes | Actual MCU/USB |
|---|---|---|---|
| Heltec V3 | Heltec_v3_LoRagotchi_Repeater | _usb, _ble, _wifi | ESP32-S3; CP2102 UART, 115200 |
| Heltec V4 **OLED** | heltec_v4_LoRagotchi_Repeater | _usb, _ble, _wifi | ESP32-S3; stock native USB CDC |
| Heltec T096 | Heltec_t096_LoRagotchi_Repeater | _usb, _ble | nRF52840 native CDC/SoftDevice BLE; **no Wi-Fi radio** |

These inherit the exact stock repeater board profiles, not new pin maps. V4 TFT,
expansion variants, T114, M2, Paper and T-Beams are NOT qualified repeater-tank
products here; their older companion RX profiles are unchanged. An owner-reported
"V4.3" label does not prove OLED/partition compatibility; inspect before flashing.
Heltec hardware comparison: https://docs.heltec.org/en/node/esp32/wifi_lora_32/index.html

Example: pio run -e Heltec_v3_LoRagotchi_Repeater_usb -j 1
Matrix: test/rx/repeater-targets.json. The existing default and CI matrices are not
silently switched. Separate images avoid BLE/Wi-Fi coexistence and USB ownership
conflicts. Simultaneous RS232/ESP-NOW/Ethernet bridges are compile-rejected.

These continuously available tank profiles intentionally suppress stock idle sleep
without rewriting the saved power preference (otherwise a session or return can
wait through a 30-second sleep). Stock routing still honors saved repeat settings:
a non-forwarding companion migration may require deliberate "set repeat on".
Do not change RF blindly; this patch preserves the fork's RF defaults/settings.

## Tank transports and management

All LRX requests/replies are <=20 bytes. USB/TCP retain the existing little-endian
length framing; BLE retains one Nordic UART GATT value per request/notification.
Incremental fixed-size stream state handles partial reads/writes and backpressure.
USB CLI remains CR-terminated, bounded and no longer input-echoed, so provisioning
secrets are not printed. Close Aquarium before managing the same USB port. Only
the USB image accepts binary LRX on that port; BLE/Wi-Fi images use USB for CLI.
The remote tank protocol never exposes the CLI or companion control commands.

HELLO's link-flags byte gains **0x80 = LRX-only**, alongside 1 USB / 2 BLE / 4 Wi-Fi.
The updated Aquarium adapter keeps durable intake but suppresses companion inbox,
chat/config commands and network bridge-capability queries. Its Radio panel
identifies the repeater role instead of misreporting a BLE MTU failure. Stock
USB CLI and authenticated over-radio admin remain the management interfaces.
The BLE tank service is NOT MeshCore companion management. Existing companion
capabilities, Water negotiation and event formats are unchanged.

BLE reuses the encrypted/MITM-protected Nordic UART transport. A hardware-noise
six-digit PIN is selected per boot, displayed for 15 seconds and printed on physical
USB. "tank pin" shows it again. There is no shared fallback if the display fails.
A non-default LRX_TANK_PIN build flag is an explicit fixed PIN. This new repeater
tank service does not import companion BLE preferences or offer phone admin;
existing platform bonds are not deliberately erased.

## Onboard Wi-Fi security and provisioning

ESP32 STA-only; **disabled/unconfigured by default**, no AP or anonymous listener.
Use a private physical USB console (values are never echoed or returned):

- tank ssid YOUR_SSID (1..32 bytes)
- tank password YOUR_WIFI_PASSWORD (8..63 bytes; protected WLAN)
- tank token YOUR_64_HEX_DIGIT_TOKEN (32 random bytes, hex input)
- tank wifi on, then deliberately reboot to apply
- tank wifi off, then reboot to disable without erasing the journal

Credentials live in **unencrypted ESP32 NVS**, namespace lrx-net, never source/history.
Do not provision through a shared/recorded console. This task creates no credential.
Create a private 32-byte random token file (mode 0600) on the Aquarium host; provision
the hexadecimal encoding of those same bytes on the radio. The Aquarium Network
connection uses tcp:HOST:8765 and that **raw 32-byte file**, not a 64-character hex file.

The server speaks existing LRXB + random 32-byte challenge, HMAC-SHA256 proof,
OK/NO. One TCP client; a newcomer cannot replace it. Five-second authentication
deadline; constant-time proof comparison; partial nonblocking socket sends prevent
a stalled aquarium waiting in the radio loop. An authenticated idle TCP connection
remains owned until disconnect; the independent LRX lease still expires after
15 seconds. Its 32-bit lease token is arbitration, not a security credential.

HMAC authenticates clients but **does not encrypt or integrity-protect later TCP
frames**, or authenticate the server to the client. This is the existing PC bridge
trust boundary: trusted protected LAN/WLAN or VPN only, never public port-forwarding.
USB is physically trusted; BLE pairing is required. LFG1 retains MeshCore's existing
crypto/trust limits. AQUARIUM1 is not attestation that a PC stored the copy.

On blank installation, provision a private stock admin password through USB before
remote administration. New profiles use an empty initial admin password (no
password-based remote enrollment), not stock shared "password". Saved stock
passwords and ACLs are preserved; unsafe saved passwords are the operator's
responsibility. Do not disclose the inherited CLI's password reply in logs.

## Storage, migration and acceptance limits

Journal filenames/regions match RX: ESP32 SPIFFS, nRF ExtraFS at 0xD4000/0x19000.
No autoformat. Invalid journals block intake/export while stock admin/forwarding
continues. A failed nRF ExtraFS mount likewise leaves the identity-bearing stock
repeater running with unhealthy LRX storage. A primary identity-filesystem mount
failure halts safely instead of erasing data or generating another identity. Blank
devices need deliberate compatible filesystem provisioning, not inferred erasure.

Back up identity/preferences/pending jobs before any later role migration. Stock
and companion share /prefs.json but differ in schema: common settings load, but
saving stock settings may discard companion-only fields. This patch does not
promise reversible role migration or change partitions. Legacy companion profiles
and journal formats are untouched. Older RX safely rejects newer Water journals;
do not assume downgrade can export them.

Run before building:
python3 test/rx/check_repeater.py /path/to/pio /path/to/updated-aquarium

Also run existing receiver/journal/Aquarium integration, lost-first rich COMMIT,
native MeshCore suites, old target config checks and updated DX picker tests.
The production-handler ASan/UBSan harness exercises ACL isolation, same-prefix/full-
key binding, pinned peers, offline returns, routes, packet exhaustion, reboot,
corruption, actual Water/legacy SQLite replay, framing and LRXB authentication
states. Radio/flash/crypto primitives are mocked: not crypto, power-cut, USB/GATT,
Wi-Fi association, RF airtime, display responsiveness or multi-hop device proof.
Exact build coverage is recorded separately; configuration resolution is not a build.

No push, CI trigger, upstream change, release, tag or flash is authorized here.
Hardware acceptance must cover forwarding/admin alongside visits, offline/reconnect,
power loss, pairing/authentication, stalled sockets, route loss and board-specific
USB reset behavior. HA's separately version-locked MeshCoMod overlay is unchanged;
this is not a substitute for its HACS/multi-client companion API.
