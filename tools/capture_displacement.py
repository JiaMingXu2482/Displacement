"""Receive D5K1 packets into CSV. Windows serial I/O uses only Python's stdlib.

python capture_displacement.py --list
python capture_displacement.py --port COM6 --seconds 60
Ctrl+C closes and flushes the CSV. No serial assistant may own the same port.
"""
import argparse
import binascii
import csv
import ctypes as ct
from ctypes import wintypes as wt
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import struct
import sys
import time

MAGIC = b"D5K1"
HEADER = struct.Struct("<4sIIHH")
RATE = 5000
BAUD = 921600
MAX_SAMPLES = 64
FAULTS = {1: "ADC initialization/configuration failed", 2: "PDB clock invalid",
          3: "MCU sample buffer overflow", 4: "PDB ADC sequence error",
          5: "ADC stopped producing samples", 6: "UART transmit error"}


class Decoder:
    def __init__(self):
        self.buffer = bytearray()
        self.crc_errors = 0
        self.discarded_bytes = 0

    def feed(self, data):
        self.buffer.extend(data)
        frames = []
        while True:
            pos = self.buffer.find(MAGIC)
            if pos < 0:
                removed = max(0, len(self.buffer) - 3)
                self.discarded_bytes += removed
                del self.buffer[:removed]
                break
            self.discarded_bytes += pos
            del self.buffer[:pos]
            if len(self.buffer) < HEADER.size:
                break
            _, first, rate, count, fault = HEADER.unpack_from(self.buffer)
            if rate != RATE or count > MAX_SAMPLES or (count == 0 and fault == 0):
                del self.buffer[0]
                self.discarded_bytes += 1
                continue
            length = HEADER.size + 2 * count + 2
            if len(self.buffer) < length:
                break
            expected, = struct.unpack_from("<H", self.buffer, length - 2)
            if binascii.crc_hqx(self.buffer[:length - 2], 0xFFFF) != expected:
                self.crc_errors += 1
                self.discarded_bytes += 1
                del self.buffer[0]
                continue
            values = struct.unpack_from(f"<{count}h", self.buffer, HEADER.size)
            frames.append((first, fault, values))
            del self.buffer[:length]
        return frames


class CsvRecorder:
    def __init__(self, output):
        self.writer = csv.writer(output)
        self.writer.writerow(["sample_index", "time_s", "displacement_mm"])
        self.first_index = None
        self.expected = None
        self.total = 0
        self.missing = 0
        self.next_relative = 0

    def accept(self, first, fault, values):
        if fault:
            raise RuntimeError(f"MCU error {fault}: {FAULTS.get(fault, 'unknown error')}; reset board after fixing it")
        if self.expected is None:
            self.first_index = first
        else:
            gap = (first - self.expected) & 0xFFFFFFFF
            if gap >= 0x80000000:
                raise RuntimeError("Sample sequence went backwards (board reset or duplicate packet). Start a new recording.")
            self.missing += gap
            self.next_relative += gap
        rows = []
        for offset, position in enumerate(values):
            index = self.next_relative + offset
            rows.append((index, f"{index / RATE:.6f}", f"{position / 10:.1f}"))
        self.writer.writerows(rows)
        self.next_relative += len(values)
        self.expected = (first + len(values)) & 0xFFFFFFFF
        self.total += len(values)


def serial_ports():
    import winreg
    result = []
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DEVICEMAP\SERIALCOMM") as key:
            for i in range(winreg.QueryInfoKey(key)[1]):
                result.append(winreg.EnumValue(key, i)[1])
    except FileNotFoundError:
        pass
    return sorted(set(result), key=lambda p: int(p[3:]) if p[3:].isdigit() else 99999)


class DCB(ct.Structure):
    _fields_ = [("DCBlength", wt.DWORD), ("BaudRate", wt.DWORD), ("flags", wt.DWORD),
                ("wReserved", wt.WORD), ("XonLim", wt.WORD), ("XoffLim", wt.WORD),
                ("ByteSize", wt.BYTE), ("Parity", wt.BYTE), ("StopBits", wt.BYTE),
                ("XonChar", ct.c_char), ("XoffChar", ct.c_char), ("ErrorChar", ct.c_char),
                ("EofChar", ct.c_char), ("EvtChar", ct.c_char), ("wReserved1", wt.WORD)]


class Timeouts(ct.Structure):
    _fields_ = [(x, wt.DWORD) for x in ("ReadIntervalTimeout", "ReadTotalTimeoutMultiplier",
                "ReadTotalTimeoutConstant", "WriteTotalTimeoutMultiplier", "WriteTotalTimeoutConstant")]


class ComStat(ct.Structure):
    _fields_ = [("flags", wt.DWORD), ("cbInQue", wt.DWORD), ("cbOutQue", wt.DWORD)]


class WindowsSerial:
    def __init__(self, port):
        self.handle = None
        self.k = ct.WinDLL("kernel32", use_last_error=True)
        signatures = {
            "CreateFileW": ([wt.LPCWSTR, wt.DWORD, wt.DWORD, wt.LPVOID, wt.DWORD, wt.DWORD, wt.HANDLE], wt.HANDLE),
            "CloseHandle": ([wt.HANDLE], wt.BOOL),
            "GetCommState": ([wt.HANDLE, ct.POINTER(DCB)], wt.BOOL),
            "BuildCommDCBW": ([wt.LPCWSTR, ct.POINTER(DCB)], wt.BOOL),
            "SetCommState": ([wt.HANDLE, ct.POINTER(DCB)], wt.BOOL),
            "SetCommTimeouts": ([wt.HANDLE, ct.POINTER(Timeouts)], wt.BOOL),
            "SetupComm": ([wt.HANDLE, wt.DWORD, wt.DWORD], wt.BOOL),
            "PurgeComm": ([wt.HANDLE, wt.DWORD], wt.BOOL),
            "ReadFile": ([wt.HANDLE, wt.LPVOID, wt.DWORD, ct.POINTER(wt.DWORD), wt.LPVOID], wt.BOOL),
            "ClearCommError": ([wt.HANDLE, ct.POINTER(wt.DWORD), ct.POINTER(ComStat)], wt.BOOL),
        }
        for name, (args, result) in signatures.items():
            f = getattr(self.k, name)
            f.argtypes, f.restype = args, result
        handle = self.k.CreateFileW("\\\\.\\" + port, 0xC0000000, 0, None, 3, 0, None)
        if handle == ct.c_void_p(-1).value:
            raise ct.WinError(ct.get_last_error())
        self.handle = handle
        try:
            config = DCB()
            config.DCBlength = ct.sizeof(config)
            self.check(self.k.GetCommState(handle, ct.byref(config)))
            self.check(self.k.BuildCommDCBW(f"baud={BAUD} parity=n data=8 stop=1 dtr=off rts=off xon=off odsr=off octs=off", ct.byref(config)))
            # Binary, no null stripping, no software/hardware flow control.
            config.flags = 1
            self.check(self.k.SetCommState(handle, ct.byref(config)))
            self.check(self.k.SetupComm(handle, 1024 * 1024, 4096))
            self.check(self.k.SetCommTimeouts(handle, ct.byref(Timeouts(0xFFFFFFFF, 0, 100, 0, 1000))))
            self.check(self.k.PurgeComm(handle, 0x000A))
        except BaseException:
            self.close()
            raise

    @staticmethod
    def check(ok):
        if not ok:
            raise ct.WinError(ct.get_last_error())

    def read(self):
        errors, state = wt.DWORD(), ComStat()
        self.check(self.k.ClearCommError(self.handle, ct.byref(errors), ct.byref(state)))
        if errors.value:
            raise RuntimeError(f"Windows UART error 0x{errors.value:X} (overrun/framing/etc.)")
        buf, n = ct.create_string_buffer(16384), wt.DWORD()
        self.check(self.k.ReadFile(self.handle, buf, len(buf), ct.byref(n), None))
        return buf.raw[:n.value]

    def close(self):
        if self.handle is not None:
            self.k.CloseHandle(self.handle)
            self.handle = None


def main(argv=None):
    parser = argparse.ArgumentParser(description="5 kHz displacement -> CSV (921600, 8N1)")
    parser.add_argument("--port", help="e.g. COM6; otherwise choose from detected ports")
    parser.add_argument("--seconds", type=float, default=0, help="0 = until Ctrl+C")
    parser.add_argument("--output", type=Path, help="new CSV path; never overwrites a file")
    parser.add_argument("--list", action="store_true", help="list ports without opening them")
    args = parser.parse_args(argv)
    if os.name != "nt":
        parser.error("This recorder uses Windows native serial APIs.")
    if args.seconds < 0:
        parser.error("--seconds must be >= 0")
    ports = serial_ports()
    if args.list:
        print("Ports: " + (", ".join(ports) or "none"))
        return 0
    port = args.port
    if not port:
        print("Detected ports: " + (", ".join(ports) or "none"))
        port = input("Board port (e.g. COM6): ").strip().upper()
    if not port.upper().startswith("COM") or not port[3:].isdigit():
        parser.error("Enter a Windows COM port, e.g. COM6")
    path = args.output or (Path(__file__).resolve().parents[1] / "recordings" /
                          ("displacement_" + datetime.now().strftime("%Y%m%d_%H%M%S_%f") + ".csv"))
    path = path.resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.with_suffix(".json").exists():
        raise FileExistsError(f"Metadata already exists: {path.with_suffix('.json')}")
    decoder = Decoder()
    source = None
    started = time.monotonic()
    last_data = last_report = started
    capture_start = None
    error = None
    metadata = {"pc_start_utc": datetime.now(timezone.utc).isoformat(), "port": port,
                "baud": BAUD, "nominal_sample_rate_hz": RATE,
                "time_basis": "sample index / 5000; nominal FIRC clock, not PC reception timestamps",
                "wire_format": "D5K1 signed int16 in 0.1 mm; ADC remains 12 bit",
                "nominal_vref_v": 3.3, "shunt_ohms": 100.0, "range_mm": 1500.0}
    # Exclusive create; an existing recording is never truncated.
    with path.open("x", newline="", encoding="utf-8") as output:
        recorder = CsvRecorder(output)
        try:
            source = WindowsSerial(port)
            print(f"Saving: {path}\n921600 8N1, 5000 samples/s. Ctrl+C to stop and save.")
            while True:
                data = source.read()
                frames = decoder.feed(data)
                for frame in frames:
                    recorder.accept(*frame)
                    last_data = time.monotonic()
                    if capture_start is None:
                        capture_start = last_data
                now = time.monotonic()
                if now - last_data > 5:
                    raise RuntimeError("No valid D5K1 packets for 5 seconds. Download/run the 5 kHz firmware, verify COM port and 921600 baud.")
                if now - last_report >= 1:
                    output.flush()
                    print(f"Saved {recorder.total} | missing {recorder.missing} | CRC errors {decoder.crc_errors}", flush=True)
                    last_report = now
                if args.seconds and capture_start is not None and now - capture_start >= args.seconds:
                    break
        except KeyboardInterrupt:
            print("\nStopped by user.")
        except (OSError, RuntimeError) as exc:
            error = str(exc)
            print("\nERROR: " + error, file=sys.stderr)
        finally:
            if source is not None:
                source.close()
            output.flush()
            os.fsync(output.fileno())
            metadata.update({"samples_saved": recorder.total, "missing_samples": recorder.missing,
                             "crc_errors": decoder.crc_errors, "discarded_bytes": decoder.discarded_bytes,
                             "first_device_index": recorder.first_index, "error": error,
                             "pc_end_utc": datetime.now(timezone.utc).isoformat()})
            path.with_suffix(".json").write_text(json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8")
            print(f"Saved {recorder.total} points: {path}")
    return 1 if error else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except OSError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        sys.exit(1)
