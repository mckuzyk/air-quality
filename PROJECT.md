# Air Quality & Bioaerosol Monitoring — Project Context

**Status:** Phase 1, pre-build (parts being ordered)
**Last updated:** 2026-07-16

> This document is both a personal reference and a context primer for AI agents
> assisting with this project. Agents: read the *Operator Profile* and *Decision
> Log* before proposing anything — several obvious-looking suggestions have
> already been considered and rejected for stated reasons.

---

## 1. Project Goals

A staged, escalating project in airborne particulate sensing.

1. **Near term:** PM2.5 sensing indoors, logged over WiFi to a self-hosted
   time-series stack with a trend dashboard.
2. **Medium term:** A remote node at a friend's off-grid mountain property
   (solar powered, weak cellular), reporting to the same stack.
3. **Long term / aspirational:** A DIY automated pollen counter — optical
   imaging of collected bioaerosol + ML classification by taxon, targeting
   approximate hourly pollen counts. Possibly extending to mold spores.
4. **Speculative side-quest:** Allergen protein sensing (dust mite Der p1) via
   fiber-optic fluorescence immunoassay. Separate discipline; parked.

The through-line: each phase reuses the previous phase's infrastructure
(power, comms, ingest, dashboard) and adds exactly one hard new thing.

---

## 2. Operator Profile

Important for calibrating advice. **Do not explain things in these columns.**

| Domain | Level |
|---|---|
| Experimental quantum optics | PhD. Laser alignment, optical components, Fourier optics are native skills. |
| Physics / math generally | PhD. |
| Data science / ML | Professional, current. |
| Python | Expert. |
| SQL / databases | Expert on SQL; comfortable with database design and operation generally. |
| Programming generally | Strong. Comfortable picking up new languages. |
| C | Rusty but confident. |
| **C++** | **None yet — and learning it is part of the appeal.** See note below. |
| **Circuit electronics** | **Weakest area, and an explicit thing to improve.** See note below. |
| Aerosol fluid dynamics | None. The key gap for the pollen phase. |

**On circuits:** this is the weakest area *relative to the rest*, not an absolute
one — the underlying physics is a PhD-level strength, so the gap is practical
fluency (what components exist, what the conventions are, what fails in
practice), not theory. **This is a thing to get better at, not a thing to design
around.** Explain the reasoning behind a circuit choice rather than just handing
over a wiring table; suggest the instructive path when it's not much costlier
than the shortcut. Reserve caution for genuinely destructive mistakes (reversed
polarity on an expensive part), not for ordinary difficulty.

**On C++ and Python:** the fact that Python is the day-job language is **not** a
reason to route the firmware through MicroPython or ESPHome. **Writing real C++
is part of the point.** Suggest Python where it's genuinely the better tool
(ingest, analysis, ML, tooling) — but don't offer it as an escape hatch from the
embedded work.

**Environment preference:** macOS, **tmux + neovim**. Not a VS Code user. All
tooling recommendations must work headless/CLI-first.

**Available resources:**
- Father runs a university lab (professor) — bench space, likely surplus
  optomechanics, possibly a real microscope w/ motorized stages. This is
  load-bearing for the pollen phase (paired ground-truth data, species ID).
- Friend's mountain property near Spokane, WA: solar power, weak but present
  cell signal.

---

## 3. Phase 1 — Indoor PM2.5 Node (CURRENT)

### Hardware

**Already owned:**
- Plantower PM sensor (bought years ago; **model unconfirmed** — check the
  label on the metal shield. Assume PMS5003 family. PMS7003 uses a different
  connector pitch; PMS3003 lacks some particle-count bins.)
- Arduino Uno, Adafruit Metro, an old Raspberry Pi — *all rejected, see
  Decision Log.*

**Ordering / to acquire:**

| Item | Notes | ~Cost |
|---|---|---|
| ESP32 dev board | ELEGOO ESP-WROOM-32, 3-pack, USB-C, CP2102, 30-pin. **Vetted, correct choice.** | ~$20/3 |
| **PMS breakout cable** | ⚠️ **Long pole.** 1.25mm JST → pigtail or 0.1" header. Cannot breadboard the connector directly, cannot hand-crimp. Adafruit sells both cable and adapter board. *Check the original sensor's bag first.* | ~$5 |
| Breadboard | 830-point (full size). Half-size is cramped — 30-pin ESP32 straddles the center channel leaving one row per side. | ~$5 |
| Jumper wires | M-M and M-F assortment. | ~$5 |
| USB-C cable | **Must be a data cable.** Charge-only cables power the board while the serial port never enumerates. Classic hour-loss. | ~$5 |
| 5V/2A USB supply | Underpowered supply → brownouts → "random reboots." Most common ESP32 failure mode. | ~$8 |
| BME280 (recommended) | I2C temp/humidity/pressure. **Not optional in practice** — PM2.5 readings are humidity-sensitive (hygroscopic growth inflates readings at high RH). Need RH logged alongside to interpret own data. | ~$5 |
| MicroSD module (recommended) | Local logging as ground truth. $3 insurance against every future comms problem. | ~$3 |

### Wiring

PMS exposes: `VCC, GND, TXD, RXD, SET, RESET`. Minimum viable = 4 wires.

| PMS | ESP32 | Note |
|---|---|---|
| VCC | **5V / VIN** | Fan needs 5V. 3V3 pin will not work. |
| GND | GND | |
| TXD | GPIO16 (RX2) | **Crossed, not straight.** |
| RXD | GPIO17 (TX2) | Only needed for sleep/mode commands. |
| SET | float (or 3V3) | Pull LOW to sleep the fan. Used later for duty cycling. |
| RESET | float | |

**Key fact:** PMS fan is 5V but its **UART logic is 3.3V** = ESP32 native. **No
level shifter, no divider, no analog work** — the two devices happen to agree on
logic levels, so the 5V/3.3V split only matters for the fan's power rail. Worth
understanding *why* this is the lucky case, since the next sensor may not be:
mismatched logic levels are the usual reason a level shifter shows up.

⚠️ Verify pinout against the datasheet for the *confirmed* model. Random blog
pinout diagrams are frequently wrong. Swapping VCC/GND is the one genuinely
destructive available mistake.

### Power topology

5V supply plugs into the ESP32's **USB-C port** — that's it. USB 5V is tied to
the VIN pin (usually via a Schottky diode), so VIN becomes a 5V *output* to tap
for the PMS. The onboard AMS1117 separately makes 3.3V for the ESP32 itself.

```
5V charger → USB-C → board
                      ├─ AMS1117 → 3.3V → ESP32 + 3V3 pin
                      └─ 5V rail → VIN pin → PMS VCC (fan)
```

Breadboard: `VIN → red rail`, `GND → blue rail`, PMS feeds from rails.

Do **not** power via USB and inject into VIN simultaneously.

Current budget: PMS fan ~100mA, ESP32 WiFi TX spikes 250–500mA. ~600mA worst
case. 2A supply is ample.

⚠️ **USB-C gotcha:** cheap dev boards sometimes omit the 5.1kΩ CC pulldowns a
USB-C source needs to see before enabling 5V. Symptom: C-to-C cable from a PD
charger does *nothing*, board looks dead. Workaround: use an **A-to-C cable**
with a legacy USB-A brick, which supplies 5V unconditionally.

### Firmware stack

- **Framework: Arduino-on-ESP32** (not ESPHome, not ESP-IDF — see Decision Log)
- **Build system: PlatformIO CLI** (`pio`), *not* the VS Code extension
- **Editor: neovim + clangd**

```bash
brew install pipx
pipx install platformio          # pipx, not brew — isolates its Python deps

mkdir pm25-node && cd pm25-node
pio project init --board esp32dev --ide vim
```

`platformio.ini`:
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps = fu-hsi/PMS Library@^1.1.0
```

Workflow:
```bash
pio run                          # build
pio run -t upload -t monitor     # flash + serial console (chained)
pio device list                  # find port; expect /dev/cu.usbserial-XXXX
pio run -t compiledb             # → compile_commands.json for clangd
```

Re-run `compiledb` whenever `lib_deps` changes. Add `.clangd`:
```yaml
CompileFlags:
  Remove: [-m*, -f*]     # strip xtensa flags clangd chokes on
```

macOS ships a CP210x driver; if `pio device list` is empty, try the Silicon
Labs VCP driver — *but check the cable first.*

### C++ notes (first C++ project — learning it is a goal, not an obstacle)

Arduino code is **"C with objects."** The minimum needed on day one:
- Method syntax: `pms.requestRead()` not `pms_request_read(&pms)`
- Constructors: `PMS pms(Serial2);`
- References: `void f(PMS::DATA &data)` — a pointer with nicer syntax
- Scope resolution: `PMS::DATA` = "the DATA type inside PMS"

**Not needed to get PM readings on day one:** templates, inheritance, virtuals,
move semantics, smart pointers, RAII. Don't reach for them just to have reached
for them — but this is a **sequencing** note, not a ban. Once the sensor works,
writing a proper sensor-abstraction class, using RAII for the SD file handle, or
templating the ring buffer are all reasonable and instructive things to do, and
this project is a fine place to learn them. Introduce them when there's a real
reason, and say what the reason is.

Environment facts: exceptions and RTTI are **off by default** on ESP32-Arduino
(worth knowing before designing around them). `Serial.printf()` works.

⚠️ **Avoid the Arduino `String` class.** It heap-fragments on long-running
devices — the classic "worked for 3 days then crashed" bug. Use `char[]` +
`snprintf()`. (Aligns with existing C instincts anyway.)

### Build order — strictly one layer at a time

1. **Sensor → serial.** Print PM values. Light a match to see a spike. If this
   doesn't work nothing else will.
2. **+ WiFi.** `WiFi.begin()`, print IP.
3. **+ MQTT.** PubSubClient or AsyncMqttClient → JSON to a topic.
4. **+ OTA.** `ArduinoOTA`, ~5 lines. **Do this before the board is physically
   annoying to reach.**
5. **+ BME280, SD logging, duty cycling.**

Do not do 1–3 at once. Debugging a compound failure across wiring, baud rate,
credentials, and broker config is miserable.

Reference sketch:
```cpp
#include <PMS.h>
PMS pms(Serial2);
PMS::DATA data;

void setup() {
  Serial.begin(115200);                      // USB console
  Serial2.begin(9600, SERIAL_8N1, 16, 17);   // PMS, 9600 fixed
  pms.passiveMode();
}

void loop() {
  pms.requestRead();
  if (pms.readUntil(data)) {
    Serial.printf("PM1.0: %u  PM2.5: %u  PM10: %u ug/m3\n",
                  data.PM_AE_UG_1_0, data.PM_AE_UG_2_5, data.PM_AE_UG_10_0);
  }
  delay(5000);
}
```

### Ingest / dashboard

Two options, both fine:
- **Home Assistant** — turnkey, MQTT broker + storage + dashboards, ~no code.
- **Mosquitto → InfluxDB → Grafana** — more work, more "yours," plays to the
  data engineering strengths. **Likely preference.**

Design constraint: **the dashboard must not care where data came from.** Every
transport (WiFi, cellular, LoRa) should terminate in the same MQTT topic
structure. This is what makes the phases composable.

### Toolchain bring-up gotchas (macOS) — encountered during Phase 1 setup

⚠️ None of this is specific to sensor code — it's infrastructure-layer stuff
that will resurface on a new machine, a new project, or for anyone else picking
this up. Each fix is quick once known; diagnosing them from scratch took a full
session.

**pipx-installed tools silently missing pip**
- Symptom: `pio project init` fails with `MissingPackageManifestError: Could
  not find one of 'package.json' manifest files in the package`. The real
  error, only visible with a fresh package cache, is
  `<venv>/bin/python: No module named pip`.
- Cause: pipx creates each tool's venv with `--without-pip`, then seeds pip
  from a one-time shared install at `~/.local/pipx/shared`. If that shared
  bootstrap never completed, every pipx-installed tool's venv silently ends up
  pip-less — not just PlatformIO.
- Diagnose: compare a throwaway plain venv against the suspect one:
  ```bash
  python3 -m venv /tmp/testvenv && /tmp/testvenv/bin/python -m pip --version   # works
  ~/.local/pipx/venvs/platformio/bin/python -m pip --version                  # fails
  ```
- Fix:
  ```bash
  ~/.local/pipx/venvs/platformio/bin/python -m ensurepip --upgrade --default-pip
  rm -rf ~/.platformio   # clear the half-installed package cache from the failed attempt
  ```

**Apple's system `clangd` breaks cross-compiled C++ header resolution**
- `/usr/bin/clangd` (Xcode-bundled) injects its own Darwin system include
  paths ahead of the Xtensa toolchain's, regardless of `.clangd` flags. Not
  fixable via config — the binary itself is the problem.
- Fix: `brew install llvm`, then put `/opt/homebrew/opt/llvm/bin` ahead of
  `/usr/bin` on `PATH`. Confirm with `clangd --version` — should print a plain
  version number, not "Apple clangd".

**clangd + Xtensa toolchain: full working `.clangd`** (supersedes the
simplified version earlier in this doc)
```yaml
CompileFlags:
  Remove:
    - -m*
    - -fno-tree-switch-conversion
    - -fstrict-volatile-bitfields
    - -Wno-frame-address
  Add:
    - -nostdlibinc
    - -nostdinc++
    - -isystem
    - ~/.platformio/packages/toolchain-xtensa-esp32/xtensa-esp32-elf/include/c++/8.4.0
    - -isystem
    - ~/.platformio/packages/toolchain-xtensa-esp32/xtensa-esp32-elf/include/c++/8.4.0/xtensa-esp32-elf
    - -isystem
    - ~/.platformio/packages/toolchain-xtensa-esp32/xtensa-esp32-elf/include/c++/8.4.0/backward
```
- The `-isystem` lines are load-bearing, not cosmetic: clangd's automatic
  `--query-driver` extraction returned only the *C* header paths for this
  toolchain (`include-fixed`, `sys-include`, `include`) and silently dropped
  the C++-specific ones, causing `pp_file_not_found` on `<algorithm>` and
  similar STL headers even though query-driver was otherwise working. Confirm
  by comparing clangd's extracted include list against the compiler's own
  ground truth:
  ```bash
  xtensa-esp32-elf-g++ -E -v -xc++ - < /dev/null 2>&1 | sed -n '/search starts here/,/End of search list/p'
  ```
- `--query-driver` itself (allow-listing the real cross-compiler so clangd can
  query it at all) is a **launch-time flag, not a `.clangd` YAML key** — a
  security boundary that deliberately can't be granted from a checked-in
  project file:
  ```
  --query-driver=~/.platformio/packages/**/bin/*-g++,~/.platformio/packages/**/bin/*-gcc
  ```
  Goes wherever the `clangd` process itself is launched (editor LSP client
  `cmd`, or a manual test invocation) — never in `.clangd`.
- Headless sanity check, independent of any editor:
  ```bash
  clangd --background-index --query-driver="..." --check=src/main.cpp
  ```

**Wrong USB-serial port picked by autodetect**
- If another USB-serial device is plugged in (a wireless keyboard/mouse dongle
  is a common culprit — many use generic serial chips that enumerate
  identically to a real board), PlatformIO's port autodetect can silently
  attach to the wrong `/dev/cu.usbserial-*`. Symptom: upload/monitor report no
  error, but nothing ever actually reaches the ESP32 — no boot text even on a
  manual EN reset, board LED lit, everything else checks out.
- Diagnose: unplug the ESP32 specifically and see which `/dev/cu.usbserial-*`
  disappears — that's the real one.
- Fix: pin the port explicitly rather than trusting autodetect:
  ```ini
  upload_port = /dev/cu.usbserial-0001
  monitor_port = /dev/cu.usbserial-0001
  ```

**Apple's built-in CP210x driver only actually supports genuine CP2102**
- macOS's built-in `AppleUSBSLCOM` driver auto-binds to CP2102, CP2102N,
  CP2104, CP2108, and CP2109 — but is documented to only work correctly with
  the original CP2102. For the newer variants it silently enumerates the
  device (looks completely normal, gets a device path) while passing zero
  actual data, in either direction, regardless of terminal tool used.
- Confirm which chip is actually present with `ioreg -p IOUSB -w0` (note:
  `system_profiler SPUSBDataType` gave no output at all on this Mac — an
  unrelated quirk; `ioreg` is the reliable fallback). Our board's chip
  identified as genuine CP2102, so this wasn't the cause here, but worth
  checking first on a future board: if `ioreg` shows CP2102N/2104/2108/2109
  and the port enumerates but never transmits, install Silicon Labs' own
  driver rather than relying on Apple's bundled one:
  https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers
  (expect a system-extension approval prompt, and possibly a second,
  differently-named `/dev/cu.*` device afterward).

**Garbled/repeating serial output ≠ broken board — check baud rate first**
- PlatformIO's monitor defaults to 9600 baud unless `monitor_speed` is set in
  `platformio.ini`, independent of `upload_speed`. A mismatch against the
  sketch's `Serial.begin(115200)` produces repeating garbage characters, not
  silence or an error — easy to mistake for a deeper problem.
  ```ini
  monitor_speed = 115200
  ```

---

## 4. Phase 2 — Remote Mountain Node

### Site facts
- Off-grid, solar powered, near Spokane WA.
- **Weak cell signal exists — and the one spot with reception is right next to
  the solar panels.** Power and signal co-located. Unusually lucky.

### Comms decision: narrowband cellular (primary), LoRa (fallback / fun)

**Payload reality check:** `{"pm25":12,"pm10":18,"rh":41,"t":19}` ≈ **40 bytes**.
Hourly ⇒ **~1 KB/day**. This is a *tiny* data problem and should drive every
decision.

**Starlink: rejected.** 50–75W continuous ⇒ ~1.5–2 kWh/day of solar to move one
kilobyte. Node's own duty-cycled budget is ~50–100 mW. Three orders of
magnitude mismatched. *(Exception: if the friend already has Starlink for his
own use, just join the WiFi and skip this entire section.)*

**Chosen: Blues Notecard, Narrowband SKU.**
- **Get the "LTE-M, NB-IoT for North America" variant.** *Not* the "midband"
  LTE Cat 1 bis — that's a step up in **throughput**, which is worthless here.
  Want the opposite: sensitivity.
- LTE-M/NB-IoT buy **~20 dB extra link budget** vs standard LTE (~100× in power)
  by trading bandwidth for sensitivity. Designed to reach basement water meters.
- ~$49, **500MB + 10 years of service bundled, no monthly fee, no SIM to
  manage.** Cannot exhaust 500MB at 1 KB/day.
- Embedded SIM roams across carriers rather than locking to one — meaningfully
  better odds at a marginal site than a single-carrier SIM.
- Talks JSON over serial/I2C from the ESP32.

⚠️ **Caveat: SMS working ≠ data working.** SMS rides the control channel and is
far more robust than a data session. Encouraging evidence, not proof.
Also: LTE-M coverage ≠ LTE coverage — carrier must have deployed it on that
tower. Unknown until tested.

⚠️ **Don't forget the Notecarrier.** The Notecard is a bare M.2 module. Needs a
Notecarrier breakout for headers, LiPo JST, and — critically — the **antenna
connector**. Same category of gotcha as the PMS cable.

**The antenna dominates everything at marginal signal.** Proper external LTE
antenna, mounted high, above the panel array, clear of metal. $30 and an
afternoon of placement beats any firmware cleverness. This is a link budget
problem = home turf.

### Store-and-forward is the key feature

At weak signal the modem TXes at max power and takes longer to attach ⇒ each
connection costs real energy, and some fail. The Notecard queues JSON "notes"
locally and syncs on a schedule; a failed sync is a retry, not lost data.

**Pattern: sample every 10 min, sync every 6 h.** Batches 36 readings into one
attach instead of paying the attach cost 36×. Full-resolution time series,
dramatically easier energy + reliability. Config change to hourly during fire
season if needed.

### Power

**Ask the friend: what voltage is the battery bank, and is there a spare fused
circuit?** Most cabin systems are 12V. If so, the entire power solution is a
12V→5V buck converter off the existing bank — inheriting an already-maintained
energy system. Far less work than sizing dedicated panel + battery.

### Battery / duty cycling notes (if a standalone pack is ever needed)

The **fan dominates.** ESP32 deep sleep is µA; the PMS fan is ~100mA @ 5V
whenever running. Continuous ⇒ ~250mA ⇒ a 10,000mAh bank (≈6,000–6,500mAh
usable at 5V after boost losses) lasts **~24–30 h.** Useless.

Duty cycled via the SET pin:
1. Wake, SET high, fan spins up
2. **Wait ~30 s — mandatory.** Chamber must purge and stabilize; earlier
   readings are garbage.
3. Read, publish
4. SET low, ESP32 deep sleep 5–10 min

~40 s on per 600 s = ~7% duty ⇒ ~15–25mA average ⇒ **weeks.**

⚠️ **USB power banks auto-shutoff below ~50–100mA** (they think nothing's
plugged in). A duty-cycled node looks exactly like an unplugged phone. Either
buy an "always-on"/"low current mode" bank (Voltaic et al. target this), or
skip power banks: 18650/LiPo + **TP4056 w/ DW01 protection** + boost to 5V
(the fan needs 5V regardless of the cell's 3.7V nominal).

**Measure actual duty-cycled draw with a USB meter before sizing anything.**
Battery sizing is trivial once measured and impossible to guess before.

### Order of operations

1. Build the whole thing at home on WiFi. Sensor → MQTT → dashboard. Done.
2. Swap WiFi for the Notecard **on the desk in Spokane**, strong signal. Get
   Notehub routing into the existing MQTT broker.
3. **Then** take it up the mountain — where the only new variable is signal
   strength, not code/wiring/cloud plumbing.
4. **SD card on it regardless.** If cellular disappoints, data still exists and
   the failure converts into the store-and-forward option.

### LoRa (parked, but keep warm)

If step 3 fails, this becomes the answer — and the sensor, code, and dashboard
would already work, needing only a radio swap.

- 915 MHz ISM, license-free, chirp spread spectrum. Demodulates **below the
  noise floor** (documented decode at **-18.8 dB SNR**).
- Range is purely line-of-sight: few hundred m in trees/urban; **20+ miles**
  elevated w/ clear LOS; Meshtastic community record **331 km (206 mi)**
  mountain-to-mountain.
- Mountains are a *gift* here, not an obstacle. Spokane ↔ ridge is plausible.
  **It's a link budget calculation** — Fresnel clearance, path loss, antenna
  gain, RX sensitivity. Native skillset.
- Hardware: Heltec/TTGO LoRa32 (~$15, ESP32 + SX1276 on one board) ×2. One at
  the cabin, one at home republishing to MQTT.
- Two flavors: **raw point-to-point** (~30 lines, total control, zero
  infrastructure/fees) or **Meshtastic** (flash firmware instead of writing
  radio code; built-in telemetry modules + MQTT bridging; opportunistic relay
  via any other nodes in between; solar mountaintop relays are a first-class
  supported use case).
- Cost to test the hypothesis: **$30.** Worst case, learn the path doesn't close.

### Other options considered

- **Satellite (non-Starlink):** Swarm is **dead** — SpaceX acquired it 2021,
  sunset end of 2024 in favor of Direct-to-Cell. *Do not buy Swarm modems on
  eBay.* Current: **Blues Starnote for Skylo** ($49, 18 KB bundled, no monthly
  active-device fee, $0.75/KB after ⇒ ~$1–2/mo at hourly). Iridium/RockBLOCK is
  ~$999 kit + ~$31.50/mo — overkill for Washington. Skylo is NB-IoT-over-
  satellite (3GPP Rel-17 NTN); modest antenna/power, nothing like a dish.
- **Store-and-forward sneakernet:** SD + RTC, collect on visits. Unglamorous but
  gives *better* data integrity than a flaky link. Genuinely the right answer if
  trends (not real-time) are the point. Counterargument: **wildfire smoke** —
  real-time may actually matter in eastern WA.
- **Ham radio / APRS:** Technician license is a weekend. Free existing
  digipeater/igate network across the mountain west, better propagation than
  LoRa, data lands on aprs.fi. Downsides: no encryption permitted, no commercial
  use, callsign transmitted in the clear. Filed under "enjoyable rabbit hole."

---

## 5. Phase 3 — Pollen Counter (long horizon, exploratory)

### Framing

Two *separate* instruments, not one:
- **Bioaerosol imaging + classification** (pollen, mold spores). Optics + ML
  carry almost the whole thing. **Aerosol fluid dynamics is the one real gap.**
- **Allergen protein sensing** (Der p1). Optics carry the instrument; **wet-lab
  immunochemistry is the gap.** Parked. Needs commercial antibody pairs or a
  collaborator in immunology/biochem.

⚠️ **Dust mites are not an imaging target.** The mites live in fabric and aren't
meaningfully airborne. The allergen is a protein in their feces (**Der p1**),
detected chemically (ELISA / antibody biosensor), not visually. No morphology to
image. Does *not* extend from the pollen pipeline. **Mold spores do** — airborne,
2–10 µm, same capture and imaging path (though spore taxa look more alike than
pollen grains do, so classification is harder).

### Why this is tractable here specifically

Standard method is a **Hirst-type volumetric spore trap** + a human taxonomist
counting by eye under a microscope (grain size, shape, surface texture, pore/
furrow count). The bottleneck is trained experts — which is what automation
targets.

State of the art is *literally the camera + ML idea*:
- **Digital holography** (Swisens Poleno): 96% pollen-vs-other, 90%+ on six of
  eight taxa.
- **Imaging flow cytometry**: CNN on morphology + fluorescence + scatter, up to
  2000 particles/s.

The hard part for most hobbyists — the optics — **is the operator's PhD field.**
This inverts the usual difficulty ordering: real-time optical sensing is a
natural starting point rather than a stretch goal.

### Cost discipline (the expensive parts are optional for v1)

- **Pulsed laser diodes** exist to freeze fast-moving particles in flow. Not
  needed until committed to a flowing-air design. A **CW diode** is a fine
  coherent point source for lensless holography (people use $3.50 diodes).
- **Scientific CMOS:** not needed. **Raspberry Pi HQ camera (~$50–75) with the
  lens removed**, bare sensor in the beam path, is a legitimate lensless
  holography sensor. Plenty of resolution for 10–100 µm objects.
- **Deep-UV diodes** (250–300nm, tryptophan autofluorescence à la WIBS): pricey,
  short-lived. **Skip for v1.** Near-UV LEDs (365–405nm) are a few dollars and
  still excite useful pollen/spore autofluorescence — enough to test whether a
  fluorescence channel adds discrimination before betting money on deep-UV.
- **Lab surplus:** optomechanics (mounts, breadboards, translation stages) is
  the expensive-if-bought-new category and labs have it gathering dust.

### Phased build

- **Phase 0 (~$100–150):** Validate holographic reconstruction on **static**
  particles. Cheap CW diode + Pi HQ camera (lens off) + known pollen on a slide
  + Fresnel back-propagation code. Proves the physics/software pipeline before
  spending a dollar on flow hardware. **Also solves the labeled-training-data
  problem cheaply** — photographing known samples anyway.
- **Phase 1 (~few hundred $):** Slow, controlled flow. Small pump + settling
  chamber. No pulsing needed if flow is slow relative to exposure. Gets from
  "static demo" to "sampling room air."
- **Phase 2 (cost jumps):** Real-time fast flow. Virtual impactor, faster optics,
  possibly pulsed diode + near-UV fluorescence channel. **An earned upgrade**,
  only after 0–1 prove classification works.

### Architecture decision: how to get an *hourly count*

The published Pi/LED work images **one grain at a time** on a **static slide**
via motorized XYZ raster scan (~3 grains/mm² over ~500mm²). No airflow, no
real-time. The authors explicitly flag this, noting it "could be extended for
use in real-time with the use of a flow chamber or an impactor."

Two paths to a rate-over-time:
- **(A) Accumulate + periodic batch-scan.** Classic Hirst mechanics: rotating
  drum or advancing adhesive tape (~2mm/hour) where **position encodes time**.
  Hourly, raster-scan the section that advanced, classify, report counts/taxon.
  **Reuses the validated single-particle technique unmodified.** No triggering,
  no fast optics — just a slow mechanical advance + a timer.
- **(B) True continuous-flow event counting.** Virtual impactor concentrates
  particles into a fixed beam; each transit triggers a capture; hourly count =
  tally of events. What the commercial instruments do. Adds a real new problem:
  **detecting a particle is transiting *now*** and capturing a well-timed frame
  (→ pulsed illumination or careful flow/exposure matching).

**→ Chosen target: (A) with transect subsampling.** Rather than scanning the
whole accumulated area, count along fixed **transects** (sample strips) and
extrapolate — exactly what human analysts have done with Hirst traps for
decades. Cuts imaging burden to a few fields of view per hour. **Not a
corner-cut; it's the established methodology, automated.** Goal was explicitly
*approximate* hourly counts. Path B is a legitimate later upgrade.

### ML architecture

⚠️ **Correction on the Pi/LED paper's architecture:** it is a **conditional GAN
(Pix2Pix)** — U-Net generator, ~7 layers, 256×256 in/out, lr 0.0002, dropout
0.5, discriminator used only during training. **Supervised pixel-space
image-to-image translation** (scattering pattern → microscope-equivalent image).
Not representation learning, no latent-space prediction, no two-tower setup.

**The expensive part is the paired ground truth.** They needed **two physical
setups**: a real microscope (Nikon Eclipse LV150L, 20× obj, Basler cam, ~110×
total) ~85mm from the cheap LED+Pi rig, with the slide on **motorized XYZ
stages** shuttling between them, imaging **every grain twice** — cropped and
registered to 256×256. 935 paired images (4 species) train; 31 held-out + 100
from 3 unseen species test; 160 epochs, ~3 h on 2× A6000.

⇒ Replicating this needs **microscope access with stage repeatability**, not
just the cheap rig. Another reason the lab access is load-bearing.

**Results:** SSIM 0.88, PSNR >27 dB. Size, orientation, and grain count
(including agglomerations) recovered, even for unseen species. **But surface
texture was NOT well recovered** — attributed to the **LED's low spatial
coherence** limiting high-spatial-frequency content in the scattering pattern.
Real constraint: exine texture (spines, pores, ridges) is exactly what
species-level palynology uses. Points toward wanting a **more coherent source
than an LED** — conveniently, home turf.

**→ Architectural fork.** The same group's *earlier* work classified **directly
from the scattering pattern** with an ordinary CNN — no U-Net, no GAN, no paired
microscope ground truth, just species labels. **Materially simpler data
pipeline:** only need to know *which species* a grain came from (from reference
samples), not to re-image every grain on a separate microscope with sub-micron
stage repeatability.

**Recommendation: "classify-directly-from-pattern" first.**
"Reconstruct-then-classify" is the ambitious, data-hungry option.

### On JEPA (considered, deferred)

Well-motivated, not a starting point.

**Why it fits:** the data profile is exactly right — **cheap unlabeled data
(sensor runs all day), expensive labels (need microscope/lab/known samples)**.
Also, holographic reconstructions are noisy in semantically irrelevant ways
(speckle, phase artifacts, orientation/position jitter); a pixel-reconstruction
objective (MAE) burns capacity modeling that noise, while latent-space
prediction can discard it.

⚠️ **Physics-specific catch: JEPA must operate on the *reconstructed,
back-propagated real-space image*, not the raw hologram.** Patch-masking assumes
a local patch is spatially meaningful. In an inline hologram, a single particle's
information is **delocalized across the whole diffraction pattern by
construction** — masking a patch of a raw interferogram doesn't hide a localized
piece of the object. This is a **preprocessing** consideration, upstream of JEPA;
the pipeline after back-propagation is standard I-JEPA, unmodified.

*(For reference — I-JEPA architecture: context encoder (ViT) sees only visible
patches; target encoder (EMA of context encoder, no gradients) sees the **whole
unmasked image** and its patch embeddings for each target block are the ground
truth; a predictor maps context embeddings + positional mask tokens → predicted
target embeddings; loss is L2/smooth-L1 between them. Both towers see the **same
single image** — there is no raw/reconstructed split. The EMA asymmetry prevents
collapse. The pixel-space decoder in the paper is a **post-hoc visualization
tool only** — not in the training loop.)*

**Plan:**
1. **Do not train JEPA from scratch.** Original I-JEPA: ImageNet-scale, ViT-Huge,
   16× A100 × 3 days. SSL methods fail **silently** — collapsed/degenerate
   representations throw no error.
2. Start from a **pretrained SSL backbone** (released I-JEPA checkpoints, or
   DINOv2) frozen or lightly fine-tuned; linear probe / small MLP on top.
3. **Expect domain shift** — these were trained on natural photos, not holographic
   reconstructions. Fix is **continued/domain-adaptive pretraining** on an
   accumulated unlabeled corpus (thousands of images), not from-scratch training.
   Precedent exists: I-JEPA adapted to retinal fundus imaging.
4. **Always keep a boring baseline:** plain CNN / small ViT, heavy augmentation,
   ImageNet or pollen-microscopy transfer. If the fancy pipeline can't beat it,
   that's information.

### Software

**Don't write Fresnel propagation from scratch.** Use **pyDHM**
(github.com/catrujilla/pyDHM) — angular-spectrum, Fresnel, Fresnel-Bluestein
propagators + phase-shifting reconstruction. Writing it is an afternoon's work
given the background, but starting from validated code avoids debugging a
numerical propagator *while* validating the optical setup.

Public datasets to bootstrap: **POLEN23E**, **Cretan Pollen Dataset**. Caveat:
standard bright-field microscopy, **not holographic** — may need a bespoke
labeled set matched to the sensor's actual output modality.

### Other gaps
- **Laser safety / enclosure** — trivial to reason about correctly, but do it
  properly, especially if UV excitation enters the picture.
- **Calibration & placement** — flagged early as the biggest real failure mode
  for cheap PM sensors generally (airflow sensitivity, CO2 needs clean-air
  reference). It's a **data-quality problem** ⇒ plays to strengths.

---

## 6. Decision Log

| Decision | Rationale |
|---|---|
| **ESP32** over Arduino Uno / Metro | Those have **no WiFi**. Remote logging is the entire point. |
| **ESP32** over Raspberry Pi | Overkill; want a node that can lose power / reboot without ceremony. |
| **Classic ESP32 (WROOM-32)** over C3/S3 | Multiple hardware UARTs (keeps `Serial` for USB console, `Serial2` for PMS); widest library/example coverage; fewer surprises. |
| **30-pin dev board** | Must expose **5V/VIN** for the PMS fan. |
| **Arduino framework** over ESPHome / MicroPython | Operator wants to write real C++ — **learning it is an explicit goal**, so "but Python is easier" is not a valid argument here. ESPHome is YAML-only and would be fought the moment custom logic appears. |
| **Arduino framework** over ESP-IDF | ESP-IDF is the "proper" native SDK w/ FreeRTOS, but more ceremony and no benefit for a sensor node. **Not a corner:** Arduino-on-ESP32 is a wrapper *over* ESP-IDF; IDF functions are callable directly from a sketch. |
| **PlatformIO CLI** over Arduino IDE | Real dependency management, versioned board configs, works headless. Arduino IDE hides everything and is a bad editor. |
| **PlatformIO** over `arduino-cli` | Both work with nvim; pio has better dependency/board management and more actively maintained ESP32 support. |
| **BME280 included from the start** | PM2.5 is humidity-sensitive; RH is needed to interpret own data. The correction cheap-sensor comparisons live and die on. |
| **Narrowband (LTE-M/NB-IoT)** over midband Cat 1 bis | Need **sensitivity**, not throughput. ~20 dB link budget advantage at a marginal site. |
| **Cellular first**, LoRa parked | Signal exists and is co-located with power. LoRa retained as a fun side project + fallback. |
| **Pollen: classify-from-pattern** over reconstruct-then-classify | Avoids needing paired microscope ground truth for every grain. Only needs species labels. |
| **Pollen: accumulate + batch-scan (A)** over continuous-flow (B) | Reuses the validated static-slide technique; avoids solving particle-transit triggering. Goal is only *approximate* hourly counts. |
| **Pollen: static Phase 0 first** | Validates physics + software before spending on flow hardware; simultaneously generates the labeled dataset. |
| **Homebrew `clangd`** over Apple's Xcode-bundled `/usr/bin/clangd` | Apple's build injects Darwin system includes that break Xtensa cross-compilation regardless of `.clangd` config; not fixable via flags, only by using a different binary. |
| **Explicit `upload_port`/`monitor_port`** over PlatformIO autodetect | Autodetect can silently attach to an unrelated USB-serial device (e.g. a wireless keyboard/mouse dongle) when more than one is plugged in, with no error — just silent failure to reach the board. |

---

## 7. Open Questions / Next Actions

- [ ] **Confirm the Plantower model number** (label on the metal shield).
      Determines connector and available data fields.
- [ ] **Check the sensor's original bag for a breakout cable** before ordering one.
- [ ] Confirm the ELEGOO boards ship with **headers pre-soldered** (product
      photos: look for black pin strips on both edges).
- [ ] **Ask the friend:** battery bank voltage? Spare fused circuit / accessory
      outlet? Any existing internet on the property (would collapse Phase 2 to
      "join the WiFi")?
- [ ] **Have the friend check cell signal standing exactly where the sensor
      would go** — not just "do I have bars at the cabin."
- [ ] Decide: Home Assistant vs. Mosquitto/InfluxDB/Grafana.
- [ ] **Measure actual duty-cycled current with a USB meter** before sizing any
      battery.
- [ ] Inventory the lab: microscope w/ motorized stages? Surplus optomechanics?
      Anyone in biology who can identify collected pollen samples?

---

## 8. References

### Lensless / on-chip holography (foundations)
- Tseng et al., "Lensfree microscopy on a cellphone," *Lab Chip* 10, 1787–1792
  (2010). https://doi.org/10.1039/C003477K
- Isikman et al., "Giga-Pixel Lensfree Holographic Microscopy and Tomography
  Using Color Image Sensors," *PLOS ONE* 7(9), e45044 (2012).
  https://doi.org/10.1371/journal.pone.0045044 — works with ordinary Bayer CMOS.
- Wu & Ozcan, "Lensless digital holographic microscopy and its applications,"
  *Methods* (2017). https://doi.org/10.1016/j.ymeth.2017.08.013 — best single
  starting point for the reconstruction pipeline.

### Pollen-specific
- **Mills, Zervas & Grant-Jacob, "Imaging pollen using a Raspberry Pi and LED
  with deep learning," *Sci. Total Environ.* 955, 177084 (2024).**
  https://doi.org/10.1016/j.scitotenv.2024.177084 — **the closest thing to a
  build spec.** PDF: https://eprints.soton.ac.uk/495353/
- "Virtual Impactor-Based Label-Free Pollen Detection using Holography and Deep
  Learning," *ACS Sensors*. https://doi.org/10.1021/acssensors.2c01890 — 92.9%
  accuracy; the Phase 2 target.
- Sauvageat et al., "Real-time pollen monitoring using digital holography,"
  *Atmos. Meas. Tech.* 13, 1539–1550 (2020).
  https://amt.copernicus.org/articles/13/1539/2020/ — Poleno validation;
  accuracy benchmarks + two-step (shape screen → taxon) classification.

### DIY / cheap builds
- Hackaday.io, "Basic lensless imaging for low-cost microscopy."
  https://hackaday.io/project/19677/logs — someone working this exact Phase 0
  problem in public.
- "Cost-Effective, DIY, and Open-Source Digital Lensless Holographic Microscope
  with Distortion Correction," Optica (2024).
  https://opg.optica.org/abstract.cfm?uri=dh-2024-W4A.20 — the failure modes of
  budget diodes + cheap aspherics that the expensive papers never hit.
- "3D-printable portable open-source platform for low-cost lens-less holographic
  cellular imaging." https://arxiv.org/pdf/1904.04497

### Software
- **pyDHM** — https://github.com/catrujilla/pyDHM /
  https://doi.org/10.1371/journal.pone.0275818

### ML
- Assran et al., I-JEPA (Meta AI, 2023) — architecture summarized in §5.
- Pix2Pix: Isola et al., CVPR 2017.
