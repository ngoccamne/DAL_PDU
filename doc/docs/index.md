# DAL_PDU software design

## Purpose

DAL_PDU is the power distribution unit board of the IGRVT-50 PC104 stack. The current
ATmega128A firmware controls a TRANS eFuse, heartbeat LED, and TPS55287 variable output;
it offers text commands on UART0 and the software console, plus a Modbus RTU register
subset on USART1. The broader design calls for power telemetry, fault logging, and a
dual-bank bootloader; those features are not yet implemented.

Start with [Current firmware architecture](current_firmware.md) for implemented modules
and runtime flow, and [Current command reference](command_reference.md) for commands
available today. The requirements and use-case pages also describe planned features.

## Source hardware evidence

All hardware facts in this documentation set are drawn from the independent hardware review at:

```
D:\PROJECT\LIINSPACE\DAL\REVIEW\review_dal_pdu_docs\docs
```

which covers `SpaceLiinTech_BEE_DAL_PDU_HW_V1.0` (schematic PDF revision 1.1.0, dated 15-Sep-2026; BOM
`DAL_PDU_V1.0.xlsx`). That review's own conclusion is **"Not ready for fabrication or assembly"** — three
blocking issues and several major issues are still open. See
[Hardware overview](hardware_overview.md#known-hardware-issues-that-gate-firmware-bring-up) for the
subset that directly constrains this software design, and treat the full issue register in the review
package as the authority for hardware release status.

## Document map

| Page | Content |
|---|---|
| [Current firmware architecture](current_firmware.md) | Implemented layers, tasks, interfaces, and tests |
| [Current command reference](command_reference.md) | All implemented text commands with request/response examples |
| [Modbus RTU implementation](modbus_implementation.md) | Implemented Modbus configuration and register subset |
| [Hardware overview](hardware_overview.md) | Block diagram, power tree, connector I/O, and the MCU pin/peripheral map firmware has to live with |
| [Software requirements](software_requirements.md) | Functional and non-functional requirements: power control, protection, telemetry, communications, bootloader, watchdog, configuration |
| [Use cases, user stories & commands](use_cases_commands.md) | Design targets and draft Modbus register map; links to the implemented command reference |

## Key assumptions carried through every page

1. Hardware blocker `IF-001` (RS-232 driver/receiver reversed) is corrected before any firmware bring-up. The console is the software UART on PE3/PE4 (`MCU-001`, decided); see
   [Hardware overview](hardware_overview.md#known-hardware-issues-that-gate-firmware-bring-up).
2. The ATmega128A has exactly two hardware USARTs (`USART0` on PE0/PE1, `USART1` on PD2/PD3). All three
   required serial channels (PC104 RS-232, local console, RS-485/Modbus) must be allocated across those
   two USARTs plus one software (bit-banged) UART — see
   [Serial resource allocation](software_requirements.md#41-serial-resource-allocation).
3. The implemented Modbus map and ASCII commands are described on the current-firmware pages. Additional registers and commands in the requirements remain design targets.
