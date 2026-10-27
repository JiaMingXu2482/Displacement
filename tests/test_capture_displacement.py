import binascii
import csv
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import capture_displacement as cap


def frame(first=0, values=(0,) * 64, fault=0):
    body = cap.HEADER.pack(cap.MAGIC, first, 5000, len(values), fault)
    body += struct.pack(f"<{len(values)}h", *values)
    return body + struct.pack("<H", binascii.crc_hqx(body, 0xFFFF))


class CaptureTests(unittest.TestCase):
    def test_bytewise_and_signed(self):
        decoder = cap.Decoder()
        received = []
        for b in b"old text\r\n" + frame(10, (-3, 0, 7500, 15000)):
            received.extend(decoder.feed(bytes([b])))
        self.assertEqual(received, [(10, 0, (-3, 0, 7500, 15000))])

    def test_corruption_recovers_without_inventing_values(self):
        decoder = cap.Decoder()
        bad = bytearray(frame())
        bad[30] ^= 0x80
        result = decoder.feed(bad + frame(64))
        self.assertEqual([r[0] for r in result], [64])
        self.assertEqual(decoder.crc_errors, 1)

    def test_invalid_header_and_bounded_garbage(self):
        d = cap.Decoder()
        self.assertEqual(d.feed(b"x" * 1000000), [])
        self.assertLessEqual(len(d.buffer), 3)
        bad = cap.HEADER.pack(cap.MAGIC, 0, 5000, 65535, 0)
        self.assertEqual(len(d.feed(bad + frame(64))), 1)

    def test_wrap_loss_and_csv_time(self):
        output = io.StringIO()
        recorder = cap.CsvRecorder(output)
        recorder.accept(0xFFFFFFC0, 0, (7500,) * 64)
        recorder.accept(0, 0, (15000,) * 64)
        recorder.accept(128, 0, (-3,))  # Missing indices 64..127 from the device.
        self.assertEqual(recorder.missing, 64)
        rows = list(csv.reader(io.StringIO(output.getvalue())))
        self.assertEqual(rows[-1], ["192", "0.038400", "-0.3"])
        self.assertEqual(rows[2], ["1", "0.000200", "750.0"])

    def test_reset_and_mcu_fault(self):
        r = cap.CsvRecorder(io.StringIO())
        r.accept(128, 0, (0,) * 64)
        with self.assertRaisesRegex(RuntimeError, "backwards"):
            r.accept(0, 0, (0,) * 64)
        with self.assertRaisesRegex(RuntimeError, "overflow"):
            r.accept(192, 3, ())

    def test_actual_c_packets(self):
        fixture = Path(__file__).parent / "stream_packets.bin"
        self.assertTrue(fixture.exists(), "Compile/run stream_protocol_test.c first")
        frames = cap.Decoder().feed(fixture.read_bytes())
        self.assertEqual(len(frames), 3)
        self.assertEqual(frames[0][0], 0xFFFFFFC0)
        self.assertEqual(frames[0][2][:2], (-32768, 32767))
        self.assertEqual(frames[1][0], 0)
        self.assertEqual(frames[2], (64, 3, ()))

    def test_long_capture_50000_samples(self):
        d, output = cap.Decoder(), io.StringIO()
        r = cap.CsvRecorder(output)
        for index in range(0, 50000, 50):
            packet = frame(index, tuple(range(50)))
            for pos in range(0, len(packet), 17):
                for received in d.feed(packet[pos:pos + 17]):
                    r.accept(*received)
        self.assertEqual(r.total, 50000)
        self.assertEqual(r.missing, 0)
        self.assertEqual(d.crc_errors, 0)
        self.assertTrue(output.getvalue().endswith("49999,9.999800,4.9\r\n"))

    def test_native_dcb_layout_and_config_parser(self):
        if sys.platform != "win32":
            self.skipTest("Windows only")
        self.assertEqual(cap.ct.sizeof(cap.DCB), 28)
        k = cap.ct.WinDLL("kernel32", use_last_error=True)
        k.BuildCommDCBW.argtypes = [cap.wt.LPCWSTR, cap.ct.POINTER(cap.DCB)]
        k.BuildCommDCBW.restype = cap.wt.BOOL
        config = cap.DCB()
        config.DCBlength = 28
        self.assertTrue(k.BuildCommDCBW("baud=921600 parity=n data=8 stop=1 dtr=off rts=off xon=off odsr=off octs=off", cap.ct.byref(config)))
        self.assertEqual(config.BaudRate, 921600)
        self.assertEqual(config.ByteSize, 8)

    def test_recorder_finalizes_csv_and_metadata(self):
        class FakeSerial:
            def __init__(self, port): self.n = 0
            def read(self):
                self.n += 1
                if self.n == 1: return frame(2000)
                raise KeyboardInterrupt
            def close(self): pass
        root = Path(__file__).resolve().parents[1] / "tmp"
        with tempfile.TemporaryDirectory(dir=root) as directory:
            self.assertTrue(Path(directory).resolve().is_relative_to(root.resolve()))
            path = Path(directory) / "capture.csv"
            with patch.object(cap, "WindowsSerial", FakeSerial), patch.object(cap, "serial_ports", return_value=[]):
                self.assertEqual(cap.main(["--port", "COM99", "--output", str(path)]), 0)
            self.assertEqual(len(path.read_text().splitlines()), 65)
            metadata = json.loads(path.with_suffix(".json").read_text())
            self.assertEqual(metadata["samples_saved"], 64)
            self.assertEqual(metadata["first_device_index"], 2000)
            self.assertIsNone(metadata["error"])


if __name__ == "__main__":
    unittest.main()
