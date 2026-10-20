"""Host-only tests for the PDU Modbus RTU client and register interpretation."""

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
import pdu_modbus as pdu


def response(body):
    return body + pdu.crc16(body).to_bytes(2, "little")


class FakePort:
    def __init__(self, reply):
        self.reply = reply
        self.incoming = b""
        self.requests = []

    def reset_input_buffer(self):
        self.incoming = b""

    def write(self, request):
        self.requests.append(request)
        self.incoming = self.reply(request)
        return len(request)

    def flush(self):
        pass

    def read(self, count):
        chunk, self.incoming = self.incoming[:min(count, 2)], self.incoming[min(count, 2):]
        return chunk


class RecordingClient:
    def __init__(self):
        self.writes = []

    def write_register(self, address, value):
        self.writes.append(("register", address, value))

    def write_registers(self, address, values):
        self.writes.append(("registers", address, values))

    def write_coil(self, address, value):
        self.writes.append(("coil", address, value))


class PduModbusTests(unittest.TestCase):
    def test_crc_and_one_based_register_offset(self):
        self.assertEqual(pdu.crc16(b"123456789"), 0x4B37)

        def reply(request):
            self.assertEqual(pdu.crc16(request[:-2]), int.from_bytes(request[-2:], "little"))
            self.assertEqual(request[0:6], bytes((7, 3, 0, 29, 0, 1)))  # 40030 -> offset 29
            return response(bytes((7, 3, 2, 0, 3)))

        port = FakePort(reply)
        client = pdu.RtuClient(port, unit=7, baud=115200, timeout=0.02, retries=0)
        self.assertEqual(client.read_registers("holding", 40030, 1), [3])
        self.assertEqual(len(port.requests), 1)

    def test_exception_and_bad_crc_are_reported(self):
        port = FakePort(lambda request: response(bytes((1, 0x86, 2))))
        client = pdu.RtuClient(port, 1, 115200, 0.02, retries=0)
        with self.assertRaisesRegex(pdu.ModbusError, "illegal address"):
            client.write_register(40060, 0xB007)
        port.reply = lambda request: response(bytes((1, 6)) + request[2:6])[:-1] + b"\x00"
        with self.assertRaisesRegex(pdu.ModbusError, "CRC mismatch"):
            client.write_register(40060, 0xB007)

    def test_write_echo_and_coil_values(self):
        port = FakePort(lambda request: response(request[:6] if request[1] == 16 else request[:-2]))
        client = pdu.RtuClient(port, 1, 115200, 0.02, retries=0)
        client.write_register(40060, 0xB007)
        client.write_coil(2, True)
        client.write_registers(40040, [0x1234, 0x5678])
        self.assertEqual(port.requests[0][0:6], bytes((1, 6, 0, 59, 0xB0, 7)))
        self.assertEqual(port.requests[1][0:6], bytes((1, 5, 0, 1, 0xFF, 0)))
        self.assertEqual(port.requests[2][0:7], bytes((1, 16, 0, 39, 0, 2, 4)))

    def test_status_and_config_units(self):
        class Client:
            def read_registers(self, kind, address, count):
                if address == 30020:
                    data = [0] * 21
                    data[0:4] = [0x12, 251, 2, 5000]
                    data[4:6] = [4990, 250]
                    data[12:15] = [4, 6, 0]
                    data[15] = 125  # 1250 mW
                    data[19:21] = [0xFFFF, 0xFFFF]
                    return data
                if address == 40020:
                    return [0, 1000, 0, 2000]
                if address == 40030:
                    return [2]
                if 40031 <= address <= 40035:
                    if count != 1:
                        raise AssertionError("limit and retry registers must be read separately")
                    return [[850, 1000, 1500, 1, 5][address - 40031]]
                if address == 40100:
                    return [0x5044, 0x5500, 0x0102, 3, 12, 7, 0xC200, 0x1234, 0x5678, 1]
                raise AssertionError((kind, address, count))

            def read_bits(self, kind, address, count):
                return [True]

        client = Client()
        status = pdu.read_status(client)
        self.assertEqual(status["mode"], "WARNING")
        self.assertTrue(status["trans_on"])
        self.assertTrue(status["vout_on"])
        self.assertTrue(status["fault_pc4_latched"])
        self.assertEqual(status["rails"]["vout"]["power_mw"], 1250)
        self.assertIsNone(status["clock"])
        config = pdu.read_config(client)
        self.assertEqual(config["baud"], 115200)
        self.assertEqual(config["limit_temp_c"], 85.0)
        self.assertEqual(config["firmware_version"], "1.2.3")

    def test_configuration_writes_and_validation(self):
        client = RecordingClient()
        pdu.set_value(client, "limit-temp-c", "-10.5")
        pdu.set_value(client, "retry-time-s", "12")
        pdu.set_value(client, "led-on-ms", "70000")
        pdu.set_value(client, "trans", "on")
        self.assertEqual(client.writes, [
            ("register", 40031, (-105) & 0xFFFF),
            ("register", 40035, 12),
            ("registers", 40020, [1, 4464]),
            ("coil", 2, True),
        ])
        with self.assertRaisesRegex(ValueError, "10 mV"):
            pdu.set_value(client, "vout-mv", "5001")
        with self.assertRaisesRegex(ValueError, "0.1 C"):
            pdu.set_value(client, "limit-temp-c", "10.25")


if __name__ == "__main__":
    unittest.main()
