# Use cases, user stories & commands

This page records the wider design targets. For implemented behavior, see
[Current firmware architecture](current_firmware.md),
[Current text command reference](command_reference.md), and
[Modbus RTU implementation](modbus_implementation.md).

## Actors

| Actor | Description |
|---|---|
| **OBC** | On-Board Computer; Modbus master over RS-485, and the sole client of the PC104 UART for supplementary commands and firmware updates |
| **Ground/bench technician** | Human operator on the local console (`P2`) during integration, test, or troubleshooting |
| **DAL_PDU firmware** | The system under design; Modbus slave, PC104 UART/console command handler, bootloader host |

## Use cases

| ID | Use case | Primary actor | Summary |
|---|---|---|---|
| `UC-1` | Set variable output voltage | OBC | OBC writes a `VOUT` setpoint over Modbus; PDU applies it via U5 and confirms via telemetry readback |
| `UC-2` | Enable/disable transducer branch | OBC | OBC toggles the `U1` eFuse branch that feeds `VOUT` and `+5_TRANS` loads |
| `UC-3` | Read telemetry | OBC | OBC polls voltage/current on all four monitored rails plus board temperature |
| `UC-4` | Handle a protection fault | DAL_PDU firmware | An eFuse fault latches and raises WARNING without a software output shutdown. Overtemperature or overcurrent in NORMAL turns outputs off and enters ERROR. OBC observes and clears the latch. |
| `UC-5` | Update firmware | OBC | OBC pushes a new image to the inactive bank over the PC104 UART and activates it, with automatic rollback if it fails to confirm |
| `UC-6` | Local diagnostics | Ground/bench technician | Technician queries status, forces states, and inspects the event log from the console during bring-up/test |
| `UC-7` | Recover from a hang | DAL_PDU firmware | TPL5010 watchdog resets the MCU if the control loop stops servicing it; reset reason is recorded and reported |
| `UC-8` | Power-up sequencing | DAL_PDU firmware | On every reset, firmware runs self-test and starts in `Standby` with `VOUT` and the transducer branch disabled until commanded |

## User stories

- As the **OBC**, I want to set the `VOUT` setpoint and read back the actual output voltage/current, so
  that I can operate the DAL_PDU as a closed-loop variable power supply for downstream transducers.
- As the **OBC**, I want the transducer branch and `VOUT` to default to disabled after any reset, so that
  a PDU reboot never unexpectedly re-energizes a load.
- As the **OBC**, I want a fault (eFuse trip, overtemperature, overcurrent) to latch and be reported
  rather than silently retried, so that I can decide when it is safe to re-enable power.
- As the **OBC**, I want to update PDU firmware without a risk of bricking the board, so that a bad image
  cannot strand the mission with unswitchable power.
- As the **OBC**, I want the PDU to fail safe (disable `VOUT`) if it stops hearing valid Modbus traffic,
  so that a lost or hung communications link cannot leave an output energized unsupervised.
- As a **ground/bench technician**, I want a local console with human-readable status and the same
  command set the OBC uses, plus a few maintenance-only commands, so that I can bring up and troubleshoot
  a unit without needing the OBC in the loop.
- As a **ground/bench technician**, I want to read the event log and reset-reason code, so that I can
  diagnose an intermittent watchdog reset or fault after the fact.

## Modbus RTU interface

> **Superseded.** This is the original design draft. The implemented map is in [Modbus RTU implementation](modbus_implementation.md). Several addresses below differ from the firmware, and some are not implemented. Decided 2026-10-06: the implemented map is the reference. The differences are in [Design differences](modbus_implementation.md#design-differences).

The currently implemented LED and TRANS register subset is documented in
[Modbus RTU implementation](modbus_implementation.md). The map below is the wider design draft.

Primary OBC control/telemetry channel, over RS-485 (`H1-37`/`H1-38`, `SRS-COM-002`/`003`). Slave address
and baud rate are configurable (`SRS-CFG-060`) and take effect after reboot. All values below are the
proposed default register map — draft, pending firmware sign-off (see [Index](index.md)).

**Supported function codes:** `01` Read Coils, `02` Read Discrete Inputs, `03` Read Holding Registers,
`04` Read Input Registers, `05` Write Single Coil, `06` Write Single Holding Register, `15` Write Multiple
Coils, `16` Write Multiple Holding Registers.

### Coils (read/write, FC 01/05/15)

| Address | Name | Description |
|---:|---|---|
| `00001` | `VOUT_ENABLE` | Enable/disable the `VOUT` rail (`UC-1`) |
| `00002` | `TRANS_BRANCH_ENABLE` | Enable/disable the `U1` transducer eFuse branch (`UC-2`) |
| `00003` | `CLEAR_FAULT_LATCH` | Clear a latched eFuse fault (`UC-4`). Refused while a fault input is active. |
| `00004` | `ENTER_BOOTLOADER` | Pulse to request bootloader entry (`UC-5`); forces `VOUT`/branch off first per `SRS-BOOT-076` |

### Discrete inputs (read-only, FC 02)

| Address | Name | Description |
|---:|---|---|
| `10001` | `EFUSE1_FAULT` | Transducer branch (`U1`) fault flag |
| `10002` | `EFUSE2_FAULT` | MCU branch (`U2`) fault flag — reportable only, not remediable (`SRS-PROT-024`) |
| `10003` | `OVER_TEMP_FLAG` | Board over-temperature flag |
| `10004` | `COMMS_WATCHDOG_TRIPPED` | Set if `VOUT` was auto-disabled by `SRS-COM-045` link-loss protection |

### Input registers (read-only, FC 04)

| Address | Name | Units | Description |
|---:|---|---|---|
| `30001` | `VOUT_VOLTAGE_MV` | mV | Measured `VOUT`, PAC1934 channel 1 |
| `30002` | `VOUT_CURRENT_MA` | mA | Measured `VOUT` current |
| `30003` | `VIN_VOLTAGE_MV` | mV | Measured `+12` input |
| `30004` | `VIN_CURRENT_MA` | mA | Measured input current (`R51` shunt) |
| `30005` | `V5TRANS_VOLTAGE_MV` | mV | Measured `+5_TRANS` |
| `30006` | `V5TRANS_CURRENT_MA` | mA | Measured `+5_TRANS` current |
| `30007` | `V5_VOLTAGE_MV` | mV | Measured `+5` logic rail |
| `30008` | `V5_CURRENT_MA` | mA | Measured `+5` logic rail current |
| `30009` | `BOARD_TEMP_C_X10` | °C ×10, signed | Board temperature |
| `30010` | `FW_VERSION` | packed major/minor | Active bank's firmware version |
| `30011` | `ACTIVE_BANK` | 0=A, 1=B | Currently running flash bank |
| `30012` | `BANK_A_STATUS` | bitfield: valid, confirmed | Bank A header status |
| `30013` | `BANK_B_STATUS` | bitfield: valid, confirmed | Bank B header status |
| `30014`–`30015` | `UPTIME_SECONDS` | seconds, 32-bit | Time since last reset |
| `30016` | `RESET_REASON_CODE` | enum | Power-on / external / watchdog / command (`SRS-WDT-081`) |
| `30017` | `SYSTEM_MODE` | enum | Current mode per the Section 3 state machine in [Software requirements](software_requirements.md#3-system-modes) |

### Holding registers (read/write, FC 03/06/16)

| Address | Name | Units | Description |
|---:|---|---|---|
| `40001` | `VOUT_SETPOINT_MV` | mV | Commanded `VOUT` (`UC-1`); rejected if out of calibrated range (`SRS-PWR-013`) |
| `40002` | `VOUT_CURRENT_LIMIT_MA` | mA | Firmware overcurrent threshold (`SRS-PROT-022`) |
| `40003` | `OVER_TEMP_THRESHOLD_C` | °C | Over-temperature protection threshold (`SRS-CFG-060`) |
| `40004` | `COMMS_WATCHDOG_TIMEOUT_S` | s | Link-loss timeout for `SRS-COM-045` |
| `40005` | `MODBUS_SLAVE_ADDRESS` | — | Effective after reboot |
| `40006` | `MODBUS_BAUD_CODE` | enum | Effective after reboot |

## PC104 UART and console command reference

> **Superseded.** This is the original design draft. The implemented command set is in [Current command reference](command_reference.md). Most commands listed below are not implemented, and the firmware uses different names.

The implemented ASCII commands, example requests, and exact reply text are listed in
[Current text command reference](command_reference.md). UART0 and the local software
UART currently accept the same command table. The proposed additional commands in
the software requirements are not implemented yet.

## Implementation status

Status as of 2026-10-05. Legend as in [Software requirements §7](software_requirements.md#7-implementation-status).

| Item | Status | Note |
|---|---|---|
| `UC-1` Set variable output voltage | Implemented | Set and read over Modbus 40001 (commanded setpoint, from system data). Measured VOUT is on input 30024. A readback mismatch is flagged (WARNING, SRS-PWR-014). |
| `UC-2` Enable/disable transducer branch | Implemented | Modbus coil 00002 and `trans on`/`trans off`. |
| `UC-3` Read telemetry | Implemented | The setpoint, the four rails' voltage and current, and the four rails' power are on Modbus `30023`–`30038` and in the `telemetry` command. Power is in 10 mW units. Board temperature is available through `temp` and Modbus input `30021`. |
| `UC-4` Handle a protection fault | Implemented | Confirmed eFuse faults latch and raise WARNING without a software output shutdown. Over-temperature and overcurrent in NORMAL turn outputs off and enter ERROR. Limits are set with `limit` or Modbus `40031`–`40033`. Automatic retry defaults to enabled every 5 s and restores the prior output request after a healthy check; `mode retry` requests one immediately. An eFuse latch still requires `fault clear`. Thresholds need bench confirmation. |
| `UC-5` Update firmware | Pending | No bootloader. |
| `UC-6` Local diagnostics | Partial | `status`, the time-stamped event log, boot history, and I2C/EEPROM tools exist. No state forcing. |
| `UC-7` Recover from a hang | Partial | TPL5010 reset works. The cause and boot count are recorded in internal EEPROM and shown by `bootinfo`. |
| `UC-8` Power-up sequencing | Implemented | Self-test runs at startup. Starts in NORMAL with both outputs off. A missing regulator is ERROR. |

| User story | Status |
|---|---|
| OBC sets VOUT and reads back voltage and current | Implemented. VOUT voltage and current are on Modbus `30024`–`30025`, and VOUT power on `30035`. |
| Transducer branch and VOUT default to disabled after reset | Implemented for TRANS. VOUT follows TRANS. |
| eFuse fault latches and is reported | Implemented. The eFuse warning is not cleared by automatic retry; use `fault clear` after the input becomes inactive. |
| Firmware update without bricking | Pending |
| Modbus traffic loss reported | Implemented. A configured timeout (default 5 s) raises the `link` WARNING cause after at least one valid request. It does not turn outputs off. |
| Technician console with the same commands plus maintenance | Partial |
| Reset-reason code | Implemented. Shown by `bootinfo`. |
| Event log | Partial. Limit, I2C, setpoint, configuration, and clock events, time-stamped and readable over Modbus. Faults, modes, and resets are not logged yet. |

## Superseded sections

The Modbus register map and the command reference below are the **original design draft**. They are kept as the design record. For what the firmware implements, use:

- [Modbus RTU implementation](modbus_implementation.md)
- [Current command reference](command_reference.md)
