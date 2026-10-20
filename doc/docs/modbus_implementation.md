# Modbus RTU implementation

The ATmega128A runs a Modbus RTU slave at the configured address (**1** by default) and baud (**115200** by default), **8N1** on
USART1. RX is PD2, TX is PD3, and PD5 drives the RS485 direction line (high
to transmit). Timer1 supplies the RTU silence interval. FreeMODBUS is a
submodule under `firmware/DAL_PDU/DAL_PDU/src/dev/M4_MiddleWare/ThirdParty/freemodbus/`.
Clone with `git submodule update --init` before building.

Addresses below use conventional one-based Modbus notation. Subtract one for
masters that expect zero-based request offsets: coil `00002` has offset `1`.
Unsupported addresses return an illegal address exception. Other addresses in
the [design draft](use_cases_commands.md#modbus-rtu-interface) remain reserved.

## Events and clock

The OBC reads an event in two steps. Write `40050` with the sequence number wanted (`0` for the newest). Then read `30041`–`30047`. Poll `30041` alone to see when a new event arrives, since it changes with each event. The clock is set with `40040`–`40041` and read with `30039`–`30040`. Both use the same clock as the console `time` command. Check `30048` for the clock source: `RESTORED` means the time came from the last checkpoint and is approximate until the OBC sets it. The clock is checkpointed to EEPROM every 15 minutes.

## Scheduling

The Modbus port is polled by a 5 ms task, not by the main loop. Each run calls `eMBPoll` twice, so a request is
answered within about one period, which is 5 ms plus the length of the other tasks in that pass. The task also
checks in with the watchdog (200 ms timeout). A request is a single frame, and the master waits for the reply,
so polling every 5 ms is well inside any normal master response timeout. Keep any single task well under the
200 ms watchdog timeout, since a long task delays the Modbus reply.

## Coils (FC 01/05/15)

| Address | Name | Read value and write behavior |
|---:|---|---|
| `00001` | `VOUT_ENABLE` | Actual TPS55287 enable bit. Enabling powers TRANS on first if it is off and requires a setpoint programmed since TPS detection, plus verified internal feedback. An eFuse FLT alone does not block it. Disabling never powers TRANS on. Reads do not clear TPS fault flags. |
| `00002` | `TRANS_BRANCH_ENABLE` | Actual TRANS eFuse output. An eFuse FLT is reported as WARNING and does not reject enable. Disabling TRANS also requests VOUT off. |
| `00003` | `CLEAR_FAULT_LATCH` | Read: 1 while any fault is latched. Write 1 to clear the latch in internal EEPROM. Write 0 does nothing. The latch does not enable TRANS. |
| `00010` | `HEARTBEAT_ENABLE` | LED blinking enable. Applies to the NORMAL pattern only. Fault and warning indications always show. |

Coils `00001` and `00002` can be read or written together. In a two-coil
write, bit 0 is VOUT and bit 1 is TRANS. Enabling VOUT while disabling TRANS
is rejected. Enabling both turns TRANS on first; a failed VOUT enable rolls
back a newly enabled TRANS branch.

## Holding registers (FC 03/06/16)

| Address | Name | Access | Value |
|---:|---|---|---|
| `40001` | `VOUT_SETPOINT_MV` | R/W | Commanded setpoint in mV, read from system data. `0xFFFF` until a setpoint is commanded since reset. Same value as input `30023`. A write powers TRANS on first if it is off and confirms the chip answers, then feedback mode and voltage range determine whether it is accepted. |
| `40020`–`40021` | `LED_ON_MS` | R/W | Unsigned 32-bit milliseconds, high word first. |
| `40022`–`40023` | `LED_OFF_MS` | R/W | Unsigned 32-bit milliseconds, high word first. |
| `40030` | `MODE_REQUEST` | R/W | Read: mode (0 NORMAL, 1 MANUAL, 2 WARNING, 3 ERROR). Write 0 for NORMAL, 1 for MANUAL, or 4 for an immediate retry. A retry succeeds only after regulator and limit checks pass. |
| `40031` | `LIMIT_TEMP` | R/W | Over-temperature limit in tenths of a degree C, signed. Range -400 to 1250. Stored in internal EEPROM. |
| `40032` | `LIMIT_VOUT` | R/W | VOUT overcurrent limit in mA. Range 100 to 5000. Stored in internal EEPROM. |
| `40033` | `LIMIT_TRANS` | R/W | +5_TRANS overcurrent limit in mA. Range 100 to 5000. Stored in internal EEPROM. |
| `40034` | `RETRY_ENABLE` | R/W | 0 disables automatic retry; 1 enables it (default). Stored in internal EEPROM; takes effect at once. |
| `40035` | `RETRY_TIME_S` | R/W | Retry interval in seconds, 1 to 60 (default 5). Stored in internal EEPROM; takes effect at once. |
| `40040`–`40041` | `CLOCK_TIME` | R/W | Clock time, Unix seconds (UTC), high word first. Reads `0xFFFF 0xFFFF` when unset. Write both words to set the clock; the write is logged as a `CLOCK_SET` event. `0xFFFFFFFF` is refused. |
| `40050` | `EVENT_SELECT` | R/W | Sequence number of the event read on `30041`–`30047`. `0` selects the newest event. |
| `40060` | `BOOTLOADER_RESET` | R/W | Read 0 while idle or 1 while reset is pending. Write `0xB007` with FC 06 or a one-register FC 16 request to enter the AVR bootloader. Other values are rejected. The application replies, waits 100 ms, turns VOUT and TRANS off, writes the boot request, and resets by watchdog. |
| `40100`–`40101` | `PRODUCT_CODE` | Read only | ASCII `PDU\0` as `0x5044`, `0x5500`. |
| `40102` | `FW_VERSION` | Read only | Major in high byte, minor in low byte; currently `0x0001`. |
| `40103` | `FW_PATCH` | Read only | Patch version; currently `0`. |
| `40104` | `CPU_MHZ` | Read only | CPU clock in MHz; currently `12`. |
| `40105` | `SLAVE_ADDRESS` | Read only | Active RTU slave address, from the configuration. Set with `config set address`; applies at the next boot. |
| `40106` | `BAUD_RATE_LOW` | Read only | Low 16 bits of the active RTU baud rate. Combine with `40109` for 115200. Set with `config set baud`; applies at the next boot. |
| `40107`–`40108` | `FW_BUILD` | Read only | Build code, high word first. Bit 28 is the dirty flag. Bits 0 to 27 are the short git commit. |
| `40109` | `BAUD_RATE_HIGH` | Read only | High 16 bits of the active RTU baud rate. Compute baud as `40109 * 65536 + 40106`. This is 1 for 115200 and 0 for the lower supported rates. |

Read the seven-word identity block with FC 03 starting at `40100` (request
offset `99`). Writes to that block are rejected. Requests cannot span gaps
between register groups. Both LED periods must be nonzero.

## Other implemented telemetry

| Type | Address | Meaning |
|---|---:|---|
| Discrete input | `10001` | 12V TRANS eFuse fault (U1), PC4. Set while the fault input is low. |
| Discrete input | `10002` | 12V_MCU eFuse fault (U2), PC0. Set while the fault input is low. Reported and latched. Firmware cannot switch the MCU branch off. |
| Discrete input | `10010` | Actual heartbeat LED output state. |
| Discrete input | `10011` | Actual TRANS output state. |
| Input register | `30020` | Status bits: bit 0 LED on, bit 1 TRANS on, bit 2 PC4 fault (12V TRANS), bit 3 PC0 fault (12V_MCU), bit 4 PC4 latched, bit 5 PC0 latched, bit 6 VOUT setpoint mismatch. |
| Input register | `30021` | Board temperature in tenths of a degree Celsius, signed 16-bit (`251` is 25.1 °C). `0x8000` means no valid reading. |
| Input register | `30022` | Operating mode: 0 NORMAL, 1 MANUAL, 2 WARNING, 3 ERROR. See [Mode state machine](mode_state_machine.md). |
| Input register | `30023` | Commanded VOUT setpoint in mV, from the console or Modbus `40001`. `0xFFFF` until a setpoint is commanded since reset. |
| Input register | `30024` | Measured VOUT voltage in mV, PAC1934 channel 1. `0xFFFF` when the PAC1934 is unavailable. |
| Input register | `30025` | Measured VOUT current in mA. Clamped to `0xFFFE`. `0xFFFF` when unavailable. |
| Input register | `30026` | Measured +12 input voltage in mV. |
| Input register | `30027` | Measured +12 input current in mA. |
| Input register | `30028` | Measured +5_TRANS voltage in mV. |
| Input register | `30029` | Measured +5_TRANS current in mA. |
| Input register | `30030` | Measured +5 voltage in mV. |
| Input register | `30031` | Measured +5 current in mA. |
| Input register | `30032` | Boot count since the boot record was first written. Increments once per boot. |
| Input register | `30033` | Cause of the last boot: 1 POWER_ON, 2 BROWN_OUT, 3 EXTERNAL, 4 AVR_WATCHDOG, 5 CLIENT_WATCHDOG, 6 COMMAND, 0 UNKNOWN. |
| Input register | `30034` | Watchdog source when the cause is 5: 1 CONSOLE, 2 HEARTBEAT, 3 MODBUS, 4 POWER_MONITOR, 5 TEMPERATURE, 16 MONITOR. Otherwise 0. |
| Input register | `30035` | Rail power, VOUT, in 10 mW units. `0xFFFF` when the PAC1934 is unavailable. Clamped to `0xFFFE`. |
| Input register | `30036` | Rail power, +12 input, in 10 mW units. Same rules as `30035`. |
| Input register | `30037` | Rail power, +5_TRANS, in 10 mW units. Same rules as `30035`. |
| Input register | `30038` | Rail power, +5, in 10 mW units. Same rules as `30035`. |
| Input register | `30039`–`30040` | Clock time, Unix seconds (UTC), high word first. `0xFFFF 0xFFFF` when unset. |
| Input register | `30041` | Sequence of the selected event. `0` when no stored event matches `40050`. |
| Input register | `30042` | Severity of the selected event: 1 INFO, 2 WARNING. |
| Input register | `30043` | Code of the selected event: 1 TEMP, 2 VOUT_CURRENT, 3 TRANS_CURRENT, 4 I2C_TIMEOUT, 5 CONFIG_CHANGED, 6 SETPOINT_MISMATCH, 7 CLOCK_SET. |
| Input register | `30044` | Mode when the event was logged: 0 NORMAL, 1 MANUAL, 2 WARNING, 3 ERROR. |
| Input register | `30045` | Value of the selected event, signed. Units depend on the code. |
| Input register | `30046`–`30047` | Time of the selected event, Unix seconds, high word first. `0xFFFF 0xFFFF` when unset. |
| Input register | `30048` | Clock source now: 0 UNSET, 1 RESTORED (approximate, from the last EEPROM checkpoint after a reset), 2 SET. |
| Input register | `30049` | Clock source of the selected event, same values. `0` when no event matches. |

The TRANS application checks both fault pins every 6 ms and latches a continuously
active indication after 50 ms. It reports WARNING without switching TRANS off. The callbacks use `SystemData` for shared
values, `AppPower_SetTrans`/`AppPower_EnsureRegulatorOn` for TRANS, and the
TPS55287 BSP for VOUT. Coil `00001` (single write) and holding register
`40001` both call `AppPower_EnsureRegulatorOn` before touching the TPS55287,
so an OBC does not need to write coil `00002` first; the two-coil write at
`00001` still handles VOUT and TRANS together explicitly.

The slave address and baud rate come from the internal EEPROM configuration record and take
effect after a reboot. Retry settings use holding registers `40034` and `40035` and take effect
immediately. Hardware revision and serial number have no defined source in this firmware. PD5
is the direction pin; verify its polarity and RX/TX timing on hardware.

The AVR project compiles upstream `mbrtu.c` through the local
`M2_BSP/Modbus/mbrtu_avr.c` wrapper. That wrapper suppresses one unused
STM32-only CRC temporary warning while keeping the FreeMODBUS submodule clean.

## Design differences

The implemented map is the reference (decided 2026-10-06, OI-14). The design draft in [Use cases](use_cases_commands.md#modbus-rtu-interface) differs as follows. "Not built" means the address is not implemented and is not planned unless the OBC needs it.

| Design address | Design meaning | Decision |
|---|---|---|
| `00004` | `ENTER_BOOTLOADER` | Deferred to the bootloader work (OI-16). |
| `10001` | `EFUSE1_FAULT` (U1 transducer) | Kept as `10001`: PC4, the 12V TRANS fault. |
| `10002` | `EFUSE2_FAULT` (U2 MCU branch) | Kept: PC0, the 12V_MCU fault. |
| `10003` | `OVER_TEMP_FLAG` | Not built. Temperature is in `30021`, and the ERROR mode is in `30022`. |
| `10004` | `COMMS_WATCHDOG_TRIPPED` | Not built. |
| `30001`–`30008` | Rails, V and mA | Kept at `30024`–`30031`. |
| `30009` | Board temperature, °C×10 | Kept at `30021`, tenths of a degree. |
| `30010` | `FW_VERSION` (input) | Kept as holding `40102`. |
| `30011`–`30015` | Active bank, bank status, uptime | Not built (bootloader, OI-16). |
| `30016` | `RESET_REASON_CODE` | Kept as boot information at `30032`–`30034`, with cause 6 COMMAND. |
| `30017` | `SYSTEM_MODE` | Kept at `30022`. |
| `40001` | `VOUT_SETPOINT_MV` | Kept. Also on `30023`. |
| `40002` | `VOUT_CURRENT_LIMIT_MA` | Kept as `40032`. |
| `40003` | `OVER_TEMP_THRESHOLD_C` | Kept as `40031`, in tenths of a degree. |
| `40004` | `COMMS_WATCHDOG_TIMEOUT_S` | Not on Modbus. Set with `config set timeout` from the console. |
| `40005` | `MODBUS_SLAVE_ADDRESS` | Read-only at `40105`. Set with `config set address` from the console. |
| `40006` | `MODBUS_BAUD_CODE` | Read-only at `40109`/`40106`, as a 32-bit rate. Set with `config set baud` from the console. |
