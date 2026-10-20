# Host unit tests

From the repository root, run `make -C PDU/tests test` with a host C compiler
and GNU Make. Run `make -C PDU/tests clean` to remove the test executables.
The Makefile builds the scheduler twice: once with its normal configuration and
once with `USE_SCH_SEMAPHORE` enabled. It also builds the command line parser
and command table, using a test output hook to capture `help` and `version`.

These tests compile the production `scheduler.c` and use a single-threaded
`util/atomic.h` shim. They check public tick, timer, lifecycle, semaphore,
per-task periods, stop/resume, period changes, capacity, and pending flag
coalescing. The shim does not validate AVR interrupt safety or real-time timing.

Register a periodic task after `SCH_Initialize()` with a nonzero period:
`SCH_TaskPropertyTypedef task = {10U, update_console};` then
`SCH_TASK_CreateTask(&handle, &task)`. The scheduler copies this property.
Timer0 advances each enabled task's period every 1 ms; callbacks run from
`SCH_HandleScheduledTask()` in the main loop. `SCH_TASK_SetPeriod()` changes
one task's interval and restarts its count. If the main loop falls behind,
multiple due periods become one pending callback.

Command line tests cover exact space-separated arguments, the eight-argument
limit, unknown commands, LED timing get/set, I2C detect/get/set argument
validation and output with a mocked BSP, and handler error codes. The parser changes its
input buffer in place, so callers must provide writable, null-terminated text.

FIFO tests cover caller-owned storage, different capacities (including 300
bytes), full and empty behavior, wraparound, clear, and independent instances.
For UART use, declare storage such as `static uint8_t rx_storage[64];` and a
`static FIFO_t rx_fifo;`, then call `FIFO_Init(&rx_fifo, rx_storage,
sizeof(rx_storage))` before enabling UART interrupts. A failed `FIFO_Push`
means the new byte was rejected; the caller can count or report overflows.

UART driver tests use fake AVR registers and call the interrupt handlers
directly. They check USART0 FIFO transfers, receive errors and overruns, console
pin and Timer2 setup, software UART TX bits, back-to-back RX frames during TX,
framing errors, BSP
dispatch by UART number, and callbacks invoked only for queued RX bytes.
The BSP tests also initialize USART0 alone and verify its descriptor and
interrupt-driven TX path. Board-level `BSP_UART_Write` and `BSP_UART_Read`
tests cover UART0 and console selection, FIFO transfer, invalid numbers,
and stopped ports.
They do not measure electrical timing or interrupt load on the target MCU.

The app startup test checks the board's UART0 and console pin directions,
115200 baud XTP/XCLI and 9600 baud software-console settings, queued messages, enabled transmit interrupts, and the
TWI prescaler and 100 kHz bit-rate setting. It checks
Timer0's 1 ms CTC setup and ISR tick, the 10 ms console/watchdog, 20 ms
heartbeat, and 6 ms power tasks, console command dispatch (`version`,
`help`, unknown commands, and backspace), and PA3 output setup. It verifies
that LED initialization releases PORTA from the AVR external memory interface
when that interface was left enabled. Heartbeat checks cover the default
1000 ms ON/OFF phases and updated periods set through the console.

Both the software UART and UART0 use the same text command table. The app
startup test sends interleaved partial lines to the two ports, checks that
each response returns to its source, and verifies routing while a longer
UART0 response is still draining.

The watchdog test checks PB6 DONE pulses every 1 s from the 10 ms application task,
independence from PB5 WAKE, and the latched no-feed state after a missed
console check-in or stalled scheduler dispatch. Pulse timing must be checked on the board.

The DIO test covers the chapter 9 `do_t` API on AVR ports A, B, and G,
including output initialization, set/reset/toggle status, preserved neighboring
bits, and rejection of invalid port or pin numbers.

Command tests cover the TRANS on/off handler and I2C address and byte parsing.
The app startup test checks automatic TRANS shutdown on PC4 and PC0 faults at
the next 6 ms power task, and immediate system-data updates on output commands.
I2C bus ACK/NACK behavior and pull-up timing require a target-board check;
the host TWI registers do not emulate bus hardware.

Modbus tests check the implemented register addresses, LED timing and enable
state, TRANS control rejection on faults, and fault/status reads. The USART1
port test checks 9600 and 115200 baud setup, PD5 direction hold through TX complete, RX
callbacks, and the Timer1 RTU timeout. It does not emulate an RS485 bus or
verify RTU timing under interrupt load; test a real master on the board.
The TPS55287 host test runs with `make test_tps55287.exe && ./test_tps55287.exe` from this directory.
It covers the dual-address BSP, reference byte order and conversion, output enable interlock,
status flag handling, and command responses. It does not verify the board's feedback wiring or
calibrate the actual VOUT; check those on hardware before enabling the rail.
