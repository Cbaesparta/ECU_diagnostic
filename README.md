# ECU Diagnostic System

Real-time automotive ECU diagnostic firmware for **STM32F429ZITx**
with an Arduino mock ECU and an ESP32 web dashboard.

```
[Arduino + MCP2515 + TJA1050] ──CAN bus─► [STM32F429 + TJA1050] ──USART2─► [ESP32] ──WiFi─► Browser
     (mock ECU)                               (diagnostic core)              (bridge)         (dashboard)
```

---

## Hardware Required

| Component | Quantity | Notes |
|-----------|----------|-------|
| STM32F429I-DISC1 | 1 | Main diagnostic board |
| Arduino (Uno / Nano) | 1 | Mock ECU |
| ESP32 (DevKit) | 1 | Web dashboard bridge |
| MCP2515 CAN module | 1 | On Arduino side |
| TJA1050 CAN transceiver | 2 | One per CAN node |
| 120 Ω resistor | 2 | CAN bus termination |

> **Note:** Many MCP2515 breakout boards already include a TJA1050 chip.
> If yours does, you only need **one** external TJA1050 for the STM32 side.

---

## Wiring

### CAN Bus

```
STM32F429-DISC1           TJA1050 #1           CAN Bus         TJA1050 #2     MCP2515 module
  PA11 (CAN1_RX) ──────► RXD                                        TXD ◄──── TXD
  PA12 (CAN1_TX) ◄────── TXD        CANH ══════ CANH ══════ CANH        TXD ─► TXD
  3.3V           ──────► VCC        CANL ══════ CANL ══════ CANL        RXD ◄── RXD
  GND            ──────► GND                                        GND       GND

                          120 Ω between CANH/CANL at each end
```

### USART2 (STM32 → ESP32)

```
STM32F429-DISC1     ESP32 DevKit
  PA2  (TX) ──────► GPIO16 (RX2)
  PA3  (RX) ◄────── GPIO17 (TX2)
  GND       ──────► GND  ← REQUIRED!
```

> Both sides are 3.3 V logic — no level shifter needed.

### Arduino + MCP2515

```
Arduino          MCP2515 module
  D10  (SS)  ──► CS
  D11  (MOSI)──► SI
  D12  (MISO)◄── SO
  D13  (SCK) ──► SCK
  D2   (INT) ◄── INT
  5V   or 3.3V──► VCC  (check your module)
  GND        ──► GND
```

---

## Software Setup

### STM32 Firmware

1. Open **STM32CubeIDE** and regenerate HAL + FreeRTOS sources from `ECU_diagnostic.ioc`.
   This creates `Drivers/` and `Middlewares/` automatically.
2. The application source files (`Core/Src/`, `Core/Inc/`) are already in this repo — **do not overwrite them**.
3. Build with `Project → Build Project` (arm-none-eabi-gcc, Release or Debug).
4. Flash via ST-Link SWD (`Run → Debug` or `Run → Run`).

**Required STM32Cube firmware package:** `STM32Cube FW_F4 V1.28.3`

### Arduino Mock ECU

1. Install library via Arduino IDE Library Manager:
   - **MCP_CAN** by coryjfowler (search "MCP_CAN")
2. Open `Arduino/mock_ecu/mock_ecu.ino`.
3. **Check crystal frequency:** open the file and set `MCP_CRYSTAL`:
   - `MCP_16MHZ` — for modules with a 16 MHz oscillator (silver/shielded)
   - `MCP_8MHZ`  — for modules with an 8 MHz oscillator (most blue boards)
4. Select your board (Uno / Nano), upload.

### ESP32 Web Dashboard

1. Install libraries via Arduino IDE Library Manager:
   - **ArduinoJson** by Benoit Blanchon (v6.x)
2. Open `ESP32/web_dashboard/web_dashboard.ino`.
3. Optionally edit `AP_SSID` / `AP_PASS` or switch to Station mode.
4. Select **ESP32 Dev Module**, upload.
5. Connect a device to the `ECU_Dashboard` WiFi network (password: `ecu12345`).
6. Navigate to **http://192.168.4.1**

---

## CAN Frame Protocol

Standard 11-bit frames at **1 Mbps**, sent by the Arduino mock ECU:

| ID     | Signal         | DLC | Encoding |
|--------|----------------|-----|----------|
| `0x100` | Engine RPM    | 2   | `uint16` big-endian, 0–8000 RPM |
| `0x101` | Speed         | 1   | `uint8`, 0–255 km/h |
| `0x102` | Engine Temp   | 1   | `uint8` = actual_°C + 40 (range -40..215 °C) |
| `0x103` | Battery       | 2   | `uint16` big-endian, millivolts (e.g. 12500 = 12.50 V) |
| `0x104` | Throttle      | 1   | `uint8`, 0–100 % |

---

## JSON Telemetry Format (STM32 → ESP32)

Sent every **500 ms** over USART2 @ 115200 8N1:

```json
{
  "t": 12345,
  "rpm": 1500, "spd": 60, "tmp": 85, "bat": 12500, "tps": 45,
  "can": {
    "ok": 1, "tec": 0, "rec": 0, "fps": 50,
    "rx": 1000, "err": 0, "to": 0, "boff": 0, "erp": 0, "lec": 0
  },
  "rtos": {
    "heap": 70000, "mheap": 68000, "ntasks": 4,
    "wm": [["CAN_Rx",200],["CAN_Diag",150],["UART_Tx",300],["RTOS_Mon",120]]
  }
}
```

| Field | Description |
|-------|-------------|
| `t` | STM32 uptime (ms) |
| `bat` | Battery voltage in mV |
| `can.ok` | 1 = bus active and receiving frames |
| `can.tec/rec` | CAN hardware error counters (≥96 = error passive) |
| `can.fps` | Frames received per second |
| `can.to` | 1 = no frame for >1 s |
| `can.boff` | 1 = bus-off condition |
| `can.lec` | Last Error Code (0=none, 1-7 = error type) |
| `rtos.heap` | FreeRTOS free heap (bytes) |
| `rtos.wm` | `[name, words_free]` stack watermarks |

---

## FreeRTOS Tasks

| Task | Priority | Period | Function |
|------|----------|--------|----------|
| `CAN_Rx` | HIGH (32) | event-driven | Processes frames from ISR queue, decodes vehicle data |
| `CAN_Diag` | ABOVE_NORMAL (28) | 200 ms | Reads TEC/REC, computes frame rate, detects bus-off/timeout |
| `UART_Tx` | NORMAL (24) | 500 ms | Formats JSON and sends to ESP32 |
| `RTOS_Mon` | BELOW_NORMAL (20) | 1 s | Snapshots heap and stack watermarks |

---

## LED Indicators (STM32F429I-DISC1)

| LED | Pin | Meaning |
|-----|-----|---------|
| LD3 (green) | PG13 | Toggles every 50 CAN frames received |
| LD4 (red) | PG14 | Toggles on CAN error; rapid blink = firmware fault |

---

## Troubleshooting

| Symptom | Check |
|---------|-------|
| No CAN frames on STM32 | Verify TJA1050 wiring, 120 Ω termination at both ends, matching baud rate on Arduino |
| MCP2515 init fails | Check crystal frequency (`MCP_8MHZ` vs `MCP_16MHZ`), SPI wiring, 5 V vs 3.3 V |
| ESP32 shows `—` values | Confirm PA2→GPIO16, GND connected; check STM32 serial output with USB-UART adapter |
| Dashboard not loading | Ensure connected to `ECU_Dashboard` AP; try `http://192.168.4.1` |
| `bus_off` in dashboard | CAN wiring fault — check for short on CANH/CANL, missing termination, or ground loop |