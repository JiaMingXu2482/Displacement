"""S32K144 位移传感器标定工具.

固件通过 921600 8N1 输出 ASCII 文本，每行一个采样点，单位 0.1 mm。
本脚本长时间采集并统计，用于两点标定。

用法:
    python tools/calibrate_displacement.py --port COM4 --seconds 5

标定流程:
    1. 把传感器拉到机械零点，运行本脚本，记下 mean_raw
    2. 拉到已知距离（例如 500 mm），再运行一次，记下 mean_raw
    3. 用两组数据算出 RAW_ZERO / RAW_FULL，写入 displacement.h
"""

import argparse
import statistics
import sys
import time

import serial

ADC_MAX = 4095.0
VREF = 3.3
SHUNT = 100.0
RANGE_MM = 1500.0
# 必须与固件 displacement.h 中的标定常数一致，否则反算的 ADC 码不准。
RAW_ZERO = 496
RAW_FULL = 2482


def tenth_mm_to_raw(position_tenth_mm: float) -> float:
    """按固件当前的两点标定公式，把 0.1 mm 输出反算成 ADC 原始码."""
    return position_tenth_mm * (RAW_FULL - RAW_ZERO) / (RANGE_MM * 10.0) + RAW_ZERO


def raw_to_current_ma(raw: float) -> float:
    return raw * VREF / ADC_MAX / SHUNT * 1000.0


def main() -> int:
    ap = argparse.ArgumentParser(description="位移数据统计与标定")
    ap.add_argument("--port", default="COM4")
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument("--seconds", type=float, default=5.0)
    ap.add_argument("--known-mm", type=float, default=None,
                    help="当前位置的真实距离（mm），给出后会反算标定常数")
    args = ap.parse_args()

    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except Exception as exc:  # noqa: BLE001
        print(f"无法打开 {args.port}: {exc}")
        print("可用串口:", [p.device for p in serial.tools.list_ports.comports()])
        return 1

    ser.reset_input_buffer()
    values: list[int] = []
    deadline = time.time() + args.seconds
    print(f"采集 {args.seconds:.1f} 秒，来自 {args.port} @ {args.baud} ...")

    while time.time() < deadline:
        line = ser.readline()
        if not line:
            continue
        text = line.strip()
        if not text or text.startswith(b"-") and len(text) == 1:
            continue
        try:
            values.append(int(text))
        except ValueError:
            continue

    ser.close()

    if not values:
        print("没有解析到数据。检查固件是否已下载、串口是否被占用。")
        return 1

    mean = statistics.fmean(values)
    stdev = statistics.pstdev(values) if len(values) > 1 else 0.0
    raw = tenth_mm_to_raw(mean)
    current = raw_to_current_ma(raw)

    print(f"\n样本数      : {len(values)}")
    print(f"平均        : {mean:8.2f} (0.1 mm)  = {mean / 10.0:7.2f} mm")
    print(f"标准差      : {stdev:8.2f} (0.1 mm)  = {stdev / 10.0:7.2f} mm")
    print(f"最小/最大   : {min(values)} / {max(values)}")
    print(f"峰峰值      : {max(values) - min(values)} (0.1 mm)")
    print(f"等效 ADC 码 : {raw:8.1f}")
    print(f"等效电流    : {current:8.3f} mA")

    if args.known_mm is not None and args.known_mm >= 0:
        print(f"\n真实位置    : {args.known_mm:.2f} mm  →  平均 ADC 码 {raw:.1f}")
        print("把这个 ADC 码记为一个标定点。")
        print("两点都测好后，按下面公式算出新常数：")
        print("  RAW_ZERO = code_A - (code_B - code_A) * pos_A / (pos_B - pos_A)")
        print("  RAW_FULL = RAW_ZERO + (code_B - code_A) * RANGE / (pos_B - pos_A)")

    print("\n提示：标准差若超过 100（10 mm），说明信号噪声大，需要更多样本或加滤波。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
