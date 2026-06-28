# RCJ Link Module — Wireless UART Bridge Firmware

Firmware that turns **two** RCJ Soccer communication modules (V7 / 2026, ESP32-C5) into a
**transparent wireless UART link** between two robots. Each robot connects to its module over a
plain UART; whatever one robot sends is delivered out the other module's UART, and vice-versa —
the wireless hop between the two modules uses **ESP-NOW**.

```
Robot A ──UART── Module A ))) ESP-NOW ((( Module B ──UART── Robot B
```

> This is a separate, self-contained firmware (not the referee-app firmware). For the full
> design/reference, see [`../../docs/LINK_MODULE_SPECIFICATION.md`](../../docs/LINK_MODULE_SPECIFICATION.md).

---

## What you need

- **Two** V7 / 2026 modules (ESP32-C5).
- A USB-C cable to flash each one.
- [ESP-IDF **v5.5.4**](https://docs.espressif.com/projects/esp-idf/en/v5.5.4/esp32c5/get-started/index.html)
  installed (Arduino is pulled in automatically as a component).

---

## 1. Build & flash (both modules)

Flash the **same firmware** to both modules.

```sh
cd firmware/RCj_link_module
idf.py set-target esp32c5      # first time only
idf.py build

# flash module 1, then swap the USB-C cable / port and flash module 2
idf.py -p <PORT> flash
```

`<PORT>` is e.g. `/dev/cu.usbmodemXXXX` (macOS), `/dev/ttyACM0` (Linux), or `COMx` (Windows).
Flashing is over USB-C and never conflicts with the robot UART (that's on the U3 header).

---

## 2. Pair the two modules

1. Place the two modules **right next to each other** (pairing requires close proximity).
2. On **each** module, **press and hold Button 2 (B2) + Button 3 (B3) together for 5 seconds**.
   The OLED shows `Pairing…`.
3. When they find each other, both **beep for 0.5 s** and show the **same 4-digit number** —
   that's the pairing confirmation.

The pairing is **saved**: after a power cycle the two modules **reconnect automatically** (no
need to pair again). To pair with a *different* module, just repeat the 5 s B2+B3 hold — it
overwrites the previous partner.

> **Proximity:** pairing only completes when the partner's signal is strong enough
> (`PAIR_RSSI_MIN`, default **−30 dBm** ≈ touching distance). This prevents pairing with the
> wrong module across the field. See [Tuning](#tuning) to change it.

---

## 3. Wire each module to its robot

Connect the robot to the module's **UART0** on the **U3 header**:

| Module (U3) | Robot |
|-------------|-------|
| `TX0` | robot RX |
| `RX0` | robot TX |
| `GND` | robot GND |

- Baud rate: **115200 8N1** (`BRIDGE_UART_BAUD`).
- Logic level is **3.3 V** — add a level shifter for a 5 V robot input. Always share **GND**.
- Power the module as usual (robot battery on VIN, or USB-C). See the
  [main README](../../README.md) for power details.

---

## 4. Use it

Once paired and wired, it's automatic: **any bytes your robot writes to the UART are delivered
to the other robot's UART**, both directions, continuously. There is no addressing or framing to
manage — treat it like a wireless serial cable between the two robots.

- Maximum ~200 bytes are sent per radio packet; longer bursts are split automatically.
- Send whatever protocol you like (text, your own binary format, etc.).

---

## On-module feedback

**OLED (when paired):**

```
LINKED                 -32dBm     <- connection status + live signal strength
        6 1 2 2                   <- the shared 4-digit pairing code
   TX:1280   RX:960               <- total bytes sent / received
```

- `LINKED` / `NO LINK` — whether the partner is currently reachable.
- RSSI (dBm) — link signal strength (closer to 0 = stronger).
- TX / RX — running byte counters for traffic to / from the partner.

**RGB LED** (dim, ~30% brightness):

| LED | Meaning |
|-----|---------|
| 🔴 Red flash | a packet was **sent** (TX) |
| 🔵 Blue flash | a packet was **received** (RX) |
| 🟢 Green | unused (off) |

**Buzzer:** 0.5 s beep on successful pairing.

**Buttons:**

| Button | Action |
|--------|--------|
| **B1** | send a test packet to the partner (handy to check the link) |
| **B2 + B3** (hold 5 s) | enter pairing mode |

---

## Tuning

Common settings live at the top of [`definitions.h`](definitions.h):

| Constant | Default | Meaning |
|----------|---------|---------|
| `BRIDGE_UART_BAUD` | `115200` | robot UART baud rate |
| `PAIR_RSSI_MIN` | `-30` | pairing proximity threshold (dBm; lower = allow farther) |
| `PAIRING_HOLD_TIME` | `5000` | B2+B3 hold time to start pairing (ms) |
| `ESPNOW_WIFI_CHANNEL` | `1` | shared 2.4 GHz channel (must match on both) |
| `LED_BRIGHTNESS_PCT` | `30` | activity-LED brightness (%) |
| `ENABLE_USB_TEST_CONSOLE` | `1` | USB test console on/off |

After changing any constant, **rebuild and reflash both modules**.

---

## Bench testing over USB (optional)

With `ENABLE_USB_TEST_CONSOLE = 1`, each module exposes a line-based console on its **USB-C**
port (separate from the robot UART), so you can drive and observe it from a PC without wiring a
robot. Open the port at any baud (e.g. `screen /dev/cu.usbmodemXXXX 115200`) and type:

| Command | Effect |
|---------|--------|
| `PAIR` | enter pairing mode (same as the B2+B3 hold) |
| `SEND <text>` | send `<text>` to the partner (as if the robot sent it on UART) |
| `TEST` | send the fixed test packet (same as the B1 button) |
| `STATUS` | print state, 4-digit code, partner MAC, link-alive, RSSI, TX/RX bytes |
| `HELP` | list commands |

The console also prints `RX <text>` for every packet received from the partner. Set
`ENABLE_USB_TEST_CONSOLE 0` for a production build.

---

## Troubleshooting

- **Won't pair** — move the modules closer (proximity threshold is strict by default), and make
  sure you hold **B2+B3 together** for the full 5 s on **both** modules. Lower `PAIR_RSSI_MIN`
  to allow pairing from farther away.
- **Paired but no data** — check `TX0/RX0` aren't swapped, GND is shared, and both sides use
  **115200 8N1**. Watch the TX/RX counters and the red/blue LEDs to see which direction moves.
- **`NO LINK` on the OLED** — the partner is out of range or powered off; it recovers
  automatically when the partner is back.
- **Flashing fails** — the robot UART is on the U3 header, not USB, so USB flashing should
  always work; if needed, hold BOOT and tap RESET to force the bootloader.

---

## Layout

| File | Role |
|------|------|
| `espnow_link.*` | ESP-NOW transport (Wi-Fi STA, packets, RX queue) |
| `pairing.*` | pairing handshake, NVS persistence, link state, RSSI |
| `bridge.*` | UART0 ⇄ ESP-NOW data path + test packet |
| `monitor.*` | TX/RX counters and activity LEDs |
| `buttons.*` / `buzzer.*` / `display.*` | inputs, beeper, OLED |
| `usbtest.*` | USB bench-test console |
| `definitions.h` | pins and all tunables |
| `main/app_main.cpp`, `RCj_link_module.ino` | ESP-IDF / Arduino entry points |
