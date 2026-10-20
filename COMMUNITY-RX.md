# Receiver setup and test handoff

## Source and separation

RX starts from DrToxic/LoRagotchi-DX `9619fa27d0b8a249433d8356575252cde11c5ffa`
(the same source tree as accepted `6a7b41a`). It extracts the receiver portion of
the local `af5309a` community prototype. Normal-DX Send visitor home, routing-key,
and UI changes from that prototype are deliberately **not** part of this port;
the sender firmware is being developed separately.

No shared MeshCore source, radio defaults, pins, partitions, or pet save formats
are changed. RX builds select a dedicated status UI without `LORAFISH_UI`.
No local pet is instantiated. Do not flash an inherited DX environment expecting RX.

## Preparing a receiver

1. Use a listed companion board with a known-good MeshCore/LoRagotchi setup.
   ESP32 needs mountable SPIFFS; nRF52 needs mountable InternalFS and ExtraFS.
   Back up identity, contacts and settings using your normal tools. RX never
   formats storage automatically; an unprovisioned board needs the normal board
   preparation procedure. A failed filesystem mount halts startup; corrupt journal
   banks show storage-not-ready. Neither silently erases the recovery journal.
2. Choose the exact board's USB or BLE image, not both. ESP32 application-only binaries use the
   generated application partition offset (currently `0x10000`); a different
   board/partition layout is not interchangeable. nRF52 uses its board-specific UF2 bootloader file. Keep the accepted DX image for
   rollback. No guarantee of data preservation follows merely from compilation.
3. Configure regional/matching RF settings and participant contacts with normal
   MeshCore tools. This build does not change existing RF settings. Fresh build
   defaults are inherited from DX and are not a universal regional preset.
4. Ensure the receiver's actual advertised name has 🐟, 🐠, 🐡 or 🦈. The default
   `Community Tank 🐟` only applies without saved preferences. Advertise after
   renaming and save the receiver as a companion contact on each DX device.
5. Disconnect the management app before Aquarium connects. USB uses 115200 baud.
   BLE pairing PIN is shown on the receiver. The aquarium negotiates LRX/1 and
   rejects stock companions; debug serial text is not a fish-event interface.
6. For WLAN use the Aquarium project's authenticated TCP bridge on a PC/SBC
   connected to RX over USB or BLE. No onboard-WLAN endpoint is in this build.

## Lifecycle and bounds

REQ reserves one of four slots for 30 seconds. COMMIT records the complete owner
key, transaction/fish IDs, stage, name and optional genes before ARRIVED. A short
COMMIT retry cannot replace known genes with missing data. Duplicate messages do
not create another snapshot or restart the hold.

After four seconds RX sends RETURN C. Early RECALL accelerates it. There are at
most four scheduled attempts spaced at least at least ten seconds apart (or the longer radio timeout), independent of the PC;
radio acknowledgements indicate transport delivery, not a new application-level
HOME receipt. Captured jobs become due immediately after RX reboot. Failed returns
remain unresolved; LRX/1 op 6 allows an explicit retry (not yet an aquarium UI
control). Full or unhealthy storage responds BUSY to new requests.

PC snapshot and terminal return-status events have separate IDs and durable ACKs.
The PC saves SQLite before ACK. Delivered slots are reusable only after both event
ACKs; unresolved jobs are never silently evicted. Eight completed-transaction
records give bounded recent replay protection. This is not unlimited replay history.

Two checksum-protected journal banks (SPIFFS on ESP32, ExtraFS on nRF52) preserve the last verified generation
while writing the other. Flush/close/readback validation is used before promotion.
Only the inactive nRF52 bank is removed/recreated to avoid append semantics.
A torn latest bank falls back to an older valid bank; both-invalid fails closed.
Physical power-cut atomicity and flash endurance still require device testing.
READ and PEEK both refuse exports while storage is unhealthy.

## Hardware acceptance checklist

- Confirm RX screen, actual advertised fish-marked name, and USB/BLE connection.
- Send fish from two or more DX hosts; check the correct names/stages/colours.
- Verify each original returns after the short hold and its PC copy remains.
- Repeat a share: one updated copy per owner, no clones from retries.
- Disconnect the PC, share, allow return, then reconnect and verify replay.
- Fill four offline slots: a fifth visitor must receive BUSY; draining delivered
  snapshot/status events should permit further shares.
- Exercise early recall, lost ARRIVED/RETURN, late duplicates and contact removal.
  Unresolved returns must be visible, not silently discarded or sent to a namesake.
- Test RX resets during hold/retry, pairing/reconnect, and ordinary mesh inbox.
- Controlled power-cut testing needs backups and a dedicated test board; it has
  not been performed by host tests.

The firmware does not flash itself, rename saved devices, or connect to a PC
service on your behalf. No physical RF, pairing, power-cut, or device acceptance
is implied by the supplied builds.

## DX community-return marker

For a durably captured aquarium visit, RX sends
`LFG1|RETURN|tx|fish|C|AQUARIUM1`, matching the DX host-return contract. Legacy
owners ignore the suffix; updated DX owns reward eligibility and cooldown. RX
does not calculate or promise a reward. Cancelled reservations and returns before
any successful snapshot save use plain `C`, without `AQUARIUM1`. A validated
recovered snapshot remains eligible even if a later status write fails. The PC
need not be connected: this marker claims receiver acceptance, not PC persistence.

The existing LRX/1 PC event schema and version-1 journal layout are unchanged.

## Wireless Paper test addendum

Use `Heltec_Wireless_Paper_LoRagotchi_RX_usb` or `_ble` only for the
Wireless Paper V1.1/V1.1.1/V1.2 panel family (not V1.0, V3 or Vision Master).
Existing inherited E213 auto-detection, RF pins and SPIFFS partitioning are used.
The BLE image deliberately does not inherit the old companion profile's Wi-Fi
interface; WLAN still uses the authenticated PC/SBC bridge.

- Boot and verify the 250x122 landscape status layout and, for BLE, pairing PIN.
- Leave idle: no periodic panel refresh or animation should occur.
- Send a visit; status changes may be delayed/coalesced up to about 15 seconds,
  plus panel refresh time. The last fish ID remains after its original returns.
- Send several visits while the panel is visibly refreshing. Check each original
  still returns and the Aquarium receives each distinct copy. The four-second
  hold is a radio scheduling deadline, not a promise of exact over-the-air latency.
- Check short visits, offline queueing/reconnect and storage/failed-return status.
- Check ghosting after repeated visits. Panel refresh quality, worker stack usage
  and physical SPI/radio coexistence are not established by host tests.

No new journal writes are made for screen state, and no LFG1/LRX1 formats change.
There is no on-board swimming fish; the aquarium PC renders the animated copy.
