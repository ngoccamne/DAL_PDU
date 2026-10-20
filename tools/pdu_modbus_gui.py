#!/usr/bin/env python3
"""Keyboard-friendly table editor for the DAL PDU Modbus RTU interface."""

from __future__ import annotations

from dataclasses import dataclass
import queue
import threading
import tkinter as tk
from tkinter import messagebox, ttk

import pdu_modbus as pdu


@dataclass(frozen=True)
class Row:
    key: str
    group: str
    label: str
    path: str = ""
    unit: str = ""
    write_name: str = ""
    action: str = ""
    hint: str = ""


ROWS = (
    Row("mode", "Outputs", "Operating mode", "status.mode", write_name="mode",
        hint="Enter normal, manual, or retry"),
    Row("trans", "Outputs", "TRANS branch", "status.trans_on", write_name="trans",
        hint="Enter on or off"),
    Row("vout", "Outputs", "VOUT enable", "status.vout_on", write_name="vout",
        hint="Enter on or off; first program a VOUT setpoint"),
    Row("setpoint", "Outputs", "VOUT setpoint", "status.setpoint_mv", "mV", "vout-mv",
        hint="800 to 20000 mV in 10 mV steps"),
    Row("temperature", "Telemetry", "Board temperature", "status.temperature_c", "C"),
    Row("vout_voltage", "Telemetry", "VOUT voltage", "status.rails.vout.voltage_mv", "mV"),
    Row("vout_current", "Telemetry", "VOUT current", "status.rails.vout.current_ma", "mA"),
    Row("vout_power", "Telemetry", "VOUT power", "status.rails.vout.power_mw", "mW"),
    Row("input_voltage", "Telemetry", "+12 input voltage", "status.rails.input_12v.voltage_mv", "mV"),
    Row("input_current", "Telemetry", "+12 input current", "status.rails.input_12v.current_ma", "mA"),
    Row("input_power", "Telemetry", "+12 input power", "status.rails.input_12v.power_mw", "mW"),
    Row("trans_voltage", "Telemetry", "+5_TRANS voltage", "status.rails.trans_5v.voltage_mv", "mV"),
    Row("trans_current", "Telemetry", "+5_TRANS current", "status.rails.trans_5v.current_ma", "mA"),
    Row("trans_power", "Telemetry", "+5_TRANS power", "status.rails.trans_5v.power_mw", "mW"),
    Row("rail_voltage", "Telemetry", "+5 voltage", "status.rails.rail_5v.voltage_mv", "mV"),
    Row("rail_current", "Telemetry", "+5 current", "status.rails.rail_5v.current_ma", "mA"),
    Row("rail_power", "Telemetry", "+5 power", "status.rails.rail_5v.power_mw", "mW"),
    Row("pc4_active", "Faults", "TRANS eFuse FLT active", "status.fault_pc4_active"),
    Row("pc0_active", "Faults", "MCU eFuse FLT active", "status.fault_pc0_active"),
    Row("pc4_latched", "Faults", "TRANS eFuse latched", "status.fault_pc4_latched"),
    Row("pc0_latched", "Faults", "MCU eFuse latched", "status.fault_pc0_latched"),
    Row("mismatch", "Faults", "Setpoint mismatch", "status.setpoint_mismatch"),
    Row("fault_clear", "Faults", "Clear eFuse latch", action="fault-clear",
        hint="Press Enter to clear latches whose fault inputs are inactive"),
    Row("limit_temp", "Protection", "Temperature limit", "config.limit_temp_c", "C", "limit-temp-c",
        hint="-40.0 to 125.0 C in 0.1 C steps"),
    Row("limit_vout", "Protection", "VOUT current limit", "config.limit_vout_ma", "mA", "limit-vout-ma",
        hint="100 to 5000 mA"),
    Row("limit_trans", "Protection", "+5_TRANS current limit", "config.limit_trans_ma", "mA",
        "limit-trans-ma", hint="100 to 5000 mA"),
    Row("retry_enable", "Protection", "Automatic retry", "config.retry_enable", write_name="retry-enable",
        hint="Enter on or off"),
    Row("retry_time", "Protection", "Retry interval", "config.retry_time_s", "s", "retry-time-s",
        hint="1 to 60 seconds"),
    Row("led_enabled", "LED", "Heartbeat enabled", "status.led_enabled", write_name="led-enable",
        hint="Enter on or off"),
    Row("led_actual", "LED", "LED output", "status.led_on"),
    Row("led_on", "LED", "Normal ON period", "config.led_on_ms", "ms", "led-on-ms",
        hint="Positive number of milliseconds"),
    Row("led_off", "LED", "Normal OFF period", "config.led_off_ms", "ms", "led-off-ms",
        hint="Positive number of milliseconds"),
    Row("clock", "Board", "Clock UTC", "status.clock", write_name="time",
        hint="Enter now or an ISO 8601 time with timezone, e.g. 2026-10-19T12:00:00Z"),
    Row("boot_count", "Board", "Boot count", "status.boot_count"),
    Row("boot_cause", "Board", "Last boot cause", "status.boot_cause"),
    Row("watchdog_source", "Board", "Watchdog source", "status.watchdog_source"),
    Row("product", "Board", "Product", "config.product"),
    Row("version", "Board", "Firmware version", "config.firmware_version"),
    Row("build", "Board", "Firmware build", "config.firmware_build"),
    Row("address", "Board", "Modbus address", "config.slave_address"),
    Row("baud", "Board", "Modbus baud", "config.baud", "bps"),
    Row("cpu", "Board", "CPU clock", "config.cpu_mhz", "MHz"),
    Row("bootloader", "Board", "Reset to bootloader", action="bootloader",
        hint="Press Enter to request the AVR bootloader; outputs will turn off"),
)
ROW_BY_KEY = {row.key: row for row in ROWS}


def row_value(row: Row, snapshot: dict) -> str:
    if row.action:
        return "Press Enter"
    value = snapshot
    for part in row.path.split("."):
        if not isinstance(value, dict):
            return "—"
        value = value.get(part)
    if value is None:
        return "—"
    if isinstance(value, bool):
        return "On" if value else "Off"
    return str(value)


def read_snapshot(client) -> dict:
    status = pdu.read_status(client)
    status["led_enabled"] = client.read_bits("coil", 10, 1)[0]
    return {"status": status, "config": pdu.read_config(client)}


class PduGui:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("DAL PDU Modbus")
        self.root.geometry("920x680")
        self.requests: queue.Queue = queue.Queue()
        self.results: queue.Queue = queue.Queue()
        self.connected = False
        self.connecting = False
        self.refresh_pending = False
        self.editor: ttk.Entry | None = None
        self.auto_refresh = tk.BooleanVar(value=True)
        self.port_var = tk.StringVar(value="COM5")
        self.baud_var = tk.StringVar(value="115200")
        self.unit_var = tk.StringVar(value="1")
        self.status_var = tk.StringVar(value="Disconnected")
        self.hint_var = tk.StringVar(value="Use ↑/↓ to select a row. Enter edits; Enter again sends. Esc cancels.")
        self._build()
        self.worker = threading.Thread(target=self._run_worker, name="pdu-modbus", daemon=True)
        self.worker.start()
        self.root.after(100, self._drain_results)
        self.root.after(2000, self._auto_refresh)
        self.root.protocol("WM_DELETE_WINDOW", self._close)

    def _build(self) -> None:
        panel = ttk.Frame(self.root, padding=10)
        panel.pack(fill="both", expand=True)
        connection = ttk.Frame(panel)
        connection.pack(fill="x", pady=(0, 8))
        ttk.Label(connection, text="Port").pack(side="left")
        ttk.Entry(connection, textvariable=self.port_var, width=16).pack(side="left", padx=(4, 12))
        ttk.Label(connection, text="Baud").pack(side="left")
        ttk.Combobox(connection, textvariable=self.baud_var, width=9,
                     values=("115200", "38400", "19200", "9600", "4800", "2400", "1200")).pack(
                         side="left", padx=(4, 12))
        ttk.Label(connection, text="Unit").pack(side="left")
        ttk.Entry(connection, textvariable=self.unit_var, width=5).pack(side="left", padx=(4, 12))
        self.connect_button = ttk.Button(connection, text="Connect", command=self._toggle_connection)
        self.connect_button.pack(side="left")
        ttk.Button(connection, text="Refresh", command=self._request_refresh).pack(side="left", padx=(8, 0))
        ttk.Checkbutton(connection, text="Auto refresh (2 s)", variable=self.auto_refresh).pack(
            side="left", padx=(12, 0))

        table_frame = ttk.Frame(panel)
        table_frame.pack(fill="both", expand=True)
        self.tree = ttk.Treeview(table_frame, columns=("value", "unit", "access"), show="tree headings",
                                 selectmode="browse")
        self.tree.heading("#0", text="Parameter")
        self.tree.heading("value", text="Value")
        self.tree.heading("unit", text="Unit")
        self.tree.heading("access", text="Access")
        self.tree.column("#0", width=300, minwidth=200)
        self.tree.column("value", width=390, minwidth=130)
        self.tree.column("unit", width=70, anchor="center")
        self.tree.column("access", width=80, anchor="center")
        scroll = ttk.Scrollbar(table_frame, orient="vertical", command=self.tree.yview)
        self.tree.configure(yscrollcommand=scroll.set)
        self.tree.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")
        groups = {}
        for row in ROWS:
            if row.group not in groups:
                groups[row.group] = self.tree.insert("", "end", text=row.group, open=True)
            access = "Action" if row.action else ("R/W" if row.write_name else "Read")
            self.tree.insert(groups[row.group], "end", iid=row.key, text=row.label,
                             values=("—" if not row.action else "Press Enter", row.unit, access))
        self.tree.bind("<Return>", self._activate)
        self.tree.bind("<Double-1>", self._activate)
        self.tree.bind("<<TreeviewSelect>>", self._selection_changed)
        self.tree.bind("<F5>", self._request_refresh)
        ttk.Label(panel, textvariable=self.hint_var).pack(fill="x", pady=(8, 2))
        ttk.Label(panel, textvariable=self.status_var).pack(fill="x")

    def _set_status(self, text: str) -> None:
        self.status_var.set(text)

    def _toggle_connection(self) -> None:
        if self.connecting:
            return
        if self.connected:
            self.requests.put(("disconnect", None))
            self.connecting = True
            self.connect_button.configure(state="disabled")
            self._set_status("Disconnecting…")
            return
        try:
            port = self.port_var.get().strip()
            baud = int(self.baud_var.get())
            unit = int(self.unit_var.get())
            if not port or baud <= 0 or not 1 <= unit <= 247:
                raise ValueError("enter a port, positive baud, and unit 1..247")
        except ValueError as exc:
            messagebox.showerror("Connection settings", str(exc), parent=self.root)
            return
        self.connecting = True
        self.connect_button.configure(state="disabled")
        self._set_status(f"Connecting to {port} at {baud} baud…")
        self.requests.put(("connect", (port, baud, unit)))

    def _request_refresh(self, event=None):
        if self.connected and not self.refresh_pending:
            self.refresh_pending = True
            self.requests.put(("refresh", None))
            self._set_status("Reading PDU…")
        return "break" if event is not None else None

    def _auto_refresh(self) -> None:
        if self.connected and self.auto_refresh.get():
            self._request_refresh()
        self.root.after(2000, self._auto_refresh)

    def _selection_changed(self, _event=None) -> None:
        row = ROW_BY_KEY.get(self.tree.focus())
        if row:
            self.hint_var.set(row.hint or ("Read only" if not row.write_name and not row.action else "Press Enter"))

    def _activate(self, event=None):
        if not self.connected:
            self._set_status("Connect to the PDU first")
            return "break"
        if event is not None and event.type == tk.EventType.ButtonPress:
            item = self.tree.identify_row(event.y)
            if item:
                self.tree.focus(item)
                self.tree.selection_set(item)
        row = ROW_BY_KEY.get(self.tree.focus())
        if row is None:
            return None
        if row.action:
            if row.action == "bootloader" and not messagebox.askyesno(
                    "Reset to bootloader", "Turn VOUT and TRANS off and reset into the AVR bootloader?",
                    parent=self.root):
                return "break"
            self.requests.put(("action", row.action))
            self._set_status(f"Sending {row.label}…")
        elif row.write_name:
            self._begin_edit(row)
        else:
            self._set_status(f"{row.label} is read only")
        return "break"

    def _begin_edit(self, row: Row) -> None:
        self._cancel_edit()
        bounds = self.tree.bbox(row.key, "value")
        if not bounds:
            self.tree.see(row.key)
            bounds = self.tree.bbox(row.key, "value")
        if not bounds:
            return
        x, y, width, height = bounds
        self.editor = ttk.Entry(self.tree)
        current = self.tree.set(row.key, "value")
        self.editor.insert(0, "" if current == "—" else current)
        self.editor.place(x=x, y=y, width=width, height=height)
        self.editor.focus_set()
        self.editor.selection_range(0, "end")
        self.editor.bind("<Return>", lambda _event: self._commit_edit(row))
        self.editor.bind("<Escape>", lambda _event: self._cancel_edit())

    def _commit_edit(self, row: Row):
        if self.editor is None:
            return "break"
        value = self.editor.get().strip()
        if not value:
            self._set_status("Enter a value, or press Esc to cancel")
            return "break"
        self._cancel_edit()
        self.requests.put(("set", (row.write_name, value)))
        self._set_status(f"Sending {row.label} = {value}…")
        return "break"

    def _cancel_edit(self):
        if self.editor is not None:
            self.editor.destroy()
            self.editor = None
        return "break"

    def _run_worker(self) -> None:
        port = None
        client = None
        while True:
            operation, data = self.requests.get()
            if operation == "stop":
                if port is not None:
                    port.close()
                return
            try:
                if operation == "connect":
                    import serial
                    if port is not None:
                        port.close()
                    name, baud, unit = data
                    port = serial.Serial(name, baud, bytesize=8, parity="N", stopbits=1,
                                         timeout=0.6, write_timeout=0.6)
                    client = pdu.RtuClient(port, unit, baud, timeout=0.6, retries=0)
                    self.results.put(("connected", name))
                elif operation == "disconnect":
                    if port is not None:
                        port.close()
                    port = None
                    client = None
                    self.results.put(("disconnected", None))
                elif client is None:
                    self.results.put(("error", (operation, "Connect to the PDU first")))
                elif operation == "refresh":
                    self.results.put(("snapshot", read_snapshot(client)))
                elif operation == "set":
                    name, value = data
                    pdu.set_value(client, name, value)
                    self.results.put(("sent", f"{name} = {value}"))
                elif operation == "action":
                    if data == "fault-clear":
                        client.write_coil(3, True)
                        self.results.put(("sent", "eFuse latch clear requested"))
                    elif data == "bootloader":
                        client.write_register(40060, 0xB007)
                        self.results.put(("bootloader_sent", "Bootloader reset acknowledged"))
                        port.close()
                        port = None
                        client = None
                        self.results.put(("disconnected", None))
            except Exception as exc:
                if operation == "connect":
                    port = None
                    client = None
                self.results.put(("error", (operation, str(exc))))

    def _drain_results(self) -> None:
        try:
            while True:
                operation, data = self.results.get_nowait()
                if operation == "connected":
                    self.connected = True
                    self.connecting = False
                    self.connect_button.configure(text="Disconnect", state="normal")
                    self._set_status(f"Connected to {data}")
                    if not self.tree.focus():
                        self.tree.focus("mode")
                        self.tree.selection_set("mode")
                    self.tree.focus_set()
                    self._request_refresh()
                elif operation == "disconnected":
                    self.connected = False
                    self.connecting = False
                    self.refresh_pending = False
                    self.connect_button.configure(text="Connect", state="normal")
                    self._set_status("Disconnected")
                elif operation == "snapshot":
                    self.refresh_pending = False
                    for row in ROWS:
                        self.tree.set(row.key, "value", row_value(row, data))
                    self._set_status("PDU values updated")
                elif operation == "sent":
                    self._set_status(f"Sent: {data}")
                    self._request_refresh()
                elif operation == "bootloader_sent":
                    self._set_status(data)
                elif operation == "error":
                    failed_operation, message = data
                    if failed_operation == "connect":
                        self.connecting = False
                        self.connect_button.configure(state="normal")
                    if failed_operation == "refresh":
                        self.refresh_pending = False
                    self._set_status(f"{failed_operation}: {message}")
                    if failed_operation in ("set", "action", "connect"):
                        messagebox.showerror("PDU Modbus", message, parent=self.root)
        except queue.Empty:
            pass
        self.root.after(100, self._drain_results)

    def _close(self) -> None:
        self.requests.put(("stop", None))
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    PduGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()
