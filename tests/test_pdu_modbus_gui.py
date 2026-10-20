"""Host-only checks for the table GUI's register mapping and edit flow."""

import pathlib
import queue
import sys
import tkinter as tk
import unittest
from unittest.mock import patch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import pdu_modbus_gui as gui


class GuiTests(unittest.TestCase):
    def test_rows_have_unique_keys_and_render_values(self):
        self.assertEqual(len(gui.ROWS), len(gui.ROW_BY_KEY))
        snapshot = {"status": {"setpoint_mv": 5000, "trans_on": True}}
        self.assertEqual(gui.row_value(gui.ROW_BY_KEY["setpoint"], snapshot), "5000")
        self.assertEqual(gui.row_value(gui.ROW_BY_KEY["trans"], snapshot), "On")
        self.assertEqual(gui.row_value(gui.ROW_BY_KEY["temperature"], snapshot), "—")

    def test_snapshot_reads_led_coil_and_configuration(self):
        class Client:
            def read_bits(self, kind, address, count):
                self_called.append((kind, address, count))
                return [True]

        self_called = []
        client = Client()
        with patch.object(gui.pdu, "read_status", return_value={"mode": "MANUAL"}), \
             patch.object(gui.pdu, "read_config", return_value={"retry_time_s": 5}):
            data = gui.read_snapshot(client)
        self.assertTrue(data["status"]["led_enabled"])
        self.assertEqual(data["config"]["retry_time_s"], 5)
        self.assertEqual(self_called, [("coil", 10, 1)])

    def test_enter_edit_then_enter_queues_write(self):
        try:
            root = tk.Tk()
        except tk.TclError as exc:
            self.skipTest(f"Tk display unavailable: {exc}")
        try:
            app = gui.PduGui(root)
            root.update()
            app.requests = queue.Queue()  # Keep the write for inspection; no hardware needed.
            app.connected = True
            app.tree.focus("setpoint")
            app.tree.selection_set("setpoint")
            app.tree.see("setpoint")
            root.update()
            app._activate()
            self.assertIsNotNone(app.editor)
            app.editor.delete(0, "end")
            app.editor.insert(0, "5000")
            app._commit_edit(gui.ROW_BY_KEY["setpoint"])
            self.assertEqual(app.requests.get_nowait(), ("set", ("vout-mv", "5000")))
        finally:
            root.destroy()


if __name__ == "__main__":
    unittest.main()
