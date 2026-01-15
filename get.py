# subscriber_log.py
import argparse
from pathlib import Path
import paho.mqtt.client as mqtt
import uuid

DEFAULT_HOST = "mqtt.abcsolutions.com.vn"
DEFAULT_PORT = 1883
DEFAULT_USER = "abcsolution"
DEFAULT_PASS = "CseLAbC5c6"
DEFAULT_TOPIC = "duy/sensorDetection1"
DEFAULT_OUT = "output/detections.csv"

CSV_HEADERS = [
    "Time",
    "Temperature",
    "Humidity",
    "Humidity_WeatherStation",
    "Temperature_WeatherStation",
    "Label",
    "Feature Extraction Time (ms)",
    "Testing Time (ms)",
    "Score",
]

def ensure_header(out_path: Path):
    if not out_path.exists() or out_path.stat().st_size == 0:
        with open(out_path, "w", encoding="utf-8", newline="") as f:
            f.write(",".join(CSV_HEADERS) + "\n")

def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print(f"[MQTT] Connected to {userdata['host']}:{userdata['port']}")
        client.subscribe(userdata["topic"], qos=0)
        print(f"[MQTT] Subscribed: {userdata['topic']}")
    else:
        print(f"[MQTT] Connect failed, reason_code={reason_code}")

# NOTE: tương thích cả v1 (5 tham số) lẫn v2 (4 tham số) bằng cách chấp nhận flags
def on_disconnect(client, userdata, flags, reason_code, properties=None):
    print(f"[MQTT] Disconnected (rc={reason_code}). Reconnecting...")

def on_message(client, userdata, msg):
    line = msg.payload.decode("utf-8", errors="replace").strip()
    print(line)
    out_path: Path = userdata["out_path"]
    try:
        if not line.endswith("\n"):
            line += "\n"
        with open(out_path, "a", encoding="utf-8", newline="") as f:
            f.write(line)
    except Exception as e:
        print(f"[ERR] Cannot write to {out_path}: {e}")

def build_client(args, out_path: Path):
    # Use unique client ID to prevent conflicts
    unique_id = f"det_subscriber_{uuid.uuid4().hex[:8]}"
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=unique_id)
    client.username_pw_set(args.username, args.password)
    client.user_data_set({
        "host": args.host,
        "port": args.port,
        "topic": args.topic,
        "out_path": out_path,
    })
    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message
    client.reconnect_delay_set(min_delay=1, max_delay=30)
    return client

def parse_args():
    p = argparse.ArgumentParser(description="Subscribe duy/sensorDetection and append to CSV.")
    p.add_argument("--host", default=DEFAULT_HOST)
    p.add_argument("--port", type=int, default=DEFAULT_PORT)
    p.add_argument("--username", default=DEFAULT_USER)
    p.add_argument("--password", default=DEFAULT_PASS)
    p.add_argument("--topic", default=DEFAULT_TOPIC)
    p.add_argument("--out", default=DEFAULT_OUT, help="Output CSV path (default: detections.csv)")
    return p.parse_args()

def main():
    args = parse_args()
    out_path = Path(args.out).expanduser().resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    ensure_header(out_path)

    client = build_client(args, out_path)
    try:
        client.connect(args.host, args.port, keepalive=60)
        client.loop_forever(retry_first_connection=True)
    except KeyboardInterrupt:
        print("\n[INFO] Stopped by user.")
    finally:
        try:
            client.disconnect()
        except Exception:
            pass

if __name__ == "__main__":
    main()
