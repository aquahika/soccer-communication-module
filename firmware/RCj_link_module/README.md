# RCJ Link Module — Wireless UART Bridge

Firmware that turns **two** RCJ Soccer communication modules (V7 / 2026, ESP32-C5) into a
**transparent wireless link** between two robots. Each robot connects to its module over a plain
UART; whatever one robot sends comes out the other module's UART, and vice-versa. The hop between
the two modules is wireless (ESP-NOW) — think of it as a **wireless serial cable**.

```
Robot A ──UART── Module A ))) wireless ((( Module B ──UART── Robot B
```

You use the two modules **as a pair**: pair them once, wire each to a robot, and send data.
No configuration, addresses, or apps required.

---

## 1. Get the modules ready

**If you received pre-flashed modules:** nothing to install — skip to
[Pair the two modules](#2-pair-the-two-modules).

**Otherwise, flash the prebuilt binary** to each module over USB-C:

1. Download the latest merged binary from the
   **[Releases page](https://github.com/aquahika/soccer-communication-module/releases/latest)** —
   e.g.
   [`RCj_link_module-v1.0-merged.bin`](https://github.com/aquahika/soccer-communication-module/releases/download/link-fw-v1.0/RCj_link_module-v1.0-merged.bin).
2. Flash it with [esptool](https://docs.espressif.com/projects/esptool/) (`pip install esptool`):

   ```sh
   esptool --chip esp32c5 -p <PORT> --before default_reset --after hard_reset write_flash 0x0 RCj_link_module-v1.0-merged.bin
   ```

`<PORT>` is e.g. `/dev/cu.usbmodemXXXX` (macOS), `/dev/ttyACM0` (Linux), or `COMx` (Windows).
Flash the **same** binary to **both** modules. (A full flash erases the module, including any
saved pairing — just re-pair afterwards.) Building from source instead? See
[For developers](#for-developers).

---

## 2. Pair the two modules

Do this once per pair of modules:

1. Place the two modules **right next to each other** (they only pair at close range).
2. On **each** module, **hold Button 2 (B2) + Button 3 (B3) together for 5 seconds**. The OLED
   shows `Pairing…`.
3. When they find each other, both **beep for 0.5 s** and show the **same 4-digit number** —
   that confirms they're paired to each other.

Pairing is **remembered**: after a power cycle the two modules **reconnect automatically**. To
re-pair a module with a different partner, just repeat the 5 s B2+B3 hold — it replaces the old
partner.

> Pairing only completes at close range, on purpose, so you don't accidentally pair with another
> team's module across the field.

---

## 3. Wire each module to its robot

Connect the robot to the module's **UART** on the **U3 header**:

| Module (U3) | Robot |
|-------------|-------|
| `TX0` | robot RX |
| `RX0` | robot TX |
| `GND` | robot GND |

- Serial settings: **115200 baud, 8N1**.
- The module's logic level is **3.3 V** — add a level shifter for a 5 V robot input, and always
  share **GND**.
- Power the module as usual (robot battery on VIN, or USB-C). See the
  [main README](../../README.md) for power and mounting.

---

## 4. Send data

Once paired and wired, it just works: **bytes your robot writes to the UART arrive at the other
robot's UART**, both directions, continuously. Use any format you like (text or your own binary
protocol). Longer messages are split across radio packets automatically.

---

## On-module feedback

**OLED (when paired):**

```
LINKED                 -32dBm     <- connection status + signal strength
        6 1 2 2                   <- the shared 4-digit pairing number
   TX:1280   RX:960               <- total bytes sent / received
```

- `LINKED` / `NO LINK` — whether the partner is currently reachable.
- RSSI (dBm) — signal strength (closer to 0 = stronger).
- TX / RX — running totals of bytes sent to / received from the partner.

**RGB LED** (soft glow):

| LED | Meaning |
|-----|---------|
| 🔴 Red blink | data **sent** |
| 🔵 Blue blink | data **received** |

**Buzzer:** short beep when pairing succeeds.

**Buttons:**

| Button | Action |
|--------|--------|
| **B1** | send a test packet to the partner (quick way to check the link) |
| **B2 + B3** (hold 5 s) | pair with a nearby module |

---

## Checking the link

Press **B1** on one module: its red LED blinks (sent) and the partner's blue LED blinks
(received), and the partner's **RX** counter goes up. If `LINKED` shows on both OLEDs and B1
moves the counters, the link is healthy.

---

## Troubleshooting

- **They won't pair** — bring the two modules closer together and make sure you hold **B2+B3
  together** for the full 5 seconds on **both** modules at the same time.
- **Paired but no data** — check that `TX0`/`RX0` aren't swapped, `GND` is shared, and both the
  robot and module use **115200 8N1**. Watch the TX/RX counters and the red/blue LEDs to see
  which direction is moving.
- **OLED shows `NO LINK`** — the partner is out of range or powered off; it reconnects on its own
  when the partner is back.

---

## For developers

This is a self-contained ESP-IDF project (Arduino as a component), target `esp32c5`. Defaults
(UART baud, pairing range, radio channel, LED brightness, …) are compiled in from
[`definitions.h`](definitions.h); end users are not expected to change them.

```sh
cd firmware/RCj_link_module
idf.py set-target esp32c5
idf.py build
idf.py -p <PORT> flash
```

Full design, protocol, tunables, and a USB bench-test console are documented in
[`../../docs/LINK_MODULE_SPECIFICATION.md`](../../docs/LINK_MODULE_SPECIFICATION.md).
