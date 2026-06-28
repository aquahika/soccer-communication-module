# RCJ Soccer Communication Module — Firmware & Hardware Specification

**Board:** V7 / 2026 (ESP32-C5)
**Firmware version:** 0.97 (`FW_VERSION_MAJOR.FW_VERSION_MINOR` in `definitions.h`)
**Last updated:** 2026-06-29

This document consolidates the firmware and hardware specifications of the RoboCupJunior
Soccer referee-to-robot communication module. It is a standalone summary; the in-depth
per-subsystem notes live in [`docs/ai/`](./ai/).

---

## 1. Overview

The module is a small referee-to-robot interface for **RoboCupJunior Soccer**. A referee
drives it over **Bluetooth Low Energy (BLE)** from the mobile app, and the module relays the
live match state — **PLAY, STOP, penalty, half-time, game over** — to the robot over simple
digital outputs and/or a UART serial line.

- BLE peripheral built around an **ESP32-C5-WROOM-1** (RISC-V, Wi-Fi 6, BLE 5).
- On-board **OLED display**, **3 buttons**, **RGB LED**, **buzzer**, **USB-C** (power +
  flashing) and a **15 F supercapacitor** backup.
- Powered directly from the robot battery so it stays connected for the whole match.
- Does **not** count toward the robot weight limit.

> A legacy 2024 board uses an **ESP32-C6** (no USB-C). It is maintained on the
> `legacy/esp32-c6` branch and is out of scope for this document.

---

## 2. Hardware Specification

### 2.1 Microcontroller (U1)

| Parameter | Value |
|-----------|-------|
| Part | ESP32-C5-WROOM-1 (schematic label `-N16R4`) |
| Architecture | RISC-V single-core, Wi-Fi 6, BLE 5 |
| Flash | **8 MB** (measured via JEDEC ID; schematic marking `-N16R4`/16 MB is a labeling discrepancy) |
| PSRAM | 4 MB |
| Package | 38-pin module |

The firmware is configured for **8 MB** flash (`CONFIG_ESPTOOLPY_FLASHSIZE_8MB`). Treat 8 MB
as ground truth.

### 2.2 GPIO pin assignment

| GPIO | Net | Function | Notes |
|------|-----|----------|-------|
| IO2  | SDA   | I2C SDA | OLED display data (H2 header) |
| IO3  | SCL   | I2C SCL | OLED display clock (H2 header) |
| IO4  | RX1   | UART1 RX | Secondary serial from robot (U3) |
| IO5  | TX1   | UART1 TX | Secondary serial to robot (U3) |
| IO6  | B3    | Button B3 (SW2) | Pull-up R4 10 kΩ, active-low |
| IO7  | B2    | Button B2 (SW1) | Pull-up R5 10 kΩ, active-low |
| IO8  | OUT2  | Robot output 2 | U3 connector |
| IO9  | OUT1  | Robot output 1 | U3 connector |
| IO10 | B1    | Button B1 (SW5) | Pull-up R6 10 kΩ, active-low |
| IO13 | USB D− | USB | via R10 33 Ω |
| IO14 | USB D+ | USB | via R11 33 Ω |
| IO23 | B     | LED Blue | R7 470 Ω, active-high |
| IO24 | G (schematic) | LED — **lights Red** ⚠️ | R8 470 Ω (footprint swap, see §2.4) |
| IO26 | BUZZER | Buzzer driver | Q1 base via R3 470 Ω, drive high |
| IO27 | R (schematic) | LED — **lights Green** ⚠️ | R9 470 Ω (footprint swap, see §2.4) |
| IO28 | GPIO28 | Robot GPIO | H1 header pin 2 |
| RX0  | RX_OUT | UART0 RX | U3; primary serial / flashing |
| TX0  | TX_OUT | UART0 TX | U3; primary serial / flashing |

> Pins used by firmware are defined in `definitions.h`: `I2C_SDA=2`, `I2C_SCL=3`,
> `BUTTON=10`, `BUTTON2=7`, `OUTPUT1=9`, `OUTPUT2=8`.

### 2.3 Buttons

Three inputs, all **active-low** (10 kΩ pull-up to 3.3 V), footprint `TS-1088-AR02016`
(SW1/SW2) and a 4-pin slide/toggle `TS-1002S-07026C` (SW5).

| Designator | Net | GPIO | Role |
|------------|-----|------|------|
| SW5 | B1 | IO10 | Main/power button (penalty + 5 s hold = disconnect) |
| SW1 | B2 | IO7  | General-purpose button (also triggers penalty) |
| SW2 | B3 | IO6  | General-purpose button |

Debounce in software (20–50 ms recommended).

### 2.4 RGB LED (LED1)

| Parameter | Value |
|-----------|-------|
| Part | TC5050RGBF08-3CJH-AF53A, common-cathode RGB |
| Current limiting | R7/R8/R9, all 470 Ω to 3.3 V |
| Drive | Active-high; PWM supported on all 3 channels |

> ⚠️ **Red/Green pads are SWAPPED on the real board (footprint bug).** The part's physical
> pin order is G,R,B but the footprint is wired R,G,B. **Drive IO24 to show Red and IO27 to
> show Green.** Blue (IO23) is correct. Confirmed on hardware 2026-06-02.

### 2.5 Buzzer (BUZZER1)

| Parameter | Value |
|-----------|-------|
| Type | Passive buzzer, ~2.7 kHz resonant |
| Driver | NPN transistor Q1 (BC817-40), base resistor R3 470 Ω |
| Flyback diode | D1 (1N4148W-7-F) |
| GPIO | IO26 — drive PWM near 2.7 kHz for max volume |

### 2.6 OLED display (I2C, H2 header)

- 128×64 SSD1306/SH1106 class, I2C address `0x3C`.
- SDA = IO2, SCL = IO3, power 3.3 V. Up to 400 kHz (Fast Mode).
- No on-board I2C pull-ups visible; rely on the OLED module's pull-ups.

### 2.7 USB-C (USB1)

| Parameter | Value |
|-----------|-------|
| Connector | TYPE-C 16PIN 2MD(073) |
| Data | D+ = IO14 (R11 33 Ω), D− = IO13 (R10 33 Ω) |
| CC | R12/R13 5.1 kΩ to GND → **UFP (device)** mode |
| VBUS protection | D2 (1N5819HW-7-F) |
| Use | Firmware flashing (USB-CDC) and power input (VBUS → VIN) |

### 2.8 Power system

```
USB-C VBUS ──D2──► VIN ──► U4 (LMR51610XDBVR buck) ──► 3.3 V rail ──► all logic
Robot battery ───► VIN
```

| Item | Detail |
|------|--------|
| Input (VIN) | **4.5 V – 50 V** (RCJ rules cap robot voltage at 50 V) |
| Buck regulator | U4 LMR51610XDBVR → 3.3 V, inductor 22 µH; FB divider R1 100 kΩ / R2 32 kΩ |
| 3.3 V supply option | May feed 3V3 pin directly; design for **~0.5 A startup (≈1.65 W)** |
| Supercapacitor | C1 **15 F**, charged via R14 15 Ω, discharge protection D3 (SS34), enable slide switch U6 (MSS12C02LS) |

The supercapacitor rides through brief supply dips (battery sag). Turn its switch **ON
before a match**, **OFF in storage**. No firmware action required.

### 2.9 Connectors

**H1 — 4-pin power/GPIO header** (PZ254V-11-04P): `1=GND, 2=GPIO28, 3=3.3V, 4=VIN`.

**U3 — 6-pin robot interface** (2541WV-06P):

| Pin | Signal | Direction (from module) |
|-----|--------|--------------------------|
| 1 | OUT1   | Output to robot |
| 2 | OUT2   | Output to robot |
| 3 | RX_OUT | UART0 TX |
| 4 | TX_OUT | UART0 RX |
| 5 | RX1    | UART1 RX |
| 6 | TX1    | UART1 TX |

**H2 — 4-pin I2C header** for the OLED display.

> Net names RX_OUT/TX_OUT are from the **module's perspective**; cross RX↔TX when wiring to a
> robot MCU. Always share GND with the robot. Never connect VIN to a robot GPIO or to 3.3 V.

### 2.10 Robot wiring summary

| Method | Pin | PLAY / GO | STOP |
|--------|-----|-----------|------|
| Output pin | `OUT1` or `OUT2` | **3.3 V** (HIGH) | **0 V** (LOW) |
| UART | `TX0` (UART0, U3) | sends `PLAY` | sends `STOP` |

Both outputs carry the same 3.3 V logic signal. Add a level shifter for 5 V inputs.

---

## 3. Firmware Specification

### 3.1 Architecture

- **ESP-IDF** application with **Arduino-as-component** (`espressif/arduino-esp32 ^3.3.0`),
  built for target `esp32c5` on ESP-IDF **v5.5.4**. BLE stack: **NimBLE**.
- Two parallel entry points doing the same 4-step setup/loop:
  - `RCj_comm_module.ino` — Arduino IDE (`setup()`/`loop()`).
  - `main/app_main.cpp` — ESP-IDF (`app_main()` → `initArduino()` + setup/loop). **CI builds
    this path.**

> ⚠️ Any change to setup/loop logic must be mirrored in **both** entry points.

**Setup:** `Serial.begin(115200)` → `display_init()` → `module_init_gpios()` → `stm_init()`
→ `ble_start_server()`.

**Loop (single thread):**
1. `ble_msg_processing()` — pop ≤1 message from the queue and dispatch (non-blocking).
2. `stm_update()` — run the current state handler; update output pins on state change.
3. `check_disconnect_button()` — 5 s hold (`DISCONNECT_HOLD_TIME`) on the main button → BLE disconnect.
4. `check_penalty_button()` — double-press on B1 or B2 → self-penalty request.

**Concurrency:** the only cross-context structure is the BLE message queue
(`xQueueCreate(16, sizeof(ble_msg_t))`). BLE callbacks (NimBLE host context) enqueue;
the main loop consumes. Queue is **overwrite-oldest on full**, so the newest referee command
is never lost to a backlog.

### 3.2 BLE protocol

The module is a BLE **peripheral / GATT server** using the **Nordic UART Service (NUS)**.

- **Advertised name:** `RCJs-m_<BT-MAC>` (e.g. `RCJs-m_AA:BB:CC:DD:EE:FF`). The MAC is in the
  name because iPhones do not expose the BLE MAC to apps; the same MAC is shown as a **QR
  code** on the wait screen.
- Advertising restarts automatically on disconnect.

| Role | UUID | Properties |
|------|------|------------|
| Service | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` | NUS |
| RX (app → module) | `6E400002-…` | `WRITE` + `WRITE_NR` |
| TX (module → app) | `6E400003-…` | `NOTIFY` |

**Connection parameters** (`definitions.h`):

| Constant | Value | Meaning |
|----------|-------|---------|
| `BLE_CONN_INTERVAL_MIN` | 12 | 15 ms |
| `BLE_CONN_INTERVAL_MAX` | 24 | 30 ms |
| `BLE_CONN_LATENCY` | 0 | no slave latency |
| `BLE_CONN_TIMEOUT` | 300 | **3 s** (lowered from 20 s in v0.97 for faster reconnect + fail-safe stop) |

**Message framing:** `byte[0] = msg_id`, `byte[1..N] = payload` (≤ `BLE_DATA_MAX_LENGTH = 10`
bytes). Writes with total length outside `1..11` bytes are silently dropped.

**Message IDs** (`ble_msg_id` enum — ordinals are the wire IDs; **do not reorder**):

| ID | Value | Direction | Payload → Effect |
|----|-------|-----------|------------------|
| `BLE_MSG_PING` | 0 | app → module | echoes the same bytes back |
| `BLE_MSG_FW_VERSION` | 1 | app → module | replies `[id, MAJOR, MINOR]` |
| `BLE_MSG_SET_NAME` | 2 | app → module | 2 ASCII chars → indicator (display) |
| `BLE_MSG_SET_SCORE` | 3 | app → module | `[my, opp]` → scores (display) |
| `BLE_MSG_PLAY` | 4 | app → module | → `STM_PLAY`, **OUT1/2 HIGH** |
| `BLE_MSG_STOP` | 5 | app → module | → `STM_STOP`, OUT1/2 LOW |
| `BLE_MSG_DAMAGE` | 6 | app → module | uint32 BE ms → timer + `STM_DAMAGE`, OUT LOW |
| `BLE_MSG_HALF_BREAK` | 7 | app → module | uint32 BE ms → timer + `STM_HALF_TIME`, OUT LOW |
| `BLE_MSG_GAME_OVER` | 8 | app → module | `[my, opp]` → scores + `STM_GAME_OVER`, OUT LOW |
| `BLE_MSG_DISCONNECT` | 9 | module → app | sent before dropping the link |
| `BLE_MSG_ASK_FOR_PENALTY` | 10 | module → app | sent on local double-press, **only while `STM_PLAY`** |
| `BLE_MSG_MAX_ID` | 11 | — | sentinel |

- Timer payloads (`DAMAGE`/`HALF_BREAK`) are **big-endian uint32 milliseconds**.
- No authentication/pairing: any BLE client can write commands.
- Unknown IDs and malformed lengths are ignored; no error/NACK is returned.

### 3.3 State machine

Single `current_state` + one-shot `state_changed` flag (`state_machine.cpp`).

| State | Value | Entered by | Outputs | Display |
|-------|-------|-----------|---------|---------|
| `STM_INIT` | 0 | boot | unchanged | logo + version, then 2 s → `STM_DISCONNECTED` |
| `STM_DISCONNECTED` | 1 | boot / BLE disconnect | LOW | "Wait for connection" (QR + MAC) |
| `STM_PLAY` | 2 | `BLE_MSG_PLAY` | **HIGH** | PLAY + indicator + score |
| `STM_STOP` | 3 | `BLE_MSG_STOP` | LOW | STOP |
| `STM_DAMAGE` | 4 | `BLE_MSG_DAMAGE` | LOW | PENALTY + countdown |
| `STM_HALF_TIME` | 5 | `BLE_MSG_HALF_BREAK` | LOW | HALFTIME + countdown |
| `STM_GAME_OVER` | 6 | `BLE_MSG_GAME_OVER` | LOW | GAME OVER + score |
| `STM_UPDATING` | 7 | unused | — | none |

- **Output rule:** `update_output_state()` runs only on state change —
  `robot_play ? (OUT1=OUT2=HIGH, Serial "PLAY") : (OUT1=OUT2=LOW, Serial "STOP")`.
  `robot_play` is `true` only in `STM_PLAY`.
- **Rendering:** `STM_PLAY/STOP/DAMAGE/HALF_TIME/GAME_OVER` redraw every loop (so countdowns
  update live); `STM_DISCONNECTED` redraws only on `state_changed` (a `SET_SCORE`/`SET_NAME`
  while disconnected is stored but not drawn until the next state change).
- **Timers:** `stm_set_timer(ms)` sets `timer_stop = millis()+ms`; `get_remaining_time()`
  counts down to 0. The firmware does **not** auto-transition at 0 — the app must send
  `PLAY`/`STOP`. (Returning robots to play is a referee/team action per the rules.)
- **Fail-safe:** BLE disconnect → `STM_DISCONNECTED` → outputs LOW (robot stops). The 3 s
  supervision timeout bounds how long a silently-dropped link keeps the robot playing.

### 3.4 Display & user feedback

128×64 OLED via the ThingPulse SSD1306 driver + `QRcodeOled`. Per-state screens render
PLAY/STOP/PENALTY/HALFTIME/GAME OVER with a large 2-char **indicator**, the **score**
(`my:opp`), and a live countdown for penalty/half-time. The wait screen shows a QR of the BLE
MAC plus the MAC as text.

> **RGB LED and buzzer feedback are not yet implemented** in firmware — planned for a future
> release (remember the LED Red/Green footprint swap from §2.4 when implementing).

### 3.5 Serial / UART behavior

- `Serial` (UART0, 115200 baud) routes to the U3 `TX_OUT` pin the robot reads.
- The **only** active serial output is `PLAY` / `STOP`, printed on each output change — a
  plain-UART status channel for robots that read serial instead of the OUT pins.
- Release builds silence all bootloader/IDF/Arduino-HAL logs so UART0 carries only
  `PLAY`/`STOP`. (The one-time ROM reset line at power-on cannot be removed without eFuse and
  is accepted as-is.)
- There is **no robot-to-robot UART protocol** (framing/channel select) in the current
  firmware — planned for the future.

### 3.6 Build flavors

| | Release (default, CI) | Debug overlay |
|---|---|---|
| Config | `sdkconfig.defaults` | `+ sdkconfig.debug` |
| IDF/bootloader logs | OFF | INFO |
| Console route | UART0 | USB-Serial-JTAG (USB-C) |
| `Serial` PLAY/STOP | UART0 | UART0 |

Debug build: `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.debug" build flash monitor`.

---

## 4. Build, Release & Flashing

- **Local build:** `cd firmware/RCj_comm_module && idf.py set-target esp32c5 && idf.py build`,
  flash with `idf.py -p <PORT> flash monitor`.
- **Release pipeline** (`.github/workflows/firmware-release.yml`): triggered on a tag matching
  `fw-*` or `v*`. It builds with ESP-IDF v5.5.4, runs
  `tools/prepare_firmware_release.py`, publishes a GitHub Release with the `.bin` files, and
  deploys the web flasher to GitHub Pages.

  ```sh
  git tag fw-v0.97
  git push origin fw-v0.97
  ```

  > Keep `FW_VERSION_MAJOR/MINOR` in `definitions.h` in sync with the tag.

- **Release artifacts:** a single **merged image** (bootloader + partition table + app, at
  offset `0x0`) plus individual `app` / `bootloader` / `partition-table` bins, with
  `manifest.json` / `version.json` for the web flasher.
- **Web flasher** (`web/flasher/`): static site using **esptool-js 0.6.0** + the browser
  **Web Serial API** (desktop **Chrome/Edge** only). `CHIP_FAMILY="ESP32-C5"`,
  `RESET_MODE="usb_reset"`, `BAUD_RATE=115200`. Connect over USB-C, flash erases fully, then
  resets via DTR/RTS toggling. Hosted at
  <https://robocup-junior.github.io/soccer-communication-module/>.

---

## 5. Quick Firmware Bring-up Checklist

- [ ] LED: `OUTPUT` on IO23/IO24/IO27 — **IO24 = red, IO27 = green** (footprint swap), IO23 = blue.
- [ ] Buttons: `INPUT` (pull-up) on IO10/IO7/IO6, active-low; debounce 20–50 ms.
- [ ] Buzzer: `OUTPUT` PWM ~2.7 kHz on IO26.
- [ ] I2C OLED on SDA=IO2 / SCL=IO3, address `0x3C`.
- [ ] Robot outputs: `OUTPUT` on IO9 (OUT1) / IO8 (OUT2).
- [ ] UART0 = USB serial / flashing + `PLAY`/`STOP` status; UART1 (RX1/TX1) free for robot.
- [ ] GPIO28 exposed on H1 for optional robot-side signaling.

---

## 6. Known Issues & Open Points

- **LED Red/Green footprint swap** (hardware) — drive IO24 for red, IO27 for green until a
  board respin fixes it.
- **Flash marking mismatch** — schematic `-N16R4` vs measured 8 MB; firmware uses 8 MB.
- **Two entry points** (`.ino` + `app_main.cpp`) must be kept in sync.
- **Unexplained `660000 ms` init timer** in `stm_init()` (harmless; flagged in source).
- **No auto-transition** when the penalty/half-time timer hits 0 — app-driven by design.
- **RGB LED / buzzer firmware feedback** not yet implemented in this (referee) firmware.
- **Robot-to-robot communication** is implemented by a **separate firmware** — see
  [`LINK_MODULE_SPECIFICATION.md`](./LINK_MODULE_SPECIFICATION.md) (a wireless UART bridge
  between two modules over ESP-NOW, with proximity pairing and a shared 4-digit code).

For deeper per-subsystem detail see [`docs/ai/`](./ai/) — particularly
`ESP32C5_RCJ_modul_hardware_reference.md`, `02_firmware_architecture.md`,
`03_ble_protocol.md`, `04_state_machine.md`, and `07_build_release_and_flasher.md`.
