import csv
import json
import time
import os
import sys
import platform
import subprocess
import shutil
from pathlib import Path
import paho.mqtt.client as mqtt

# =========================
# MQTT CONFIG
# =========================
MQTT_HOST = "mqtt.abcsolutions.com.vn"
MQTT_PORT = 1883
MQTT_USERNAME = "abcsolution"
MQTT_PASSWORD = "CseLAbC5c6"
MQTT_TOPIC_PUB = "duy/sensorFault1"

# =========================
# CSV CONFIG
# =========================
CSV_PATH = Path("dataset") / "Test-set_1.csv"
PUBLISH_INTERVAL_SEC = 0.25  # delay giữa các dòng

# =========================
# Sound helper (cross-platform)
# =========================
def beep_done():
    """
    Phát âm thanh 'done' theo cách phù hợp với hệ điều hành:
    - Windows: winsound (MessageBeep / Beep)
    - macOS:   afplay hệ thống
    - Linux:   paplay/aplay nếu có sẵn
    - Fallback: ký tự bell '\a'
    """
    try:
        system = platform.system()
        if system == "Windows":
            try:
                import winsound
                try:
                    winsound.MessageBeep()  # âm mặc định
                except RuntimeError:
                    winsound.Beep(1000, 400)  # tần số 1000 Hz, 400 ms
                return
            except Exception:
                # nếu winsound lỗi, rơi xuống các phương án khác
                pass

        # macOS: dùng afplay + âm mặc định của hệ thống
        if system == "Darwin":
            if shutil.which("afplay"):
                snd = "/System/Library/Sounds/Glass.aiff"
                try:
                    subprocess.Popen(
                        ["afplay", snd],
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                    )
                    return
                except Exception:
                    pass

        # Linux: thử paplay (PulseAudio) rồi đến aplay (ALSA)
        if system == "Linux":
            candidates = [
                ("paplay", "/usr/share/sounds/freedesktop/stereo/complete.oga"),
                ("aplay", "/usr/share/sounds/alsa/Front_Center.wav"),
            ]
            for cmd, path in candidates:
                if shutil.which(cmd) and os.path.exists(path):
                    try:
                        subprocess.Popen(
                            [cmd, path],
                            stdout=subprocess.DEVNULL,
                            stderr=subprocess.DEVNULL,
                        )
                        return
                    except Exception:
                        continue
    except Exception:
        pass

    # Fallback: ký tự bell (nhiều terminal sẽ kêu beep)
    try:
        print("\a", end="", flush=True)
    except Exception:
        pass

# =========================
# MQTT callbacks (API v2)
# =========================
def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print(f"[MQTT] Connected to {MQTT_HOST}:{MQTT_PORT}")
    else:
        print(f"[MQTT] Connect failed, reason_code={reason_code}")

def build_client():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    client.on_connect = on_connect
    client.connect(MQTT_HOST, MQTT_PORT, keepalive=60)
    client.loop_start()
    return client

def read_rows(csv_path: Path):
    if not csv_path.exists():
        raise FileNotFoundError(f"CSV not found: {csv_path.resolve()}")

    with open(csv_path, "r", encoding="utf-8-sig", newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise ValueError("CSV has no header row.")
        reader.fieldnames = [fn.replace("\ufeff", "").strip() for fn in reader.fieldnames]

        required = [
            "Time",
            "Temperature",
            "Humidity",
            "Humidity_WeatherStation",
            "Temperature_WeatherStation",
        ]
        missing = [c for c in required if c not in reader.fieldnames]
        if missing:
            raise KeyError(f"Thiếu cột: {missing}. Header: {reader.fieldnames}")

        for idx, row in enumerate(reader, start=2):
            try:
                yield {
                    "Time": str(row["Time"]).strip(),
                    "Temperature": float(str(row["Temperature"]).strip()),
                    "Humidity": float(str(row["Humidity"]).strip()),
                    "Humidity_WeatherStation": float(str(row["Humidity_WeatherStation"]).strip()),
                    "Temperature_WeatherStation": float(str(row["Temperature_WeatherStation"]).strip()),
                }
            except Exception as e:
                print(f"[WARN] Lỗi ép kiểu ở dòng {idx}: {e}. Row={row}")
                continue

def main():
    client = build_client()
    try:
        sent = False
        for payload in read_rows(CSV_PATH):
            sent = True
            msg = json.dumps(payload, ensure_ascii=False)
            client.publish(MQTT_TOPIC_PUB, msg, qos=0, retain=False)
            print(f"[PUB] {MQTT_TOPIC_PUB} <- {msg}")
            time.sleep(PUBLISH_INTERVAL_SEC)

        # cho broker có thời gian flush (nhất là khi kết nối chậm)
        time.sleep(0.2)

        if not sent:
            print("[INFO] CSV trống hoặc không có dòng hợp lệ.")

    except KeyboardInterrupt:
        print("\n[INFO] Stopped by user.")
    finally:
        client.loop_stop()
        client.disconnect()
        print("[MQTT] Disconnected.")
        beep_done()  # <— phát tiếng khi chạy xong

if __name__ == "__main__":
    main()
