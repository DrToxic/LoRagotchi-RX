# LoRaGotchi DX 🐟📡

**LoRaGotchi DX v1.0 — M0YNW** is a virtual LoRa fish built on the MeshCore transport. Your fish grows through pet stages, lives in a small aquarium UI, and can travel over the mesh to visit another LoRaGotchi device before being recalled home.

For setup, controls, menus, pet behavior, and DX travel, see [Getting Started](Getting_Started.md).

## v1.0 highlights
- LoRaGotchi virtual-pet aquarium and evolution/gameplay
- DX fish visiting using the LFG1 protocol
    - LFG1 protocol upgraded to include colour information
    - Improved the visiting and eating animations
    - Send, acknowledge, commit, arrival confirmation and recall/return flow
    - Host and visiting fish can coexist and swim independently
    - Visiting fish keeps its own evolution stage
    - A fish away from it's own tank will no longer poop in it's own tank
    - Travel is locked while the local fish is asleep
    - DX session state is cleared on reboot as a recovery mechanism
- LoRaGotchi / M0YNW boot branding
- MeshCore remains the underlying radio/mesh transport
- Buzzer support
- Colour screen support
- Higher/Lower game - 5 rounds; each correct guess adds 1 Happy (max +5 per game)

## Testing features:
- Emoji-safe names and fish-marked DX discovery
    - Fish markers 🐟 🐠 🐡 🦈 render as `><>`; unsupported Unicode renders as spaces
    - The DX picker lists only saved companion contacts with a fish marker in their name
    - Dismissible boot reminder for unmarked names; no automatic renaming
- LoRagotchi-local flash and display savings
    - Five-minute passive-counter checkpoints; immediate care/lifecycle/settings saves
    - Identical-record suppression and bounded retries after failed file writes
    - Shared five-second battery display cache using MeshCore's existing readings
    - Unchanged static menus skip redraws; dimmed animation keeps its normal cadence
    - Shared MeshCore, radio, board power configuration and save formats unchanged
- Lifecycle improvements, basic fish-colour DNA, breeding & self-fertilization
    - Your fish now has generations > everyone starts with a 1st gen fish. old-age death no longer ends the LoRagotchi game
    - Your game will ask for what colour your fish should start as. breeding can evolve the fish.
- happiness re-balance
    - RF interactions count toward meals, without a direct Happy bonus
    - +3 Happy per ten-event meal, limited to one meal per hour; no queued backlog
    - -1 Happy per 30 awake minutes, plus -1 per waste piece at the same interval
    - Scheduled sleep, eggs and Memorial pause decay; turning the screen off does not
    - Cleaning stays +3; new generations start at 80; Happy stays between 0 and 100
- M2 Partition change - dropped 2nd app partition in favour of a larger primary app partition.
    - alleviates flash from 95.5% to about 42%, in line with other nodes.
    - retained NVS, Filesystem and Coredump partitions.
    - Disables OTA updates (not used or planned)
- M5Stack CardPuter
    - Updated graphics and keyboard controls for the menu
- LilyGo T-Deck
    - Framebuffer for the display.
 
  
## Supported LoRaGotchi targets in this tree

| MeshCore Node | Build Environment |
| --- | --- |
| ThinkNode M2 | ThinkNode_M2_LoRaFish_companion_ble |
| Heltec V3 | Heltec_v3_LoRaFish_companion_ble |
| Heltec V4 | heltec_v4_LoRaFish_companion_ble |
| M5Stack CardPuter | cardputer_loragotchi |
| Heltec T114 | Heltec_t114_LoRaFish_companion_ble -t create_uf2 |
| Heltec T096 | Heltec_t096_LoRaFish_companion_ble -t create_uf2 |
| LilyGo T-Deck | TDeck_Loragotchi_dx_ble |

| Untested nodes | build Environment |
| --- | --- |
| T-Beam (SX1262) | Tbeam_SX1262_LoRaFish_companion_ble
| T-Beam (SX1276) | Tbeam_SX1276_LoRaFish_companion_ble

## Building

Install PlatformIO, clone the repository, then build the required environment from the repository root. For example:

```bash
git clone <your-repository-url>
cd LoRaGotchi-DX
pio run -e [BUILD ENVIRONMENT]
```
⚠️ The build environment is Case Specific!

Examples:
```bash
git clone <your-repository-url>
cd LoRaGotchi-DX
pio run -e Heltec_t114_LoRaFish_companion_ble -t create_uf2
```
The resulting file will end up in `LoRagotchi-DX-main\.pio\build\Heltec_t114_LoRaFish_companion_ble\firmware.uf2`
NRF52 devices, After the build, enter the board's UF2 bootloader and copy `firmware.uf2` to its mounted drive. 
```bash
git clone <your-repository-url>
cd LoRaGotchi-DX
pio run -e heltec_v4_LoRaFish_companion_ble
```
The resulting file will end up in `LoRagotchi-DX-main\.pio\build\heltec_v4_LoRaFish_companion_ble\firmware.bin`
Upload with PlatformIO using the normal `-t upload` target for supported serial-programmed boards. The T096 uses -t create_uf2 and the UF2 procedure above.
the meshcore.io/flasher also works, scroll to the bottom and select custom device firmware!

### Buzzer pins

LoRaFish buzzer outputs are assigned per board:

| Device | Buzzer pin |
| --- | --- |
| ThinkNode M2 | GPIO5 |
| Heltec WiFi LoRa 32 V3 | GPIO6 |
| Heltec WiFi LoRa 32 V4 OLED | GPIO6 |
| Heltec T114 | GPIO46 / P1.14 |
| Heltec T096 | GPIO45 / P1.13 |

The project tree also retains the wider upstream MeshCore targets and infrastructure from which LoRaGotchi is derived.

## Project history

Development notes from the experimental LoRaFish/LoRaGotchi builds are retained under [`docs/development/`](docs/development/) for reference. The v1.0 release note is under [`docs/release/`](docs/release/).

## Credits

LoRaGotchi DX is by **Robert Rayner / M0YNW** and is based on **MeshCore** by Scott Powell / Ripple Radios. MeshCore provides the underlying mesh transport and substantial portions of the firmware foundation.

See [`NOTICE`](NOTICE) for attribution details.

## License

MIT. See [`LICENSE`](LICENSE).
