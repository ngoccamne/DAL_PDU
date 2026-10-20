# Current text command reference

These **case-sensitive** text commands run on the software UART console (PE3/PE4) at
9600 baud. Terminate a line with CR, LF, or CRLF; arguments are separated by spaces.
The console echoes input and begins with a `> ` prompt. UART0 (PE0/PE1) carries binary
XTP/XCLI for the [OTA handoff](ota_bootloader.md); it does not echo text. The tables show
**reply text only**, without echo or prompt.

## General and application commands

| Command example | Expected reply | Notes |
|---|---|---|
| `help` | Starts with `Available commands` and `------------------`, followed by all command help lines | Entries are sent as the 256-byte response buffer drains. |
| `version` | `DAL_PDU firmware v0.1.0 (Development) build c7f0bf8-dirty` | Firmware version and build. The build is the short git commit. `-dirty` means the tree had uncommitted changes when it was built. |
| `led get` | `LED on=1000 ms off=1000 ms` | Default after reset. |
| `led set 250 750` | `LED on=250 ms off=750 ms` | Both decimal durations must be nonzero, at most `4294967295` ms. |
| `trans get` | `TRANS off` | Adds ` fault` if the PC4 TRANS eFuse fault input is active. |
| `trans on` | `TRANS on` | Enables PC2, waits 20 ms, then checks the TPS55287. The reply adds ` fault` while PC4 is active. A confirmed eFuse FLT is latched as WARNING and does not turn PC2 off or block a later enable. A regulator failure unrelated to an active TRANS eFuse FLT still raises ERROR. |
| `trans off` | `TRANS off` | Requests TPS55287 VOUT off before disabling TRANS. With an active fault, reply is `TRANS off fault`. |

## I2C maintenance commands

Addresses are 7-bit (`0x08`–`0x77`); register and data bytes are `0x00`–`0xff`.
Numbers accept decimal or a `0x` hexadecimal prefix. The examples below assume
one responding TPS55287 at address `0x75` and its reset register values.

| Command example | Expected reply | Notes |
|---|---|---|
| `i2cdetect 0x74 0x75` | `0x75 ` | Scan inclusive range; the trailing space precedes CRLF. With no devices, the reply is an empty line. Plain `i2cdetect` scans `0x08`–`0x77`. |
| `i2cget 0x75 0x04` | `0x03` | Reads one register byte using a repeated START. Without a register, reads one byte directly. |
| `i2cset 0x75 0x04 0x03` | `I2C write OK` | Writes one register byte. This example preserves the TPS55287 reset feedback setting; raw writes can change power configuration. |

An unresponsive address returns `I2C error` for `i2cget` or `i2cset`.

## 24C04 EEPROM commands

U14 has 512 bytes at logical addresses `0` through `511`. Numbers accept decimal
or `0x` hexadecimal notation. The examples assume a connected chip and successful
I2C transfers.

| Command example | Expected reply | Notes |
|---|---|---|
| `eeprom probe` | `EEPROM present` | Reports `EEPROM unavailable` if the chip does not acknowledge. |
| `eeprom read 0x100 4` | `EEPROM 0x0100: de ad be ef` | Example assumes those four bytes were stored. Omitting the length reads one byte; length must be 1–16. |
| `eeprom write 0x100 deadbeef` | `EEPROM write OK` | Writes four bytes and verifies them by reading back. The hex string must have 2–32 digits (1–16 bytes); an optional `0x` prefix is accepted. |

Writes lower the PA0 write-protect pin only during the operation. The reply is
`EEPROM read failed` or `EEPROM write failed` if the I2C transfer or verification
fails. Out-of-range addresses, reads or writes past byte `511`, and malformed
hex data return `Invalid argument`.

## TPS55287 output commands

U5 is detected at `0x74` or `0x75` according to its MODE pin. The examples assume
`0x75` and no active faults. `tps55287 set` and `tps55287 on` power TRANS on
first if it is off, and confirm the chip answers, instead of requiring a
separate `trans on` beforehand. `tps55287 off` never powers TRANS on just to
turn itself off.

Firmware selects internal feedback on the 20 V scale (register `0x04` = `0x03`) and
verifies it after powering TRANS and before setting a voltage or enabling VOUT. The
accepted setpoint range is 800–20000 mV in 10 mV steps; requests between steps are
rounded to the nearest step. If a raw I2C write changed the feedback mode while VOUT
was on, firmware disables VOUT before restoring internal feedback. A setpoint command
reprograms the reference before restoring VOUT. Output enable requires a setpoint
programmed since the current TPS detection; issue `tps55287 set` first after reset or
TRANS power cycling.
`tps55287 status` reports the mode currently read from the chip.

| Command example | Expected reply | Notes |
|---|---|---|
| `tps55287 status` | `TPS55287 addr=0x75 off internal-feedback` | Appends ` SCP`, ` OCP`, or ` OVP` for set protection flags. Reading status clears those latched flags. Does not power TRANS on; fails if TRANS is off. |
| `tps55287 set 5000` | `TPS55287 set OK` | Powers TRANS on first if it is off, verifies internal feedback, and programs the reference for 5000 mV. The actual output has not been measured by this command. |
| `tps55287 set 20000` | `TPS55287 set OK` | Programs code `0x780`, the top of the supported 20 V command range. |
| `tps55287 get` | `VOUT setpoint 5000 mV` | Converts the reference code using the internal 20 V scale. Refuses readback if the chip is in another feedback mode. Does not power TRANS on. |
| `tps55287 on` | `TPS55287 on` | Powers TRANS on first if it is off and verifies internal feedback before enabling VOUT. A setpoint must have been programmed since TPS detection. An eFuse FLT does not cause a software shutdown. |
| `tps55287 off` | `TPS55287 off` | Clears the chip's output-enable bit. Never powers TRANS on. |

`tps55287 set` and `tps55287 on` report `TPS55287 set failed (TRANS)` /
`TPS55287 enable failed (TRANS)` if TRANS was off and powering it on failed
(the regulator did not respond while the TRANS eFuse FLT was inactive, or internal feedback
could not be verified — see
[Mode state machine](mode_state_machine.md#3-regulator-monitor)). With TRANS
already on, `tps55287 set` rejects values below 800 mV and above 20 V,
reporting `TPS55287 set failed (I2C or range)` for an
I2C failure or an out-of-range request. `tps55287 on` reports
`TPS55287 enable failed (setpoint, I2C or TRANS)` if no setpoint was programmed
since detection, or if I2C fails after TRANS is on. A failed `get` or `status` reports
`TPS55287 read failed (I2C)`.

## PAC1934 rail monitor

The one-second background task reads U10 through the shared I2C bus. The board
uses 10 mΩ shunts on all four channels. The example below assumes the device
was detected at `0x1f` and all channels measure 5 V, 1 A, and 5 W.

| Command example | Expected reply | Notes |
|---|---|---|
| `pac1934 get` | `PAC1934 addr=0x1f` followed by `VOUT 5000 mV 1000 mA 5000 mW`, `+12 ...`, `+5_TRANS ...`, and `+5 ...` | Shows the latest cached sample; plain `pac1934` and `pac1934 status` are aliases. |
| `pac1934 refresh` | Same format as `get` | Takes a new measurement immediately; normal polling remains every 1000 ms. |

Before the first successful poll, or after a failed I2C read, the reply is
`PAC1934 unavailable`. Rail labels follow channel order 1–4 in the board design;
confirm the physical wiring and installed 10 mΩ shunts during bring-up; the
hardware review found a shunt-value mismatch in the BOM.

## Board temperature

The one-second temperature task samples the TC1047A on the TEMP net (PF1/ADC1), averaging
16 conversions. The reply is in degrees Celsius with one decimal place.

| Command example | Expected reply | Notes |
|---|---|---|
| `temp` | `TEMP 25.1 C` | Shows the latest cached sample. `temp get` is an alias. |
| `temp refresh` | `TEMP -0.2 C` | Takes a new sample immediately; normal sampling remains every 1000 ms. |

If the ADC fails or the reading is outside the TC1047A range of −40 °C to +125 °C, the reply is
`TEMP unavailable`. The same value is available over Modbus as input register `30021`.

## Operating mode

The mode is NORMAL, MANUAL, WARNING, or ERROR. See [Mode state machine](mode_state_machine.md).

| Command example | Expected reply | Notes |
|---|---|---|
| `mode` | `MODE normal` and `CAUSES regulator=0 pac1934=0 eeprom=0 latch=0 limit=0 setpoint=0 bus=0 link=0` | `mode get` is the same. A 1 in CAUSES means that cause is active. `link` is Modbus link loss (no valid request for the configured timeout, default 5 s); it only raises WARNING and does not turn TRANS/VOUT off by itself. |
| `mode manual` | `MODE manual` | Takes operator control even when an ERROR cause is set. A confirmed eFuse fault remains latched and reported. |
| `mode normal` | `MODE normal` | Checks the regulator and limits if either has raised ERROR, clears healthy causes, then leaves MANUAL. A latched eFuse WARNING does not block it. |
| `mode retry` | `MODE retry OK`, then current `MODE` and `CAUSES` | Immediately checks a regulator/limit ERROR and attempts to restore TRANS and the previously enabled VOUT. A failed check prints `MODE retry failed`. It does not leave MANUAL. |

## Fault latch

A confirmed eFuse fault is latched in the ATmega128A internal EEPROM, so it survives a reset.
It raises WARNING without a software shutdown of VOUT or TRANS. The latch does not block
`trans on` or `mode normal`; it is available over Modbus coil `00003`.

| Command example | Expected reply | Notes |
|---|---|---|
| `fault get` | `FAULT latch pc4=0 pc0=0 active pc4=0 pc0=0` | `latch` shows the stored bits. `active` shows the live eFuse inputs. `pc4` is the 12V TRANS fault and `pc0` is the 12V_MCU fault. Each is latched, and clears only when its input is inactive. |
| `fault clear` | `FAULT cleared` | Clears the eFuse latch and, after a successful regulator and limit check, any stored regulator or limit ERROR. Refused while an eFuse input or limit remains active, or if the regulator does not respond. It does not turn outputs back on or leave MANUAL. |
| `fault clear all` | `FAULT clear all complete` | Refreshes PAC1934, setpoint, I²C, and EEPROM diagnostics; retries recovered regulator/limit errors; clears inactive eFuse latches; and restarts the Modbus link timeout. If any cause remains, replies `FAULT clear all incomplete; run mode get and fault get`. It does not turn outputs on or leave MANUAL. |

A damaged EEPROM record is treated as both faults latched and reported as WARNING until cleared.
After an overcurrent, use `mode get` to inspect `limit` and `fault get` to inspect the live
PC4/PC0 inputs. If a live eFuse input remains active, remove the overload and restore the
hardware branch before retrying `fault clear`; software does not override an active FLT pin.
`fault clear all` does not hide an active hardware or limit fault. A link warning returns if no
valid Modbus request arrives before the timeout expires again. Modbus coil `00003` still clears
only the eFuse latch.

## Limits and events

Over-temperature and overcurrent are checked every second in every mode. In NORMAL, a trip turns
VOUT and TRANS off and sets the limit ERROR. In every mode, each trip is logged once as a WARNING
event in the 24C04. With retry enabled, firmware waits the configured interval after ERROR, then probes the regulator and restores power when the limits have recovered. Failed attempts repeat at that interval. `mode retry` requests an immediate attempt. The ERROR is stored in internal EEPROM, so it survives a reset until a healthy retry or clear succeeds.

| Command example | Expected reply | Notes |
|---|---|---|
| `limit` | `LIMIT temp=850 vout=1000 trans=1000` | `limit get` is the same. Temperature is in tenths of a degree C, so 850 is 85.0 C. Currents are in mA. |
| `limit temp 900` | `LIMIT temp=900 vout=1000 trans=1000` | Range -400 to 1250. Stored in internal EEPROM and used at the next check. |
| `limit vout 3500` | `LIMIT temp=900 vout=3500 trans=1000` | Range 100 to 5000 mA. `limit trans` sets the +5_TRANS limit the same way. |
| `limit vout 70000` | `LIMIT refused: out of range` | Nothing changes. |
| `event get` | `EVENT seq=1 sev=WARNING code=TEMP mode=NORMAL value=900 time=unset source=UNSET` | Lists the 24C04 ring, 32 slots. Codes are TEMP, VOUT_CURRENT, TRANS_CURRENT, CONFIG_CHANGED, SETPOINT_MISMATCH, and CLOCK_SET. Value is tenths of a degree C, mA, or mV, as the code says. `time` is UTC (`YYYY-MM-DDTHH:MM:SSZ`), or `unset`. `source` is where that time came from: `UNSET`, `RESTORED`, or `SET`. `EVENT none` when empty. |

## Configuration

The Modbus slave address and baud rate are read at boot. Modbus `40105` shows the active address; `40109` and `40106` are the high and low words of the active baud rate. Communications timeout and retry settings take effect at once. Each change is logged once as an INFO event (`CONFIG_CHANGED`). Existing EEPROM configuration records retain their stored baud, including 9600; use `config set baud 115200` and reboot to update an existing board.

| Command example | Expected reply | Notes |
|---|---|---|
| `config` | `CONFIG address=1 baud=115200 timeout=5 retry_enable=1 retry_time=5 source=DEFAULT` | `config get` is the same. `source=DEFAULT` means no valid record is stored; `source=EEPROM` means the stored record is in use. |
| `config set address 7` | `CONFIG address=7 baud=115200 timeout=5 retry_enable=1 retry_time=5 source=EEPROM`, then `Address and baud take effect after reboot` | Range 1 to 247. Takes effect at the next boot. |
| `config set baud 19200` | `CONFIG address=7 baud=19200 timeout=5 retry_enable=1 retry_time=5 source=EEPROM`, then the reboot line | Supported: 1200, 2400, 4800, 9600, 19200, 38400, 115200. Takes effect at the next boot. |
| `config set timeout 10` | Updated `CONFIG` line | Modbus link warning timeout, 1 to 60 s; does not turn outputs off. Takes effect at once. |
| `config set retry_enable 0` | Updated `CONFIG` line | Disables automatic retries; `mode retry` remains available. Default is 1. |
| `config set retry_time 10` | Updated `CONFIG` line | Retry interval, 1 to 60 s. Default is 5 s. |
| `config set address 0` | `CONFIG refused: out of range` | Nothing changes. |
| `config set baud 115200` | Updated `CONFIG` line, then the reboot line | Selects 115200 on the next boot. |
| `config_reset confirm` | `CONFIG reset to defaults`, the `CONFIG` line, `LIMIT temp=850 vout=1000 trans=1000` | Restores the configuration and the limits to the defaults and stores them. The reboot line appears only if the address or baud changed. |
| `config_reset` | Invalid argument reply | Without `confirm` nothing changes. |

`config_reset` does not clear the event log, the boot history, the fault latch, or a limit ERROR. A healthy automatic or manual retry, `fault clear`, or `mode normal` clears a limit ERROR.

## Input errors

The console prints `Unknown command`, `Too few arguments`, `Too many arguments`,
or `Invalid argument` for parser/handler errors. For example, `led set 0 500`
returns `Invalid argument`. Input lines longer than 63 characters return
`Line too long`. A command response that fills the shared output buffer ends
with `[output truncated]`.

The larger proposed command set and power/telemetry requirements remain in
[Use cases, user stories & commands](use_cases_commands.md). The currently
implemented Modbus register map is separate from these text commands.

## Telemetry

| Command example | Expected reply | Notes |
|---|---|---|
| `telemetry` | `SETPOINT 5000 mV`, then one line per rail | `SETPOINT unset` until a setpoint is commanded since reset. Each rail line is `VOUT`, `+12`, `+5_TRANS`, or `+5` with measured mV, mA, and mW. `PAC1934 unavailable` when the monitor cannot be read. |

## Setpoint

`setpoint get` reports the commanded VOUT setpoint held in system data. It is the value last set by the
console or by Modbus `40001`, and it does not read the chip. `tps55287 get` still reads the TPS55287 register.

| Command example | Expected reply | Notes |
|---|---|---|
| `setpoint get` | `SETPOINT 5000 mV` | `SETPOINT unset` until a setpoint is commanded since reset. |

## Boot information

Each boot adds one to the boot count, which is kept in internal EEPROM. The cause comes from the reset
flags read at startup. A watchdog fault is recorded before the TPL5010 resets the unit, and it is written to
the boot record at the next boot.

| Command example | Expected reply | Notes |
|---|---|---|
| `bootinfo` | `BOOT count=2 cause=CLIENT_WATCHDOG source=MODBUS` | `bootinfo get` is the same. Causes are POWER_ON, BROWN_OUT, EXTERNAL, AVR_WATCHDOG, CLIENT_WATCHDOG, COMMAND (from `reset confirm`), and UNKNOWN. Sources are CONSOLE, HEARTBEAT, MODBUS, POWER_MONITOR, TEMPERATURE, and MONITOR. `source=-` unless the cause is CLIENT_WATCHDOG. |

`bootinfo history` lists the last 10 boots, newest first, from internal EEPROM.

The same boot count, cause, and source are on Modbus input registers `30032`–`30034`.

## Clock

A software clock keeps UTC from the last set and the scheduler tick. It has no battery. Every 15 minutes, and on each set, it stores a checkpoint in internal EEPROM. After a reset it restores the last checkpoint with source `RESTORED`, which is approximate, since the power-off time is unknown. A set from the console or Modbus gives source `SET`. Without a checkpoint the clock is `UNSET`. Events carry the clock time and its source.

| Command example | Expected reply | Notes |
|---|---|---|
| `time` | `TIME 2026-10-06T12:34:56Z source=SET` or `TIME unset source=UNSET` | `time get` is the same. UTC. The source is `UNSET`, `RESTORED`, or `SET`. |
| `time set 1791290096` | `TIME 2026-10-06T12:34:56Z source=SET` | Sets the clock from Unix seconds. Logged as a `CLOCK_SET` event. |
| `time set 4294967295` | `TIME refused: reserved value` | The value means unset. Nothing changes. |
| `time set 17x` | Invalid argument reply | Nothing changes. |

The clock uses the MCU clock. It has no temperature compensation, so it drifts with the crystal. The OBC should set it periodically. Modbus reads and sets the same clock: `40040`–`40041` to set and read, and `30039`–`30040` to read.

## Status and reset

| Command example | Expected reply | Notes |
|---|---|---|
| `status` | `STATUS`, then the replies of `mode`, `temp`, `telemetry`, `fault get`, and `bootinfo`, one after another | Read-only summary for the technician. Each section has the same format as its own command. |
| `reset confirm` | `RESET outputs off, restarting` | Turns VOUT and TRANS off, sends the reply, then restarts the MCU through the watchdog after about 0.1 s. The next boot reports `cause=COMMAND`. Without `confirm` nothing happens. |
| `reset` | Invalid argument reply | Needs `confirm`. |
| `persist_reset internal confirm` | `PERSIST_RESET internal EEPROM erased, restarting` | Turns VOUT and TRANS off, blanks application EEPROM records (fault latch, limit error, limits, boot history, config, clock checkpoint), preserves bootloader bytes `0xF80`–`0xFC1`, then restarts to load defaults. |
| `persist_reset external confirm` | `PERSIST_RESET external EEPROM erased` | Blanks the whole U14 24C04 (the event log ring). Nothing is cached from it at boot, so this does not restart the MCU or touch the outputs. A failed erase (chip not acknowledging) replies `PERSIST_RESET external EEPROM failed`. |
| `persist_reset internal`, `persist_reset external`, `persist_reset` | Invalid or too-few-arguments reply | Each needs its scope (`internal` or `external`) and `confirm`. |

`persist_reset` is the only command that returns EEPROM to the state of a never-written chip; `fault clear`, `mode normal`, and `config_reset confirm` each clear a narrower, specific piece of internal EEPROM state (see [Mode state machine §8](mode_state_machine.md#8-known-limits) and the [configuration section](#configuration) above).

## Design command names

The firmware names are the reference. The design names below are dropped (OI-13).

| Design name | Use instead |
|---|---|
| `STAT` | `status` |
| `FAULT?` | `fault get` |
| `FAULT CLR` | `fault clear` |
| `LOG?` | `event get` |
| `TEMP?` | `temp get` |
| `ILIM` | `limit`, `limit vout`, `limit trans` |
| `RESET` | `reset confirm` |
