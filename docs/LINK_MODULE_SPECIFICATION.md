# RCJ Soccer Module — Wireless UART-Bridge Firmware (Link Module) Specification

**Firmware project:** `firmware/RCj_link_module/`
**Firmware version:** 1.0 (`FW_VERSION_MAJOR.FW_VERSION_MINOR` in `definitions.h`)
**Board:** V7 / 2026 (ESP32-C5) — same hardware as the referee module
**Last updated:** 2026-06-29

This document specifies a **separate, from-scratch firmware** (no compatibility with the
referee-app firmware) that lets **two RCJ Soccer modules talk to each other**. For the module
hardware itself (pins, power, OLED, buzzer, buttons), see
[`SPECIFICATION.md`](./SPECIFICATION.md).

---

## 1. Overview

Two modules — each mounted on a separate, moving robot — form a **transparent wireless UART
bridge**. Each robot connects to its module over **UART0** (the U3 header `TX0`/`RX0`). Bytes a
robot sends are relayed **wirelessly (ESP-NOW)** to the paired module and emitted on its UART0
to the other robot, and vice-versa. The two modules first **pair** by proximity.

```
Robot A ──UART0── Module A ))) ESP-NOW (((  Module B ──UART0── Robot B
```

### Required behaviors
1. Holding **Button 2 (B2/IO7) + Button 3 (B3/IO6) together for 5 s** enters **pairing mode**.
   Two modules in pairing mode and physically near each other **auto-complete** pairing; on
   success the **buzzer sounds for 0.5 s**.
2. Paired modules display the **same 4-digit number** on their OLEDs.

### Design decisions
- **Inter-module transport:** ESP-NOW (connectionless Wi-Fi; RSSI gives "near").
- **Robot interface:** UART0 = Arduino `Serial`, on the U3 header `TX0`/`RX0`. Flashing is over
  USB-C, so UART0 is free for robot data at runtime.
- **Pairing target:** the single **nearest** peer with **RSSI ≥ threshold**.
- **Persistence:** the peer MAC + 4-digit code are stored in **NVS** and auto-relinked on boot.

---

## 2. Reused hardware

| Function | GPIO | Notes |
|----------|------|-------|
| Button B1 | IO10 | active-low, sends a test packet |
| Button B2 | IO7 | active-low, pairing gesture (with B3) |
| Button B3 | IO6 | active-low, pairing gesture (with B2) |
| Buzzer | IO26 | passive, LEDC PWM ~2.7 kHz, active-high |
| RGB LED — Red | **IO24** | TX activity, PWM 30% brightness. ⚠️ footprint swap: schematic "Green" pad lights **red** |
| RGB LED — Blue | IO23 | RX activity, PWM 30% brightness |
| RGB LED — Green | **IO27** | kept off. ⚠️ footprint swap: schematic "Red" pad lights **green** |
| OLED (I2C) | IO2 SDA / IO3 SCL | SSD1306 @ 0x3C, 128×64 |
| Robot UART0 | `TX0`/`RX0` (U3) | Arduino `Serial`, default 115200 baud |
| USB-C | — | flashing + (optional) USB test console |

All pins are defined in `firmware/RCj_link_module/definitions.h`.

> **RGB LED footprint swap (hardware):** the board has the Red/Green LED pads swapped, so the
> as-built color differs from the schematic. This firmware drives **IO24 for red** and **IO27 for
> green** so the colors are correct on the real board. See the hardware reference.

---

## 3. ESP-NOW link layer (`espnow_link.cpp`)

- Wi-Fi is brought up in **STA mode, not associated to any AP**, and pinned to a **fixed 2.4 GHz
  channel** (`ESPNOW_WIFI_CHANNEL`, default 1) so both modules' ESP-NOW peers match.
- The IDF `esp_now` API is used directly so the receive callback can read **RSSI**
  (`esp_now_recv_info_t::rx_ctrl->rssi`) and the source MAC.
- The receive callback is kept short: it copies each frame into a **FreeRTOS queue**
  (overwrite-oldest when full, so the newest frame is never lost), which the main loop drains.
- BLE is **disabled** (`CONFIG_BT_ENABLED=n`) — this firmware has no phone-app path.

### Packet format
Every ESP-NOW frame is `[1 type byte][payload]`:

| Type | Value | Direction | Payload |
|------|-------|-----------|---------|
| `PKT_PAIR_BEACON` | 0 | broadcast (pairing) | none |
| `PKT_PAIR_CONFIRM` | 1 | leader → follower | 2-byte code (little-endian) |
| `PKT_PAIR_ACK` | 2 | follower → leader | 2-byte code (little-endian) |
| `PKT_DATA` | 3 | peer → peer | raw UART bytes (≤ `ESPNOW_MAX_PAYLOAD` = 200) |
| `PKT_HEARTBEAT` | 4 | peer → peer | none |

---

## 4. Pairing flow & state machine (`pairing.cpp`)

States: `LINK_UNPAIRED` → `LINK_PAIRING` → `LINK_PAIRED` (plus auto-relink from NVS at boot).

1. **Enter pairing** on the B2+B3 5 s hold (`buttons.cpp`) or the USB `PAIR` command. Bridging
   is paused while pairing.
2. **Discover:** broadcast `PKT_PAIR_BEACON` every `PAIR_BEACON_INTERVAL` (200 ms) and collect
   incoming beacons into a candidate map keyed by MAC, keeping the **strongest RSSI ≥
   `PAIR_RSSI_MIN`** (default −60 dBm).
3. **Code agreement (leader election by MAC):** after `PAIR_SELECT_WINDOW` (1 s), the module
   with the numerically **lower MAC** becomes leader: it generates a code
   `esp_random() % 10000` and unicasts `PKT_PAIR_CONFIRM`. The higher-MAC module is the
   follower and waits for the confirm.
4. **Finalize:** the follower adopts the code, replies `PKT_PAIR_ACK`, and both sides
   `esp_now_add_peer()`, **persist `{peer_mac, code}` to NVS**, start the **0.5 s buzzer**, and
   enter `LINK_PAIRED`. Confirms/acks are retransmitted each beacon tick until acknowledged
   (loss recovery); a re-`PAIR_CONFIRM` to an already-paired follower is re-acked so the leader
   can still finalize.
5. **Timeout:** if no qualifying peer within `PAIRING_TIMEOUT` (30 s), revert to the previous
   state (restoring a prior link if there was one).
6. **Auto-relink:** on boot, if NVS holds a pairing, the peer is re-added and the module goes
   straight to `LINK_PAIRED`. Re-running the gesture re-pairs and overwrites the stored peer.

**4-digit code:** `0000`–`9999`, generated once by the leader, shared to the follower, shown on
both OLEDs (`%04u`), and stored in NVS (`uint16_t`). It is a human-visible confirmation that the
two modules are linked to each other.

**Link liveness:** while paired, each side sends `PKT_HEARTBEAT` every `HEARTBEAT_INTERVAL`
(1 s); a peer is "alive" if any frame arrived within `LINK_TIMEOUT` (3 s). Shown as
`LINKED` / `LINK LOST` on the OLED.

---

## 5. Transparent UART bridge (`bridge.cpp`)

Active only in `LINK_PAIRED`:
- **Robot → peer:** drain UART0 (`Serial`) into a buffer; flush as `PKT_DATA` when it reaches
  `ESPNOW_MAX_PAYLOAD` (200 B) or after `BRIDGE_FLUSH_IDLE_MS` (5 ms) of idle line — balancing
  latency against packetization. Payloads larger than one frame are chunked.
- **Peer → robot:** `PKT_DATA` frames from the paired MAC are written straight out to UART0.
- While unpaired, incoming UART bytes are discarded so the FIFO can't back up.

---

## 6. UART activity monitor (`monitor.cpp`)

Monitors the bridged data (PKT_DATA only — pairing/heartbeat frames are not counted):

- **Byte counters:** running totals of TX (module → peer) and RX (peer → module) bytes, shown
  live on the OLED and in the USB `STATUS` output.
- **Activity LEDs (0.05 s non-blocking flash, PWM-dimmed to 30% brightness):**
  - **Red (IO24)** flashes on each **TX** to the peer.
  - **Blue (IO23)** flashes on each **RX** from the peer.
  - **Green (IO27)** is kept **off**.
  - LEDs are driven by **LEDC PWM** at `LED_DUTY_ON` (= `LED_BRIGHTNESS_PCT` 30% of full) so
    "on" is a soft glow, not full glare. (Red/Green pins account for the footprint swap — see §2.)
- **Test packet:** pressing **B1 (IO10)** — or the USB `TEST` command — sends a fixed
  `RCJLINK-TESTPKT\n` packet to the peer (counts as TX, flashes red).
- **Link RSSI:** the link layer records the RSSI (dBm) of the latest frame from the peer
  (`pairing_rssi()`); heartbeats refresh it ~every second even with no data traffic.

The counters/LED hooks live in `bridge.cpp` at the exact points where `PKT_DATA` is sent
(`flush_tx`, `bridge_inject`, `bridge_send_test`) and received (`bridge_write_uart`).

## 7. Display (`display.cpp`)

Reuses the ThingPulse SSD1306 driver + `fonts.h`. Screens (count refreshes throttled to 150 ms):

| State | Screen |
|-------|--------|
| boot | RC logo + firmware version |
| `LINK_UNPAIRED` | "Not paired — Hold B2 + B3 (5s) to pair" |
| `LINK_PAIRING` | "Pairing… Bring the two modules together" |
| `LINK_PAIRED` | top row: **`LINKED`/`NO LINK`** + **RSSI** (`-NNdBm`); center: large **4-digit code**; bottom row: small **`TX:` / `RX:`** byte counts |

---

## 8. Buzzer & buttons

- **Buzzer (`buzzer.cpp`):** LEDC PWM on IO26 at `BUZZER_FREQ_HZ` (2700 Hz), 50% duty.
  Non-blocking: `buzzer_beep(ms)` starts it; `buzzer_update()` stops it after the interval so the
  bridge stays responsive. Pairing success fires `buzzer_beep(PAIR_BUZZER_MS = 500)`.
- **Buttons (`buttons.cpp`):** B1/B2/B3 as `INPUT_PULLUP`, active-low.
  `buttons_pairing_gesture()` fires **once** after B2+B3 are held together for `PAIRING_HOLD_TIME`
  (5 s); `buttons_test_pressed()` fires **once** per B1 press (30 ms debounce) and sends a test
  packet.

---

## 9. USB test console (`usbtest.cpp`) — bench testing

Gated by `ENABLE_USB_TEST_CONSOLE` (1 by default; set 0 for production). Exposes a line-based
console on the **USB-Serial-JTAG (USB-C)**, **separate from UART0**, so a PC can simulate user
interaction and robot traffic and observe the link. It owns its own Arduino `HWCDC` instance;
because Arduino USB-CDC-on-boot is off, `Serial` stays on UART0.

> **Non-blocking (important):** the console sets `setTxTimeoutMs(0)` and guards writes with
> `if (USBSerial)`. Without this, writing to the console while the USB port is plugged into a
> host that isn't draining it (e.g. powered from a PC with no serial monitor open) blocks the
> HWCDC write ~2 s per line, **stalling the main loop** so the activity LEDs never clear (blue
> stuck on = apparent freeze). With a 0 ms timeout, console output is dropped instead of
> blocking, and the loop/LEDs/bridge keep running.

> **Flashing safety:** the robot bridge never uses the USB port (it is on UART0 / U3 pins), and
> the firmware does not reconfigure native USB. The USB-Serial-JTAG is the same peripheral
> esptool uses; reset-to-bootloader on the ESP32-C5 is hardware-level, so the test console does
> **not** block future flashing.

| Command | Effect |
|---------|--------|
| `PAIR` | enter pairing mode (simulates the B2+B3 hold) |
| `SEND <text>` | inject `<text>` as if the robot sent it on UART0 (→ peer) |
| `TEST` | send the fixed test packet to the peer (simulates the B1 button) |
| `STATUS` | print state, 4-digit code, peer MAC, **link-alive, RSSI**, tx/rx byte counts |
| `HELP` | list commands |

Output: `STATE <…>` on every transition (with `code=NNNN alive=… rssi=… tx=… rx=…` when paired),
and `RX <text>` for each payload received from the peer (the bytes that would go out the robot's
UART0).

---

## 10. Build & flash

```sh
cd firmware/RCj_link_module
idf.py set-target esp32c5      # target pinned in CMakeLists/sdkconfig.defaults
idf.py build
idf.py -p <PORT> flash         # over USB-C
```

- **ESP-IDF v5.5.4**, Arduino-as-component (`espressif/arduino-esp32 ^3.3.0`), target `esp32c5`,
  8 MB flash. Wi-Fi/ESP-NOW enabled, BLE disabled.
- `sdkconfig.defaults` (release) silences bootloader/IDF/Arduino logs and keeps the console on
  UART0 so the robot data line is clean. `sdkconfig.debug` overlay routes logs to USB-Serial-JTAG
  for development:
  `idf.py -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.debug" build flash monitor`.

> One-time `ESP-ROM:` reset burst appears on UART0 at power-on (same as the referee firmware,
> eFuse-only to remove) — note it for the robot's UART parser.

---

## 11. Bring-up checklist & verification

1. Build and flash **two** modules over USB-C.
2. **Pair:** hold B2+B3 5 s on both within range → **buzzer 0.5 s** on each, **same 4-digit
   number** on both OLEDs. (Or send `PAIR` to both over the USB console.)
3. **Persistence:** power-cycle both → auto-relink, same code shown, no re-pairing.
4. **Bridge:** send data on one robot's UART0 (or `SEND <text>` over USB) → it appears on the
   other module's UART0 (and as `RX <text>` on its USB console). Verify both directions.
5. **Monitor:** as data flows, the **TX/RX byte counts** on both OLEDs increment, the **red LED**
   flashes on TX and the **blue LED** on RX (green off, both at ~30% brightness). The OLED top row
   shows **`LINKED`** + **RSSI** (e.g. `-24dBm`). Press **B1** (or `TEST`) → a test packet is sent
   (TX count +16, peer logs `RX RCJLINK-TESTPKT`).
6. **Proximity guard:** with the modules far apart, entering pairing must **not** pair
   (RSSI < threshold).

---

## 12. Tunables (`definitions.h`)

| Constant | Default | Meaning |
|----------|---------|---------|
| `ESPNOW_WIFI_CHANNEL` | 1 | shared 2.4 GHz channel |
| `PAIR_RSSI_MIN` | −60 dBm | proximity threshold for "near" |
| `PAIRING_HOLD_TIME` | 5000 ms | B2+B3 hold to enter pairing |
| `PAIR_SELECT_WINDOW` | 1000 ms | candidate-collection window |
| `PAIRING_TIMEOUT` | 30000 ms | give up pairing |
| `PAIR_BUZZER_MS` | 500 ms | success beep length |
| `LED_BLINK_MS` | 50 ms | TX/RX activity flash length |
| `LED_BRIGHTNESS_PCT` | 30 % | activity-LED PWM brightness |
| `ESPNOW_MAX_PAYLOAD` | 200 B | max bridge bytes per frame |
| `BRIDGE_FLUSH_IDLE_MS` | 5 ms | partial-buffer flush gap |
| `HEARTBEAT_INTERVAL` / `LINK_TIMEOUT` | 1000 / 3000 ms | liveness |
| `BRIDGE_UART_BAUD` | 115200 | robot UART0 baud |
| `ENABLE_USB_TEST_CONSOLE` | 1 | USB test console on/off |

---

## 13. Notes & future work

- ESP-NOW is unencrypted, filtered by paired MAC; LMK/PMK encryption can be added later.
- Both modules must share the Wi-Fi channel (fixed; no AP scan).
- Possible additions: an explicit unpair gesture, link-quality (RSSI) on the OLED, and ESP-NOW
  encryption.
