> This is the inherited **DX pet-firmware** guide. For this receiver project, use
> [LoRagotchi-RX setup](COMMUNITY-RX.md) and the [RX README](README.md).

## Getting Started with LoRaGotchi DX

Thank you for downloading and trying our firmware! LoRaGotchi DX is a virtual pet running as a MeshCore BLE companion. The firmware uses the device's LoRa radio and display for the aquarium, and can exchange visiting fish with compatible LoRaGotchi DX peers over the mesh.

It's actually built straight on the stock meshcore (1.17.1 at the time of writing) firmware with the idea that, you can flash to LoRagotchi-DX without losing your settings, channels, contacts etc. We've even flashed it via the meshcore.io flasher.

For supported boards and PlatformIO build commands, see the [README](README.md#supported-loragotchi-targets-in-this-tree). This guide describes the LoRaFish companion UI and behavior.

## First Use

0. Bringing a device already flashed with a MeshCore firmware is perfectly fine ☑️, though we do recommend making a backup first. don't erase when flashing to keep your settings, channels & ID
1. Flash the LoRaGotchi DX build for your board. The NRF52 builds produce a UF2 file; see its README instructions. Other target build commands are listed there.
2. Start the device and connect to it from the MeshCore app as a BLE companion. Use the PIN shown on the device's pairing screen when the app requests one.
3. Set the device clock from the app. LoRaGotchi uses MeshCore's clock for the displayed local time and scheduled sleep. If the clock has not been set to a valid date, the UI falls back to build time plus uptime; this fallback is not a substitute for setting the clock.
4. Use **Options → Schedule & time** to set your local UTC offset and sleep/wake hours.
5. On a color display, select Scales, Body, or Eyes on the new-fish screen, then hold to edit that trait. Click through colors and hold to save. Finally select **Start fish** and hold. Double-click goes back without starting the fish; setup remains available under **Options → Display → Fish color**. Monochrome displays skip this screen and start with monochrome traits.

Add **🐟, 🐠, 🐡, or 🦈** to your node name in the MeshCore app to opt in to
fish-marked DX discovery. The firmware never renames you automatically. If no
supported fish is present at boot, an advisory appears after the splash screen.
**Short-click to dismiss**; that click does not navigate, start a visit, or change
settings. The reminder stays dismissed for this boot and returns next boot if the
name is still unmarked. A sleeping display's first click only wakes it. The
first-eight-second long-press CLI rescue gesture is unchanged.

Small device fonts display these four fish as `><>`. Other unsupported Unicode
characters become spaces, and variation selectors/joiners are not printed as
garbage bytes. This applies to the local tank name, DX target/visitor names, and
LoRagotchi alerts. Stored names and advertised names keep their original UTF-8;
the bounded name in a DX request also keeps complete UTF-8 characters rather than
the display substitution. This is a simple-font fallback, not a full emoji font.

Pet age, RF-event count, happiness, and other pet state are saved across restarts. Display and schedule settings are also saved. DX travel is temporary: after a reboot, the local fish returns home and any visiting-fish slot is cleared.

Routine pet counters are checkpointed at most five minutes apart while the firmware
is running and storage is working. New traffic does not postpone that deadline.
Meals, care rewards, lifecycle transitions, completed DX visits and explicit settings
changes still request immediate saves. Identical records are not rewritten, and
ordinary delayed age updates are saved together. LoRagotchi-controlled shutdown or
restart flushes pending pet data. Abrupt power loss or a shared MeshCore reboot path
can roll passive counters and timing back by up to the checkpoint interval; flash
failures can extend that window. This is not a transactional or power-loss-safe save
format. Existing pet and settings formats are unchanged.

## Button Controls

Most nodes use a single user button. Hold it for about one second for a long-press. A click, double-click, long-press, and triple-click have different meanings depending on the screen.

| Gesture | General action |
| --- | --- |
| Short click | Next menu item, field, candidate value, or main screen. No settings are saved just by browsing. |
| Double-click | Back to the parent menu; cancel a pending value without saving. |
| Long-press | Select the highlighted item, save the candidate value, or perform the current screen's action. |
| Triple-click | Toggle global buzzer quiet mode when the board has a buzzer. |

During the first eight seconds after startup, long-press is reserved for MeshCore's CLI rescue gesture. After that, the LoRaGotchi menu actions are available.

When the display has turned off, the first click, double-click, or long-press only wakes the screen; it does not select, save, advertise, or clean. On the aquarium, short-click always advances to Stats and long-press starts cleaning if 💩 is present.

Menu footers use **1:NEXT 2:BACK L:OK** for these three gestures. The Cardputer
keyboard uses arrows/WASD to navigate, Enter/Space to select, and Escape/Backspace to go back.

The main screens advance in this order:

1. Aquarium
2. Stats
3. Clock
4. Phone pairing
5. DX Visit
6. Options
7. RF Monitor
8. GNSS

On **DX Visit**, short-click continues to **Options**; long-press opens its contact
picker. Double-click in that picker returns to DX Visit without sending.

Short-click on **Options** moves the selection. Long-press opens the selected item.
In a multi-field menu, click to choose a field and hold to edit it. In its value
picker, click through candidates and hold to save; double-click cancels the pending
value. Returning to a parent menu keeps the relevant item highlighted. Settings
already saved with a hold are not undone by leaving their parent menu.

## Options and Display

The main **Options** menu is ordered as follows:

1. **Send Advert**: choose a local or flood advertisement, or go Back without sending.
2. **Guess Game**: play five rounds of higher/lower to earn happiness when mesh activity is quiet.
3. **Reset Pet**: opens a confirmation screen with **Cancel** selected. Opening it never resets the pet.
4. **Display**: timeout, brightness, firmware theme, aquarium rendering speed, and fish colors on color displays.
5. **Sound**: volume and sound selections; omitted on builds without a buzzer.
6. **Schedule and Time**: sleep hour, wake hour, and UTC offset.
7. **RF Monitor**: recent received signal information and activity.
8. **GNSS Monitor**: GNSS status; long-press toggles GNSS where supported.

**Breed with visitor** appears between Guess Game and Reset Pet when the local and
visiting fish are eligible and there is no pending offspring egg. Optional rows do
not change where Back returns from the other menus.

### Editing settings

**Schedule and Time**, **Screen timeout**, **Sound**, and **Fish color** use field
lists: click moves between fields without changing anything, hold opens the selected
field's value picker, and double-click returns to the parent menu.

**Brightness**, **Firmware theme**, and **Game rendering** open their value picker
directly. For every picker, click chooses the next candidate, hold saves it and
returns, and double-click returns without saving. Sound selections play a preview
when saved, not while browsing. Display changes apply when saved.

### Resetting your pet safely

Open **Options → Reset pet** with a long-press. The screen asks **"Start a new egg?"**
and warns **"Pet progress & eggs will be lost forever."**

- **Cancel** is selected every time the screen opens. Long-press to cancel.
- Short-click to select **Yes, reset pet**, then release and long-press to confirm.
- Double-click cancels from either choice and returns to Options.
- If the display turns off or another screen/alert interrupts the prompt, the
  confirmation is cancelled. Waking the display does not reset the pet.

Confirming starts a new egg at generation 1, clears pet progress and any pending
offspring egg, and cannot be undone. Color displays then open the starting-trait
picker; monochrome displays start with monochrome traits. Display, schedule, and
MeshCore settings, contacts, and channels are kept. Cancelling leaves your pet
untouched; normal gameplay continues while the prompt is open.

### Sending an advert

Open **Options → Send advert** with a long-press. Opening the submenu does not
send anything. Short-click selects an item; long-press activates it:

- **Local (zero-hop)**: advertise to nearby nodes without repeater forwarding.
- **Flood (multi-hop)**: advertise through the mesh using the configured default
  scope and path-hash mode, just like a flood advert from the MeshCore app.
- **Back**: return to Options without sending. Double-click also goes back.

Once MeshCore accepts an advert for sending, the submenu closes automatically,
**Send advert** stays selected in Options, and **Advert queued!** is shown briefly.
This confirms queuing, not reception by another node. A sleeping display's first
long-press only wakes it; it never sends an advert.

Local and flood adverts share the existing one-minute UI cooldown. If it is too
soon, the remaining wait is shown and the submenu stays open; Back still works.
A failed attempt stays in the submenu for retry and does not start the cooldown.
Both advert types keep your existing name and location-advertising policy.

### Guess game

Open **Options → Guess game**. Short-click switches between **Higher** and **Lower**; long-press guesses whether the next number will be higher or lower than the current number. Play five rounds. Each correct guess adds one happiness point; wrong guesses do not reduce happiness or count as mesh activity. At the end, long-press starts another game and double-click returns to Options.

### Display submenu

Open **Options → Display**. Short-click selects an item and long-press opens it. Double-click returns to Options.

- **Screen timeout** has independent **Fish Awake** and **Fish Asleep** settings. Each can be set to 15 seconds, 30 seconds, 1 minute, 2 minutes, 3 minutes, or Always On. The schedule determines which timeout is active.
- **Dim after** (always-on dimming) selects when an always-on screen dims after inactivity: 15, 30, or 60 seconds. It applies when the active awake/asleep timeout is Always On. A button press restores the selected brightness.
- **Brightness** cycles through 25%, 50%, 75%, and 100%. The default is 100%. Brightness is controlled by OLED contrast on the V4 and backlight level on color TFT targets.
- **Firmware theme** cycles the available themes. Color TFTs have Midnight, Paper, Lagoon, and Sunset. Monochrome OLEDs use the dark and paper themes.
- **Game rendering** selects the aquarium redraw interval. **Smooth** uses the original 100 ms interval and is the default; **Simulation** redraws every 250 ms. This affects the aquarium only; live monitors keep their existing refresh rates. Click chooses a candidate, hold saves it and returns to Display, and double-click cancels. The saved selection survives restarts.

Dimming does not stop or slow animation: the selected rendering cadence stays the
same at normal and dim brightness. A fully off display stops drawing, but pet
simulation continues and waking draws the current state. Static menus skip redraws
when their displayed state is unchanged; button input, alerts, wake and changed
menu state still refresh the display.

The aquarium battery icon and RF Monitor percentage share a five-second display
cache of MeshCore's existing board-voltage reading. Calibration and the existing
3.3–4.2 V percentage scale are unchanged. Low-battery protection still takes fresh
readings independently. These changes reduce application work; battery-life gains
have not been measured on hardware.

Color displays offer **Fish color** under Display. Click selects Scales, Body, or
Eyes; hold opens that trait's picker. Click chooses a color, hold saves it, and
double-click cancels. Available colors are Coral, Tangerine, Gold, Mint, Lagoon,
Orchid, and Mono. During first-run/reset setup there is also **Start fish**, which
must be explicitly selected with a hold; Back never completes setup. Monochrome
displays skip the picker and begin with all three traits set to Mono.

### Sound menu

On buzzer-equipped LoRaGotchi targets, open **Options → Sound**. Click selects a
field, hold opens its value picker, and double-click returns to Options. In the
picker, click browses, hold saves/previews the selection, and double-click cancels.
The settings are:

- **Volume**: Off, 25%, 50%, 75%, or 100%. Volume changes the buzzer's PWM duty; triple-click remains a separate global quiet-mode toggle.
- **Wake alarm**: Off, Chirp, Rising, or Alarm. The selected original short motif plays when the scheduled sleep state changes to awake.
- **Sleep cue**: Off, Drift, or Soft bell. The selected motif plays when the fish enters scheduled sleep.
- **Message tone**: Off, Ping, Double, or Arpeggio for incoming messages.

The default is 100% volume, wake alarm Off, sleep cue Off, and Ping for incoming messages. The short motifs are original compositions rather than copied Nokia, Tamagotchi, or other device ringtones. Sound options are hidden when the build does not define buzzer hardware; the V3, V4 OLED, and T096 LoRaFish targets used here all define it.

## Pet Behavior

### Growth and age

A new pet starts as an egg. It hatches after five minutes. The fish then progresses according to age:

- **Fry**: from hatching until one hour old.
- **Young**: one hour to six hours.
- **Adult**: six hours to three days.
- **Elder**: three days until the end of its lifespan.
- **Memorial**: shown at seven days, followed by automatic succession to the next generation.

The age clock continues while the display is off and while the fish sleeps. The aquarium animation is only a presentation; turning the display off does not pause the pet.

### Breeding and generations

When an Adult or Elder local fish is hosting an Adult or Elder visitor, **Breed with visitor** appears in Options. Long-press it to produce one egg. For each of the three traits, the offspring randomly inherits that trait from either parent; Mono is inherited like any other trait. The visitor may remain until recalled, but a second egg cannot be produced while one is pending.

If the fish reaches the end of its seven-day lifespan without an offspring egg,
it automatically self-fertilises one. This solo egg inherits all three of the
parent's traits, including Mono. No visitor or button press is needed. An existing
visitor-bred egg is kept unchanged; the fallback never replaces it. Fish already
saved on the Memorial screen without an egg recover automatically after updating.
Visitor breeding is only available while the local parent is alive.

The egg is deliberately unmarked so its colors are not revealed. A visitor-bred
egg appears as a small 🥚 indicator in the aquarium footer while the current fish
lives. Generation is shown on the Stats screen. At the seven-day Memorial stage,
the offspring egg sits beside the gravestone on a random side. After five minutes
of Memorial age, succession starts the next generation at age zero, followed by
the normal five-minute egg stage before the fry appears. These two waits are the
same for solo and visitor-bred eggs. An older save already past the Memorial wait
advances on the next age tick. Only the current generation and pending egg traits
are saved; no family tree is retained. **Reset pet** clears a pending egg and
restarts generation 1.

### Mesh activity, meals, and hygiene

MeshCore interactions such as received messages, acknowledgments, and contact
discoveries contribute to the RF-event counter, but no longer add happiness
directly. Each group of ten events offers a meal while the living fish is awake.
A credited meal adds **3 Happy**, schedules waste to appear 30 to 90 seconds later,
and starts a **one-hour cooldown**. Only credited meals play eating sounds and
animations. Events still count during the cooldown, but meal thresholds reached
then are consumed: they do not queue meals for later. After the cooldown, a new
ten-event threshold is needed. The cooldown counts powered-on time, including
scheduled sleep; powering off does not advance it.

Happy loses **1 point per 30 awake minutes**, plus **1 extra point for each waste
piece** present at that interval. Waste caps at three, so a clean tank loses one
point per half-hour and a full dirty tank loses four. Scheduled sleep, appearance
setup, the egg stage and Memorial do not accumulate decay. The partial awake
interval resumes after sleep; switching off the display does not pause care.

To clean a nonempty tank, long-press on the aquarium screen. Completed cleaning
clears all waste and adds **3 Happy**. Each correct Guess Game answer still adds
one point, up to five per game; wrong answers do not subtract points. Happy stays
between **0 and 100** and does not change lifespan, evolution or breeding rules.
New/reset fish and each new generation start at 80.

Existing pet saves upgrade without a reset. Decay progress and meal cooldown are
saved at care events and approximately five-minute checkpoints; an abrupt restart
can replay unsaved time, as with pet age. Saved timers resume after reboot, with
no powered-off decay. Resetting the pet or starting the next generation clears
both timers. Older firmware can still read the original pet/settings fields, but
does not maintain the new timing data if it rewrites settings.

### Sleep schedule

The default schedule is sleep at 21:00 and wake at 07:00. Sleep and wake are based on the device clock plus the manually configured UTC offset. Offset increments are 15 minutes, from UTC-12:00 through UTC+14:00. There is no automatic timezone or daylight-saving detection; update the offset when local rules change. Setting sleep and wake to the same hour keeps the fish awake all day.

During scheduled sleep the fish displays as sleeping. Happy decay pauses, and RF
events are still counted without adding Happy. Meal thresholds reached while
asleep are consumed without feeding, so meals do not accumulate and trigger all
at once on waking. Eggs and Memorial likewise do not earn meals. A sleeping fish
cannot start a DX visit, though a fish already away can still be recalled.

## DX Visits

The contact picker shows **only saved companion/chat contacts** with 🐟 (fish),
🐠 (tropical fish), 🐡 (blowfish), or 🦈 (shark) anywhere in their raw name.
Text/emoji presentation variants of those characters also count. Repeaters and
room servers are excluded even if their names contain a fish. Ordinary companions
without a marker remain in MeshCore's contacts but are hidden from this picker.
Other sea creatures and fish-related objects such as fishing poles or fish cakes
are not markers. A marker is an opt-in naming convention, not a firmware probe.

The displayed position/count refer to matching tanks only. With none available,
the picker says **NO FISH TANKS** and Back still works. After changing a name, send
an advert and let the peer's contact entry update before checking its picker.
Recalling an existing visit does not require the host to retain its marker: the
active target is retained by public key, so renaming/reordering contacts cannot
redirect a recall or commit retry. A deleted/unavailable target produces a notice
instead of sending to an unrelated contact.

On the **DX Visit** overview, long-press to open the contact picker. Opening it sends nothing. In the picker, short-click cycles through compatible LoRaGotchi DX peers and a separate long-press sends a visit request when the local fish is awake. The peers exchange request, acknowledgment, commit, and arrival-confirmation messages. Once accepted, the visitor appears in the other aquarium while the host fish continues its own behavior.

When your fish is away, open the DX picker and long-press again to recall it home. Double-click in the picker returns to the overview without sending or recalling. From the overview, short-click continues to Options and double-click returns to the aquarium, like Back from the other main screens. A sleeping fish cannot begin a new visit. If a visit is interrupted or the device reboots, DX travel state is cleared and the local fish is restored at home.

Color-capable displays render visitor Scales, Body/Border, and Eyes traits independently. Mono traits use the display's foreground color; monochrome displays render every trait in their foreground color. A fish returning from a monochrome host briefly animates back into its saved home colors on a color display.

## Display and Hardware Notes

The UI screens are Aquarium, Stats, Clock, Phone pairing, DX Visit, Options, RF Monitor, and GNSS. GNSS details depend on the board and whether a receiver is fitted.

Fish color controls and colored visitor rendering require a color display; monochrome OLED targets remain monochrome. Monochrome and Colour tanks know about each other and act & display accordingly

Use the companion BLE connection for MeshCore functions such as messages, contacts, and setting device time. LoRaGotchi DX adds the pet UI and DX visit behavior on top of that companion firmware.
