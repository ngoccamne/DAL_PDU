# Unit tests

The firmware is tested on the host, with a PC C compiler. The tests build the production source files for the modules they cover, and replace the hardware with fakes. They do not run on the ATmega128A, so timing, interrupt load, and electrical behaviour still need a bench check.

All test programs are in `tests/unit/`. The fakes for the AVR headers are in `tests/support/`, and the test build is `tests/Makefile`.

## How to run

### Prerequisites

- GNU Make and a host C compiler. The project uses MSYS2 `gcc` and GnuWin32 `make`.
- Python 3, on the PATH. The build runs `tools/gen_build_id.py` before it compiles anything.
- Compiler flags: `-std=gnu99 -Wall -Wextra -Werror`. A warning fails the build.

### Run every test

From the PDU folder:

```
make -C tests test
```

The first step writes `build_id.h`. Each test program then builds and runs in turn. A passing program prints one line:

```
scheduler tests passed
...
clock tests passed
```

A failing check prints the file, line, and expression, then the build stops:

```
unit/test_app_clock.c:126: strcmp(console, "TIME unset source=UNSET\r\n") == 0
make: *** [test] Error 1
```

### Run one test

```
cd tests
make test_app_clock.exe
./test_app_clock.exe
```

Run `make <name>.exe` first, then the executable. Each target is listed in the Makefile.

### Remove the executables

```
make -C tests clean
```

### PATH on Windows

Atmel Studio and GnuWin32 both put a `gcc` or `make` on the PATH that may not match the host toolchain. Run the tests with a clean PATH:

```
export PATH=/c/msys64/mingw64/bin:/usr/bin:/c/Windows/System32
"/c/Program Files (x86)/GnuWin32/bin/make.exe" test
```

## The test programs

The table lists each program, the module it tests, and what it covers. The count is the number of test functions in the file, where the file uses them. Some files run one sequence of checks from `main`.

### Scheduler and FIFO

| Program | Tests | Covers |
|---|---:|---|
| `test_scheduler` | 9 | The per-task period scheduler: tick, timers, task lifecycle, stop and resume, period changes, capacity, and coalescing of pending runs. Built with the normal configuration. |
| `test_scheduler_semaphore` | (same file) | The same checks, built with `USE_SCH_SEMAPHORE`. |
| `test_fifo` | 4 | The byte FIFO: caller storage, several capacities, full and empty, wraparound, clear, and independent instances. |

### Commands and the console

| Program | Tests | Covers |
|---|---:|---|
| `test_command_line` | 7 | The command parser: argument splitting, the argument limit, unknown commands, `help`, `version` (with the build), LED timing, I2C argument checks, and handler error codes. |
| `test_command_eeprom` | main | The `eeprom` command: probe, read, and write, with argument checks. |
| `test_command_system` | 8 | The `status`, `reset`, and `persist_reset` commands. `persist_reset internal` and `persist_reset external` are separate: internal needs a restart (outputs off first) and external does not, and a failed external erase is reported without restarting. Each needs its scope and `confirm`, and the reply text is exact. |
| `test_app_main` | main | Startup and the whole firmware loop, against the real `app_main.c`. Covers UART0 and console pins and settings, the TWI prescaler, the Timer0 tick, task periods, command routing, heartbeat, watchdog, Modbus, and eFuse WARNING without software TRANS shutdown. The simulated I2C bus persists the TPS55287 feedback register for internal-mode verification. |

### Hardware drivers and board support

| Program | Tests | Covers |
|---|---:|---|
| `test_uart_drivers` | 7 | USART0, the software UART, the console UART, and board-level UART dispatch. Uses fake AVR registers and calls the interrupt handlers directly. |
| `test_do` | 2 | The discrete output API on ports A, B, and G. |
| `test_i2c_bus` | 4 | I2C ACK and NACK handling, timeouts, bus recovery, and the fault address reported to the power logic. |
| `test_eeprom24c04` | main | The 24C04 EEPROM driver: reads, writes, pages, and write-protect. |
| `test_tps55287` | main | The TPS55287 regulator: dual address detection, 800–20000 mV reference conversion, internal-feedback verification, safe correction of a changed feedback register, and the TRANS interlock. |
| `test_pac1934` | 1 | The PAC1934 rail monitor: which device owns a bus fault. A regulator timeout is an ERROR in NORMAL and WARNING, and a WARNING in MANUAL, but only while TRANS is on. Other devices are WARNING. |
| `test_temperature` | 3 | The NTC conversion, the temperature task, and the `temp` command. |
| `test_watchdog` | 4 | The application watchdog: five client check-ins, 1 s DONE pulses independent of WAKE, and the latched no-feed state after a missed check-in. |
| `test_modbus_port` | main | The USART1 RS-485 port: 9600 and 115200 baud setup, the DIR pin held through TX complete, RX callbacks, and the Timer1 RTU timeout. |

### Application logic

| Program | Tests | Covers |
|---|---:|---|
| `test_modbus` | main | The Modbus register map: holding and input registers, coil and discrete reads, the LED and mode registers, limits, rail telemetry, boot information, the clock, the event select, the build code, and delayed bootloader reset through `40060`. |
| `test_app_mode` | 5 | The operating mode: the self-test mapping, the mode order (MANUAL, ERROR, WARNING, NORMAL, revised 2026-10-15 so MANUAL outranks ERROR), that `mode manual` is reachable from ERROR while `mode normal` still isn't until the cause clears, the Modbus link-loss timeout and that it raises WARNING without changing mode priority, and the retry rules. |
| `test_fault_latch` | main | PC4/PC0 fault persistence, 50 ms debounce and brief-glitch rejection, EEPROM latch and clear behavior, WARNING without software shutdown, and regulator probing used by a normal-mode health check. |
| `test_app_limits` | main | Over-temperature and overcurrent limits: the trip counts, the NORMAL-only shutdown, the persisted limit ERROR, `fault clear` and `fault clear all` recovery, the `limit` command, and the event ring. |
| `test_app_retry` | main | Timed recovery, manual and disabled gates, repeated failed attempts, and restoration of the saved VOUT setpoint. |
| `test_nvm_config` | main | Internal EEPROM configuration record migration from version 1, version 2 retry settings, and checksum rejection. |
| `test_app_config` | 7 | The configuration record: defaults, damaged or invalid records falling back, the `config` commands, `config_reset confirm`, and the link timeout. |
| `test_app_boot` | 8 | Boot causes from the reset flags, the client-watchdog note, the COMMAND cause, the 10-entry boot history, and `bootinfo`. |
| `test_app_setpoint` | 8 | The VOUT readback check: the 2 s settle, the 5 % tolerance with a 100 mV floor, three samples before a mismatch, the setpoint change restarting the settle, and no check with outputs off. |
| `test_app_clock` | 12 | The software clock: unset until set, the tick advance, the calendar conversion, the `time` command, the 15-minute checkpoint, the restore from the latest slot, and the clock source on events. |

### Support files

| File | Purpose |
|---|---|
| `tests/support/avr/io.h`, `interrupt.h` | Fake AVR registers and pins. Pin defines are the host values, for example `PC0 0`. |
| `tests/support/util/atomic.h` | A single-threaded `ATOMIC_BLOCK` for the host. It doesn't check interrupt safety. |
| `tests/support/util/delay.h` | `_delay_ms` as a no-op. |
| `tests/support/util/twi.h` | TWI register stand-ins. |
| `tests/support/fake_clock_nvm.c` | An in-RAM stand-in for the clock checkpoint ring in internal EEPROM. |

Most tests replace the BSP with fakes. For example, `test_app_limits` fakes the EEPROM. The fakes live in each test file, so a test controls exactly what the module under test sees.

## Adding a test

1. Add `tests/unit/test_<name>.c`. Use the `CHECK(expr)` macro, which counts failures and prints the first failing line.
2. Add a target for it in `tests/Makefile`. List the production sources it needs, and the fakes it doesn't link.
3. Add the executable to `TARGETS`, and to the `test` rule.
4. Run `make -C tests test`. The new test must pass before it is committed.

Keep each program to one module. A program that needs many modules usually needs fakes for the rest.

## What the tests do not cover

- **Interrupt timing and load.** The atomic shim doesn't model interrupt safety, and the tests don't measure how long an interrupt takes on the MCU.
- **Electrical behaviour.** I2C rise time, RS-485 bias and timing, and the eFuse thresholds need a bench check.
- **The bus.** The TWI and RS-485 tests use registers, not a real bus. A real master is needed to check Modbus framing under load.
- **The regulator and the rails.** The TPS55287 output and feedback are not calibrated on the host.
- **Timing budgets.** The tests check values computed from `F_CPU` (12 MHz). They don't measure real timing on the board.

Run the tests after every change, and check the board items above on the hardware before a bring-up.
