# Current firmware architecture

This is the short overview of the DAL_PDU firmware. The detail lives in two pages:

- [Code architecture](code_architecture.md): layers, dependencies, data flows, task inventory, shared state.
- [Scheduler design](scheduler_design.md): the task model, API, timing, concurrency rules, and limits.

## Target and entry point

The Atmel Studio project is `PDU/firmware/DAL_PDU/DAL_PDU/` and targets the ATmega128A at 12 MHz.
`main.c` calls `APP_Main()` in `src/dev/M0_App/app_main.c`, which initializes everything and then runs
the main loop, which only dispatches `SCH_HandleScheduledTask()`. Modbus runs as a 5 ms task.

## Layers

| Layer | Responsibility |
|---|---|
| `M0_App` | Startup, console, heartbeat LED, TRANS and fault handling, rail monitoring, watchdog supervision, Modbus callbacks. |
| `M1_Sys` | `Scheduler` (1 ms tick and task dispatch), `CommandLine` (parser and handlers), `SystemData` (shared status). |
| `M2_BSP` | Board pin map, UART ownership, timer ISR, I2C, power GPIO, watchdog, Modbus port, chip bindings. |
| `M3_Driver` | Component drivers (UART, software UART, I2C, GPIO) and chip drivers (TPS55287, PAC1934, 24C04). |
| `M4_MiddleWare` | Byte FIFO, printf hook, FreeMODBUS (submodule). |

## Scheduled work

Nine tasks run from the main loop, driven by a 1 ms Timer0 tick: console (10 ms), heartbeat (20 ms),
TRANS fault check (6 ms), Modbus (5 ms), rail monitor (1000 ms), retry (100 ms), watchdog (10 ms),
board temperature (1000 ms), and clock (1000 ms). The full inventory, with phases
and what each task reads and writes, is in [Code architecture §4](code_architecture.md#4-task-inventory).

## Interfaces

- **UART0 (PE0/PE1)** runs 115200 baud XTP mode `0xA2` with XCLI `ping 0` and `reboot 0` for
  `xbld_client`. **The software console (PE3/PE4)** retains the 9600 baud text commands.
  See [Current command reference](command_reference.md) and [OTA handoff](ota_bootloader.md).
- **Modbus RTU** runs as the configured slave (default 1) at the configured baud (default 115200) on USART1 (PD2/PD3, RS485 direction on PD5). Stored baud settings remain in effect until changed or reset. See
  [Modbus RTU implementation](modbus_implementation.md).
- **I2C** on PD0/PD1 serves the TPS55287 (VOUT), the PAC1934 monitor, and the 24C04 EEPROM.

## Power path

TRANS is switched by the active-low PC2 eFuse. After enabling TRANS, firmware waits 20 ms before
probing the TPS55287. A confirmed eFuse FLT raises WARNING without a software output shutdown. The
fault stays latched in internal EEPROM until cleared by command. VOUT enable still requires TPS
communication and verified internal feedback on the 20 V scale. The PAC1934 task records voltage, current,
and power for VOUT, +12, +5_TRANS, and +5. The application checks setpoint readback, board
temperature, and VOUT and +5_TRANS current limits.

The mode (NORMAL, MANUAL, WARNING, or ERROR) gates TRANS. A missing regulator, or a limit trip in NORMAL, is ERROR. An unreadable PAC1934 or a latched eFuse fault is WARNING. Outside MANUAL, a regulator or limit ERROR turns both outputs off. Automatic retry is enabled by default and attempts recovery every 5 s; the interval is configurable from 1 to 60 s. A successful retry restores TRANS and a VOUT request saved before shutdown. See [Mode state machine](mode_state_machine.md).

## Build and tests

From the PDU repository root, run `make original` for the raw firmware or `make signed` for the
bootloader package. Open `firmware/DAL_PDU/DAL_PDU.atsln` in Microchip Studio 7 for an IDE build.
Run `make -C tests test` for the host unit tests. See [Build and package](firmware_build.md) for
toolchain prerequisites and output paths. Host tests do not replace bench checks of serial
timing, output voltage, or fault wiring.

## Versioning and release

The version and the build identity are described on the [Versioning](code_implementation/versioning.md) page under Code Implementation.
