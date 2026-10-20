# Python PDU Modbus tool

`tools/pdu_modbus.py` provides a table GUI and command-line Modbus RTU client
for the PDU's USART1 RS-485 port. Connect a USB RS-485 adapter to the PDU bus,
then install Python 3.10+ and the serial dependency from the repository root:

```powershell
python -m pip install -r tools/requirements-modbus.txt
```

The default Modbus settings are slave address **1**, **115200 baud**, and 8N1.
The CLI uses a one-second response timeout. Address and baud changes stored in
the PDU take effect after its next reboot. The software text console and UART0
XTP use different physical ports.

## Find the serial port

On Windows, connect the USB RS-485 adapter and check **Device Manager → Ports
(COM & LPT)** for its COM number. You can also list ports after installing
`pyserial`:

```powershell
python -m serial.tools.list_ports
```

Use the adapter's actual port in place of `COM5` in the examples below. Connect
the adapter to the PDU's Modbus RS-485 A/B bus, not to the UART0 XTP or text
console port. Close any other program using that COM port before connecting.

## Table GUI

```powershell
python tools/pdu_modbus.py
```

Type the adapter's port (for example `COM5`) in the GUI's **Port** field. Enter
the PDU's active **Baud** and **Unit** address, then select **Connect**. The GUI
does not take a `--port` argument. The table shows outputs, measured
rails, faults, protection settings, LED settings, and board information. Values
refresh every two seconds; **Refresh** or F5 reads immediately.

Use Up/Down to select a row. On a row marked **R/W**, press Enter to edit its
value, then Enter again to send the Modbus write. Esc cancels. Double-clicking
an editable row also opens its value. The hint below the table shows accepted
values, such as `on`, `off`, or a VOUT setpoint in mV. Read-only rows cannot be
changed. Action rows use Enter to clear an inactive eFuse latch or request a
bootloader reset; the reset asks for confirmation and disconnects the GUI.

Tkinter is included with most Python installations; the serial link requires
the `pyserial` dependency above. The firmware can reject a write if its safety
conditions are not met, and the GUI shows the returned Modbus error.

## Command line

Put `--port COM5` before the command name; replace `COM5` with the adapter's
port. Use `--baud` and `--unit` when the PDU uses nondefault settings, for
example `python tools/pdu_modbus.py --port COM7 --baud 9600 --unit 3 status`.

### Read status and settings

```powershell
python tools/pdu_modbus.py --port COM5 status
python tools/pdu_modbus.py --port COM5 config
python tools/pdu_modbus.py --port COM5 --json status
python tools/pdu_modbus.py --port COM5 event 0
```

`status` shows mode, output state, eFuse fault flags, board temperature, rail
voltage/current/power, boot cause, and clock. `config` shows active address and
baud, firmware identity, LED periods, protection limits, and retry settings.
`event 0` reads the newest logged event; use a sequence number for an older one.
Missing telemetry is displayed as `None` (or `null` with `--json`).

### Configure and control

```powershell
python tools/pdu_modbus.py --port COM5 set vout-mv 5000
python tools/pdu_modbus.py --port COM5 set vout on
python tools/pdu_modbus.py --port COM5 set trans off
python tools/pdu_modbus.py --port COM5 set mode manual
python tools/pdu_modbus.py --port COM5 set retry-enable on
python tools/pdu_modbus.py --port COM5 set retry-time-s 10
python tools/pdu_modbus.py --port COM5 set limit-temp-c 85.0
python tools/pdu_modbus.py --port COM5 set limit-vout-ma 1500
python tools/pdu_modbus.py --port COM5 set led-on-ms 1000
python tools/pdu_modbus.py --port COM5 set time now
python tools/pdu_modbus.py --port COM5 fault-clear
python tools/pdu_modbus.py --port COM5 bootloader
```

Other `set` names are `led-enable`, `mode` (`normal`, `manual`, `retry`),
`limit-trans-ma`, and `led-off-ms`. VOUT accepts 800–20000 mV in 10 mV steps.
`fault-clear` clears the inactive eFuse latch; active faults are rejected by
firmware. `bootloader` writes `0xB007` to holding register `40060`, waits for its
acknowledgment, and then the PDU turns outputs off and resets into its bootloader.
The firmware image is transferred separately over UART0 XTP.

The current Modbus map reports the active slave address and baud but cannot change
them or the communications timeout. Use the text console's `config set address`,
`config set baud`, or `config set timeout` commands for those settings. See the
[register map](modbus_implementation.md) for the exact units and behavior.

## Test without hardware

```powershell
python -B -m unittest discover -s tests -p 'test_pdu_modbus*.py' -v
```

These tests exercise RTU framing, CRC, register offsets, decoding, write
validation, and GUI edit flow without hardware. Verify the RS-485 wiring and
timing on the assembled board.
