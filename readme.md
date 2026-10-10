# ESP32 Voltage & Over-Current Protector

## How to run (Wokwi)

Click this link : **[https://wokwi.com/projects/477393031120734209](https://wokwi.com/projects/477393031120734209)** and simulate by pressing the play button !

## User manual

### 1.1 Meet the controls

| Control | Where | Use |
| --- | --- | --- |
| **P1** pot | Left | Simulates supply voltage (POT mode) |
| **P2** pot | Left | Simulates load current |
| **SET** button | GPIO32 | Open the settings menu / skip a setting |
| **ACCEPT / REJECT** button | GPIO33 | 1 click = accept, 2 clicks = reject |
| **+** button | GPIO25 | Increase a value |
| **-** button | GPIO27 | Decrease a value |
| **3 LEDs** | Right | The appliances. On = powered, off = disconnected |
| **LCD** | Top | Live readings and menus |

Pot guide:

| Pot position | Voltage (P1) | Current (P2) |
| --- | --- | --- |
| 25% | 75 V | 5 A |
| 50% | 150 V | 10 A |
| 60% | 180 V | 12 A |
| 77% | 230 V | 15 A |
| 83% | 250 V | 17 A |
| 100% | 300 V | 20 A |

### 1.2 The main screen

```
V:230.0 I: 5.0
LOAD ON       P
```

| Part | Meaning |
| --- | --- |
| `V:` / `I:` | Live voltage (volts) and current (amps) |
| `LOAD ON` | Relay closed, appliances powered |
| `OFF:<fault>` | Relay open, with the reason (see 5.5) |
| Last letter | `P` = potentiometer mode, `A` = AUTO random mode |

On the main screen, **one click on ACCEPT** reconnects the load after a trip, but only if the supply is safe. Otherwise the LCD shows `Supply not safe`.

### 1.3 Changing settings on the device

1. Press **SET**. The LCD shows `1/6 OVER-VOLT limit`.
2. Use **+** and **-** to change the value.
3. Choose what to do:

| Action | Result |
| --- | --- |
| **ACCEPT** (1 click) | Saves the value and goes to the next setting |
| **REJECT** (double-click ACCEPT) | Cancels and returns to the main screen |
| **SET** | Skips this setting without changing it |

The ACCEPT click takes about 0.4 s to register, because the device waits to see if a second click follows.

After the last setting (6/6), the device returns to the main screen.

| Page | Setting | Range | Step | Default |
| --- | --- | --- | --- | --- |
| 1/6 | Over-voltage limit | 100-300 V | 10 V | 250 V |
| 2/6 | Under-voltage limit | 50-250 V | 10 V | 180 V |
| 3/6 | Over-current limit | 1-20 A | 1 A | 10 A |
| 4/6 | Trip delay | 20-1000 ms | 20 ms | 120 ms |
| 5/6 | Auto-reconnect delay | 0-60 s (0 = manual only) | 1 s | 5 s |
| 6/6 | Supply source | POT / AUTO | toggle | POT |

Rules: the OV limit must be **at least 20 V above** the UV limit. If the combination is invalid, the LCD shows `Invalid! kept old` and the old value stays.

### 1.4 Changing settings from a computer (Using the terminal in wokwi)

Open the **Serial Monitor** (115200 baud), type a command, and press Enter. Commands are not case-sensitive.

| Command | What it does |
| --- | --- |
| `HELP` | Lists all commands |
| `STATUS` | Shows live V, I, relay state and fault |
| `GET` | Shows all current settings |
| `SET OV <volts>` | Over-voltage limit, e.g. `SET OV 240` |
| `SET UV <volts>` | Under-voltage limit, e.g. `SET UV 190` |
| `SET OC <amps>` | Over-current limit, e.g. `SET OC 8` |
| `SET DELAY <ms>` | Trip delay, e.g. `SET DELAY 100` |
| `SET RECON <seconds>` | Auto-reconnect delay, `0` = manual reset only |
| `SIM POT` | Use the potentiometer for voltage |
| `SIM AUTO` | Use random voltage |
| `RESET` | Reconnect the load (only if the supply is safe) |
| `RELAY OFF` | Switch the load off manually (no auto-reconnect; use `RESET`) |
| `STREAM ON` / `STREAM OFF` | Send `time_ms,V,I,relay` every 0.5 s, useful for plotting |
| `DEFAULTS` | Restore factory settings |

Answers start with `OK` on success or `ERR:` if the value is invalid or out of range. Setting changes from the computer are saved immediately, the same as on the device.

When a trip happens, the device prints a line such as:

```
!! TRIP: OVER-VOLT (V=262.3 I=4.98)
```

### 1.5 Fault messages

| LCD text | Cause | Trips after |
| --- | --- | --- |
| `OFF:OVER-VOLT` | Voltage above the OV limit | Trip delay (120 ms) |
| `OFF:UNDER-VOLT` | Voltage below the UV limit | Trip delay |
| `OFF:OVER-CURR` | Current above the OC limit | Trip delay |
| `OFF:SHORT-CKT` | Current above **2 x** the OC limit | **Immediately** |
| `OFF:MANUAL-OFF` | You sent `RELAY OFF` | n/a |

With the default 10 A limit, a short circuit would need more than 20 A, which the pot cannot reach. To demonstrate it, first send `SET OC 5`.

### 1.6 Reconnecting after a trip

The device reconnects automatically when **all** of these are true for the whole reconnect delay (default 5 s):

- Voltage is at least 5 V inside both limits (this margin stops the relay from clicking on and off)
- Current is below 90% of the OC limit

If the reconnect delay is `0`, or the stop was `MANUAL-OFF`, reconnect with one click of **ACCEPT** or the `RESET` command.

### 1.7 AUTO (random) mode

In AUTO mode the voltage is picked at random, and the current still comes from P2. About 60% of picks are in the safe range and about 40% are anywhere from 0 to 300 V, so you see trips and reconnects without touching the pot.

Enable it with `SIM AUTO` or settings page 6/6. The change interval is set at the top of `sketch.ino`:

| Constant | Meaning | Value |
| --- | --- | --- |
| `RAND_MIN_MS` / `RAND_MAX_MS` | Time between random picks | Currently `2000L` (2 s). Use `10000L` / `20000L` for 10-20 s |
| `RAND_SAFE_PERCENT` | Share of picks in the safe range | `60` |

Tip: with a 2 s change interval and a 5 s reconnect delay, the lights rarely come back. Use `SET RECON 1` for a smooth demo.

### 1.8 Demo checklist

| Test | How | Expected |
| --- | --- | --- |
| Over-voltage | P1 above 83% | Relay off, `OFF:OVER-VOLT`, reconnects after 5 s when safe |
| Under-voltage | P1 below 60% | Relay off, `OFF:UNDER-VOLT` |
| Over-current | `SET OC 5`, P2 to about 35% | Trip after 120 ms |
| Short circuit | P2 above 50% (with OC 5) | Instant trip |
| Settings | SET, +, ACCEPT | `Saved`; `GET` shows the new value |
| PC control | `SET UV 190` | `OK`; invalid values give `ERR` |

## Project info

|  |  |
| --- | --- |
| Microcontroller | ESP32 DevKit (ESP32-WROOM-32) |
| Simulator | Wokwi |
| Language | Arduino C++ |
| Libraries | `LiquidCrystal I2C`, `Wire`, `Preferences` |"# Voltage-Protector-System-" 
