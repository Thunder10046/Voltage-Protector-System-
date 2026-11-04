# ESP32 Voltage & Over-Current Protector

![ESP32 Voltage & Over-Current Protector — project preview](image.png)

An ESP32-based voltage and over-current protection system simulated in Wokwi. The system monitors supply voltage and load current, disconnects the load when a fault is detected, and reconnects it when the supply returns to a safe condition.

## How to run (Wokwi)

1. Open the [ESP32 Voltage & Over-Current Protector project in Wokwi](https://wokwi.com/projects/477393031120734209).
2. Press the **Play** button to start the simulation.
3. Use the potentiometers, buttons, LCD, and Serial Monitor to test the system.

> **Image note:** Keep `image.png` in the same folder as this `README.md` file. GitHub will then display it at the top of the project page.

---

## User manual

### 1.1 Meet the controls

| Control | Where | Use |
| --- | --- | --- |
| **P1** potentiometer | Left | Simulates the supply voltage in POT mode |
| **P2** potentiometer | Left | Simulates the load current |
| **SET** button | GPIO32 | Opens the settings menu or skips the current setting |
| **ACCEPT / REJECT** button | GPIO33 | One click accepts; a double-click rejects |
| **+** button | GPIO25 | Increases a setting value |
| **-** button | GPIO27 | Decreases a setting value |
| **3 LEDs** | Right | Represent the appliances: ON means powered; OFF means disconnected |
| **LCD** | Top | Displays live readings, system status, and menus |

#### Potentiometer guide

| Potentiometer position | Voltage (P1) | Current (P2) |
| --- | ---: | ---: |
| 25% | 75 V | 5 A |
| 50% | 150 V | 10 A |
| 60% | 180 V | 12 A |
| 77% | 230 V | 15 A |
| 83% | 250 V | 17 A |
| 100% | 300 V | 20 A |

### 1.2 The main screen

The LCD's main screen looks similar to this:

```text
V:230.0 I: 5.0
LOAD ON       P
```

| Display item | Meaning |
| --- | --- |
| `V:` / `I:` | Live supply voltage in volts and load current in amps |
| `LOAD ON` | The relay is closed and the appliances are powered |
| `OFF:<fault>` | The relay is open; the text identifies the fault (see [Fault messages](#15-fault-messages)) |
| Last letter `P` | Potentiometer mode |
| Last letter `A` | AUTO random-voltage mode |

On the main screen, **one click on ACCEPT** attempts to reconnect the load after a trip. Reconnection is allowed only when the supply is safe. Otherwise, the LCD displays `Supply not safe`.

### 1.3 Changing settings on the device

1. Press **SET**. The LCD displays `1/6 OVER-VOLT limit`.
2. Use **+** and **-** to change the displayed value.
3. Choose one of the following actions:

| Action | Result |
| --- | --- |
| **ACCEPT** (one click) | Saves the value and moves to the next setting |
| **REJECT** (double-click ACCEPT) | Cancels the settings session and returns to the main screen |
| **SET** | Skips the current setting without changing it |

The device waits approximately **0.4 seconds** after an ACCEPT click to determine whether a second click follows. This is why a single click takes about 0.4 seconds to register.

After the last setting (6/6), the device returns to the main screen.

| Page | Setting | Range | Step | Default |
| --- | --- | ---: | ---: | ---: |
| 1/6 | Over-voltage limit | 100–300 V | 10 V | 250 V |
| 2/6 | Under-voltage limit | 50–250 V | 10 V | 180 V |
| 3/6 | Over-current limit | 1–20 A | 1 A | 10 A |
| 4/6 | Trip delay | 20–1000 ms | 20 ms | 120 ms |
| 5/6 | Auto-reconnect delay | 0–60 s (`0` = manual only) | 1 s | 5 s |
| 6/6 | Supply source | POT / AUTO | Toggle | POT |

**Voltage-limit rule:** The over-voltage (OV) limit must be at least **20 V higher** than the under-voltage (UV) limit. If a change would violate this rule, the LCD displays `Invalid! kept old`, and the previous value is retained.

### 1.4 Changing settings from a computer (Wokwi Serial Monitor)

1. Open the **Serial Monitor** in Wokwi.
2. Set the baud rate to **115200**.
3. Type a command and press **Enter**. Commands are not case-sensitive.

| Command | What it does |
| --- | --- |
| `HELP` | Lists all available commands |
| `STATUS` | Shows live voltage, current, relay state, and fault status |
| `GET` | Shows all current settings |
| `SET OV <volts>` | Sets the over-voltage limit; for example, `SET OV 240` |
| `SET UV <volts>` | Sets the under-voltage limit; for example, `SET UV 190` |
| `SET OC <amps>` | Sets the over-current limit; for example, `SET OC 8` |
| `SET DELAY <ms>` | Sets the trip delay in milliseconds; for example, `SET DELAY 100` |
| `SET RECON <seconds>` | Sets the auto-reconnect delay; `0` means manual reset only |
| `SIM POT` | Uses the P1 potentiometer as the voltage source |
| `SIM AUTO` | Uses randomly generated voltage values |
| `RESET` | Attempts to reconnect the load, but only if the supply is safe |
| `RELAY OFF` | Switches the load off manually; it will not auto-reconnect. Use `RESET` to reconnect |
| `STREAM ON` | Sends `time_ms,V,I,relay` readings every 0.5 seconds for plotting or logging |
| `STREAM OFF` | Stops the data stream |
| `DEFAULTS` | Restores the factory settings |

Successful commands return a response beginning with `OK`. Invalid or out-of-range values return a response beginning with `ERR:`. Changes made through the Serial Monitor are saved immediately, just like changes made on the device.

When a protection trip occurs, the device prints a message similar to:

```text
!! TRIP: OVER-VOLT (V=262.3 I=4.98)
```

### 1.5 Fault messages

| LCD message | Cause | Trip condition |
| --- | --- | --- |
| `OFF:OVER-VOLT` | Supply voltage is above the OV limit | After the configured trip delay (120 ms by default) |
| `OFF:UNDER-VOLT` | Supply voltage is below the UV limit | After the configured trip delay |
| `OFF:OVER-CURR` | Load current is above the OC limit | After the configured trip delay |
| `OFF:SHORT-CKT` | Load current is above **2 ×** the OC limit | Immediately, without the normal trip delay |
| `OFF:MANUAL-OFF` | The `RELAY OFF` command was sent | Manual disconnection; no automatic reconnection |

With the default over-current limit of **10 A**, the short-circuit threshold is above **20 A**. The P2 potentiometer reaches a maximum of 20 A, so it cannot exceed this default threshold. To demonstrate the short-circuit condition, first send `SET OC 5`. The short-circuit threshold then becomes above 10 A.

### 1.6 Reconnecting after a trip

After a protection trip, the system automatically reconnects the load only when **all** of the following conditions remain true for the complete reconnect delay (5 seconds by default):

- Voltage is at least **5 V inside both voltage limits**. In other words, the voltage must be at least `UV + 5 V` and no higher than `OV - 5 V`.
- Current is below **90% of the over-current limit**.

This voltage margin is a form of hysteresis. It helps prevent repeated relay switching when the supply voltage fluctuates close to a protection threshold.

If the reconnect delay is set to `0`, automatic reconnection is disabled. A manually disconnected load (`MANUAL-OFF`) also requires a manual reset. In either case, reconnect using one click of **ACCEPT** on the main screen or the `RESET` Serial Monitor command. The supply must still be safe before reconnection is allowed.

### 1.7 AUTO (random) mode

In AUTO mode, the system generates a random voltage value while the load current continues to come from the P2 potentiometer. Approximately **60%** of generated voltage values are in the safe range, while the remaining **40%** may fall anywhere between 0 and 300 V. This makes it possible to demonstrate protection trips and reconnection without continuously adjusting P1.

Enable AUTO mode by sending `SIM AUTO` in the Serial Monitor or by selecting AUTO on settings page 6/6. The random-voltage update interval is configured near the top of `sketch.ino`:

| Constant | Meaning | Current value |
| --- | --- | --- |
| `RAND_MIN_MS` / `RAND_MAX_MS` | Minimum and maximum time between random voltage updates | Both are currently `2000L` (2 seconds) |
| `RAND_SAFE_PERCENT` | Approximate percentage of generated values in the safe range | `60` |

**Demonstration tip:** With a 2-second update interval and a 5-second reconnect delay, the voltage may change again before the reconnect timer finishes. As a result, the lights may rarely turn back on. Use `SET RECON 1` to make the demonstration smoother.

### 1.8 Demo checklist

| Test | How to perform it | Expected result |
| --- | --- | --- |
| Over-voltage | Move P1 above approximately 83% | Relay opens and LCD displays `OFF:OVER-VOLT`; it reconnects after the configured delay once the supply remains safe |
| Under-voltage | Move P1 below 60% | Relay opens and LCD displays `OFF:UNDER-VOLT` |
| Over-current | Send `SET OC 5`, then move P2 to approximately 35% | Relay opens after the configured trip delay (120 ms by default) |
| Short circuit | Keep `SET OC 5` active and move P2 above 50% | Relay opens immediately |
| Device settings | Press SET, adjust a value with + or -, then press ACCEPT | The value is saved; `GET` shows the updated setting |
| Computer control | Send `SET UV 190` | Serial Monitor returns `OK`; invalid values return `ERR:` |

---

## Project information

| Item | Details |
| --- | --- |
| Microcontroller | ESP32 DevKit (ESP32-WROOM-32) |
| Simulator | [Wokwi](https://wokwi.com/) |
| Programming language | Arduino C++ |
| Libraries | `LiquidCrystal_I2C`, `Wire`, `Preferences` |
| Voltage input | P1 potentiometer (simulates 0–300 V) |
| Current input | P2 potentiometer (simulates 0–20 A) |
| Load indication | Three LEDs controlled through the relay output |

## Protection defaults

| Protection parameter | Default value |
| --- | ---: |
| Over-voltage limit | 250 V |
| Under-voltage limit | 180 V |
| Over-current limit | 10 A |
| Normal trip delay | 120 ms |
| Short-circuit trip threshold | Above 2 × the over-current limit |
| Auto-reconnect delay | 5 s |
| Reconnection voltage margin | 5 V inside each voltage limit |
| Reconnection current condition | Below 90% of the over-current limit |
