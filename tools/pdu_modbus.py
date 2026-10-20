#!/usr/bin/env python3
"""Inspect and configure a DAL PDU over Modbus RTU (one-based register numbers)."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
from decimal import Decimal, InvalidOperation
import json
import sys
import time


EXCEPTION_NAMES = {
    1: "illegal function",
    2: "illegal address",
    3: "illegal value",
    4: "device failure",
}
MODES = ("NORMAL", "MANUAL", "WARNING", "ERROR")
BOOT_CAUSES = ("UNKNOWN", "POWER_ON", "BROWN_OUT", "EXTERNAL", "AVR_WATCHDOG",
               "CLIENT_WATCHDOG", "COMMAND")
CLOCK_SOURCES = ("UNSET", "RESTORED", "SET")


class ModbusError(Exception):
    """Transport, framing, or Modbus exception response."""


class ModbusDeviceError(ModbusError):
    """Exception returned by the PDU; retrying the same request will not help."""


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xA001 if crc & 1 else 0)
    return crc


def u16(data: bytes) -> int:
    return int.from_bytes(data, "big")


def signed16(value: int) -> int:
    return value - 0x10000 if value & 0x8000 else value


def words32(high: int, low: int) -> int:
    return (high << 16) | low


class RtuClient:
    def __init__(self, port, unit: int, baud: int, timeout: float, retries: int = 1):
        if not 1 <= unit <= 247:
            raise ValueError("unit must be 1..247")
        if baud <= 0 or timeout <= 0 or retries < 0:
            raise ValueError("baud and timeout must be positive; retries must be nonnegative")
        self.port = port
        self.unit = unit
        self.timeout = timeout
        self.retries = retries
        self.gap = max(0.00175, 3.5 * 11 / baud)
        self.last_end = 0.0

    def _read_exact(self, count: int) -> bytes:
        result = bytearray()
        deadline = time.monotonic() + self.timeout
        while len(result) < count and time.monotonic() < deadline:
            chunk = self.port.read(count - len(result))
            if chunk:
                result.extend(chunk)
        if len(result) != count:
            raise ModbusError(f"response timeout: received {len(result)}/{count} bytes")
        return bytes(result)

    def _exchange_once(self, function: int, payload: bytes) -> bytes:
        wait = self.gap - (time.monotonic() - self.last_end)
        if wait > 0:
            time.sleep(wait)
        request = bytes((self.unit, function)) + payload
        request += crc16(request).to_bytes(2, "little")
        self.port.reset_input_buffer()
        self.port.write(request)
        self.port.flush()
        try:
            header = self._read_exact(2)
            if header[0] != self.unit:
                raise ModbusError(f"unexpected slave address {header[0]}")
            if header[1] == (function | 0x80):
                response = header + self._read_exact(3)
            elif header[1] == function:
                if function in (1, 2, 3, 4):
                    byte_count = self._read_exact(1)
                    response = header + byte_count + self._read_exact(byte_count[0] + 2)
                else:
                    response = header + self._read_exact(6)
            else:
                raise ModbusError(f"unexpected function 0x{header[1]:02X}")
            if crc16(response[:-2]) != int.from_bytes(response[-2:], "little"):
                raise ModbusError("response CRC mismatch")
            if response[1] & 0x80:
                code = response[2]
                name = EXCEPTION_NAMES.get(code, "unknown exception")
                raise ModbusDeviceError(f"Modbus exception {code}: {name}")
            return response[2:-2]
        finally:
            self.last_end = time.monotonic()

    def request(self, function: int, payload: bytes) -> bytes:
        for attempt in range(self.retries + 1):
            try:
                return self._exchange_once(function, payload)
            except ModbusDeviceError:
                raise
            except ModbusError:
                if attempt == self.retries:
                    raise
        raise AssertionError("unreachable")

    def read_registers(self, kind: str, address: int, count: int) -> list[int]:
        base, function = (40001, 3) if kind == "holding" else (30001, 4)
        if not 1 <= count <= 125 or address < base:
            raise ValueError("invalid register address or count")
        payload = (address - base).to_bytes(2, "big") + count.to_bytes(2, "big")
        response = self.request(function, payload)
        if len(response) != 1 + count * 2 or response[0] != count * 2:
            raise ModbusError("unexpected register count")
        return [u16(response[1 + 2 * i:3 + 2 * i]) for i in range(count)]

    def read_bits(self, kind: str, address: int, count: int) -> list[bool]:
        base, function = (1, 1) if kind == "coil" else (10001, 2)
        if not 1 <= count <= 2000 or address < base:
            raise ValueError("invalid bit address or count")
        payload = (address - base).to_bytes(2, "big") + count.to_bytes(2, "big")
        response = self.request(function, payload)
        expected = (count + 7) // 8
        if len(response) != expected + 1 or response[0] != expected:
            raise ModbusError("unexpected bit count")
        return [bool(response[1 + i // 8] & (1 << (i % 8))) for i in range(count)]

    def write_register(self, address: int, value: int) -> None:
        if address < 40001 or not 0 <= value <= 0xFFFF:
            raise ValueError("invalid register address or value")
        payload = (address - 40001).to_bytes(2, "big") + value.to_bytes(2, "big")
        if self.request(6, payload) != payload:
            raise ModbusError("write acknowledgment mismatch")

    def write_registers(self, address: int, values: list[int]) -> None:
        if address < 40001 or not 1 <= len(values) <= 123 or any(not 0 <= v <= 0xFFFF for v in values):
            raise ValueError("invalid register address or values")
        prefix = (address - 40001).to_bytes(2, "big") + len(values).to_bytes(2, "big")
        payload = prefix + bytes((2 * len(values),)) + b"".join(v.to_bytes(2, "big") for v in values)
        if self.request(16, payload) != prefix:
            raise ModbusError("write acknowledgment mismatch")

    def write_coil(self, address: int, enabled: bool) -> None:
        if address < 1:
            raise ValueError("invalid coil address")
        payload = (address - 1).to_bytes(2, "big") + (0xFF00 if enabled else 0).to_bytes(2, "big")
        if self.request(5, payload) != payload:
            raise ModbusError("coil acknowledgment mismatch")


def enum_name(names: tuple[str, ...], value: int) -> str:
    return names[value] if value < len(names) else f"UNKNOWN({value})"


def timestamp(high: int, low: int) -> str | None:
    value = words32(high, low)
    return None if value == 0xFFFFFFFF else datetime.fromtimestamp(value, timezone.utc).isoformat()


def read_status(client: RtuClient) -> dict:
    registers = client.read_registers("input", 30020, 21)
    flags = registers[0]
    trans_on = bool(flags & 2)
    vout_on = None
    if trans_on:
        try:
            vout_on = client.read_bits("coil", 1, 1)[0]
        except ModbusError:
            pass  # A tripped TRANS eFuse can make the TPS55287 unreadable.
    else:
        vout_on = False
    temperature = registers[1]
    rails = {}
    for index, name in enumerate(("vout", "input_12v", "trans_5v", "rail_5v")):
        voltage, current = registers[4 + 2 * index:6 + 2 * index]
        power = registers[15 + index]
        rails[name] = {
            "voltage_mv": None if voltage == 0xFFFF else voltage,
            "current_ma": None if current == 0xFFFF else current,
            "power_mw": None if power == 0xFFFF else power * 10,
        }
    return {
        "mode": enum_name(MODES, registers[2]),
        "trans_on": trans_on,
        "vout_on": vout_on,
        "led_on": bool(flags & 1),
        "fault_pc4_active": bool(flags & 4),
        "fault_pc0_active": bool(flags & 8),
        "fault_pc4_latched": bool(flags & 16),
        "fault_pc0_latched": bool(flags & 32),
        "setpoint_mismatch": bool(flags & 64),
        "temperature_c": None if temperature == 0x8000 else signed16(temperature) / 10,
        "setpoint_mv": None if registers[3] == 0xFFFF else registers[3],
        "rails": rails,
        "boot_count": registers[12],
        "boot_cause": enum_name(BOOT_CAUSES, registers[13]),
        "watchdog_source": registers[14],
        "clock": timestamp(registers[19], registers[20]),
    }


def read_config(client: RtuClient) -> dict:
    led = client.read_registers("holding", 40020, 4)
    settings = client.read_registers("holding", 40030, 1)
    # The firmware accepts these addresses individually, not as one block.
    limits = [client.read_registers("holding", address, 1)[0] for address in range(40031, 40036)]
    identity = client.read_registers("holding", 40100, 10)
    return {
        "mode": enum_name(MODES, settings[0]),
        "led_on_ms": words32(led[0], led[1]),
        "led_off_ms": words32(led[2], led[3]),
        "limit_temp_c": signed16(limits[0]) / 10,
        "limit_vout_ma": limits[1],
        "limit_trans_ma": limits[2],
        "retry_enable": bool(limits[3]),
        "retry_time_s": limits[4],
        "product": bytes((identity[0] >> 8, identity[0] & 255,
                          identity[1] >> 8, identity[1] & 255)).rstrip(b"\0").decode("ascii"),
        "firmware_version": f"{identity[2] >> 8}.{identity[2] & 255}.{identity[3]}",
        "cpu_mhz": identity[4],
        "slave_address": identity[5],
        "baud": words32(identity[9], identity[6]),
        "firmware_build": f"0x{words32(identity[7], identity[8]):08X}",
    }


def read_event(client: RtuClient, sequence: int) -> dict | None:
    if not 0 <= sequence <= 0xFFFF:
        raise ValueError("event sequence must be 0..65535")
    client.write_register(40050, sequence)
    data = client.read_registers("input", 30041, 9)
    if data[0] == 0:
        return None
    codes = ("UNKNOWN", "TEMP", "VOUT_CURRENT", "TRANS_CURRENT", "I2C_TIMEOUT",
             "CONFIG_CHANGED", "SETPOINT_MISMATCH", "CLOCK_SET")
    return {
        "sequence": data[0],
        "severity": enum_name(("UNKNOWN", "INFO", "WARNING"), data[1]),
        "code": enum_name(codes, data[2]),
        "mode": enum_name(MODES, data[3]),
        "value": signed16(data[4]),
        "time": timestamp(data[5], data[6]),
        "clock_source": enum_name(CLOCK_SOURCES, data[8]),
    }


def parse_on_off(value: str) -> bool:
    if value.lower() in ("on", "1", "true"):
        return True
    if value.lower() in ("off", "0", "false"):
        return False
    raise ValueError("expected on/off")


def bounded_int(value: str, low: int, high: int) -> int:
    number = int(value, 0) if value.startswith(("0x", "0X")) else int(value)
    if not low <= number <= high:
        raise ValueError(f"value must be {low}..{high}")
    return number


def set_value(client: RtuClient, name: str, value: str) -> None:
    if name in ("trans", "vout", "led-enable"):
        client.write_coil({"trans": 2, "vout": 1, "led-enable": 10}[name], parse_on_off(value))
    elif name == "mode":
        modes = {"normal": 0, "manual": 1, "retry": 4}
        if value.lower() not in modes:
            raise ValueError("mode must be normal, manual, or retry")
        client.write_register(40030, modes[value.lower()])
    elif name == "vout-mv":
        millivolts = bounded_int(value, 800, 20000)
        if millivolts % 10:
            raise ValueError("VOUT must be in 10 mV steps")
        client.write_register(40001, millivolts)
    elif name == "limit-temp-c":
        try:
            tenths = Decimal(value) * 10
        except InvalidOperation as exc:
            raise ValueError("temperature must be a decimal number") from exc
        if tenths != int(tenths):
            raise ValueError("temperature must be in 0.1 C steps")
        client.write_register(40031, bounded_int(str(int(tenths)), -400, 1250) & 0xFFFF)
    elif name in ("limit-vout-ma", "limit-trans-ma"):
        client.write_register(40032 if name == "limit-vout-ma" else 40033,
                              bounded_int(value, 100, 5000))
    elif name == "retry-enable":
        client.write_register(40034, int(parse_on_off(value)))
    elif name == "retry-time-s":
        client.write_register(40035, bounded_int(value, 1, 60))
    elif name in ("led-on-ms", "led-off-ms"):
        period = bounded_int(value, 1, 0xFFFFFFFF)
        client.write_registers(40020 if name == "led-on-ms" else 40022,
                               [period >> 16, period & 0xFFFF])
    elif name == "time":
        if value == "now":
            seconds = int(time.time())
        else:
            parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
            if parsed.tzinfo is None:
                raise ValueError("time needs a timezone, for example 2026-10-19T12:00:00Z")
            seconds = int(parsed.timestamp())
        if not 0 <= seconds < 0xFFFFFFFF:
            raise ValueError("time is outside the firmware clock range")
        client.write_registers(40040, [seconds >> 16, seconds & 0xFFFF])
    else:
        raise ValueError(f"unknown setting: {name}")


def show(data, as_json: bool) -> None:
    if as_json:
        print(json.dumps(data, indent=2))
    elif data is None:
        print("No event found")
    else:
        for name, value in data.items():
            if isinstance(value, dict):
                print(f"{name}:")
                for child, fields in value.items():
                    print(f"  {child}: " + ", ".join(f"{key}={field}" for key, field in fields.items()))
            else:
                print(f"{name}: {value}")


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--port", required=True, help="serial port, for example COM5 or /dev/ttyUSB0")
    result.add_argument("--baud", type=int, default=115200, help="active RTU baud (default: 115200)")
    result.add_argument("--unit", type=int, default=1, help="Modbus slave address (default: 1)")
    result.add_argument("--timeout", type=float, default=1.0, help="response timeout in seconds")
    result.add_argument("--retries", type=int, default=1, help="retries after a transport error")
    result.add_argument("--json", action="store_true", help="print reads as JSON")
    commands = result.add_subparsers(dest="command", required=True)
    commands.add_parser("status", help="show mode, faults, rails, outputs, and boot status")
    commands.add_parser("config", help="show active identity and Modbus settings")
    setting = commands.add_parser("set", help="write a supported PDU setting or control")
    setting.add_argument("name", choices=("trans", "vout", "led-enable", "mode", "vout-mv",
                                          "limit-temp-c", "limit-vout-ma", "limit-trans-ma",
                                          "retry-enable", "retry-time-s", "led-on-ms", "led-off-ms", "time"))
    setting.add_argument("value")
    commands.add_parser("fault-clear", help="clear an inactive eFuse fault latch")
    events = commands.add_parser("event", help="read an event (0 is newest)")
    events.add_argument("sequence", type=int, nargs="?", default=0)
    commands.add_parser("bootloader", help="request reset into the AVR bootloader")
    return result


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    try:
        import serial  # pyserial is needed only when connecting to hardware.
    except ImportError:
        print("Install the serial dependency: python -m pip install pyserial", file=sys.stderr)
        return 2
    try:
        with serial.Serial(args.port, args.baud, bytesize=8, parity="N", stopbits=1,
                           timeout=args.timeout, write_timeout=args.timeout) as port:
            client = RtuClient(port, args.unit, args.baud, args.timeout, args.retries)
            if args.command == "status":
                show(read_status(client), args.json)
            elif args.command == "config":
                show(read_config(client), args.json)
            elif args.command == "event":
                show(read_event(client, args.sequence), args.json)
            elif args.command == "set":
                set_value(client, args.name, args.value)
                print(f"Set {args.name} = {args.value}")
            elif args.command == "fault-clear":
                client.write_coil(3, True)
                print("Fault latch clear requested")
            elif args.command == "bootloader":
                client.write_register(40060, 0xB007)
                print("Bootloader reset acknowledged")
    except (ValueError, ModbusError, OSError, serial.SerialException) as exc:
        print(f"PDU Modbus error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    if len(sys.argv) == 1:
        from pdu_modbus_gui import main as gui_main

        gui_main()
    else:
        raise SystemExit(main())
