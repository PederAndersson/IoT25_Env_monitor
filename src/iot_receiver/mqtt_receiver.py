import os
import paho.mqtt.client as mqtt
import json
import logging
import sqlite3
from pathlib import Path

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s"
)
logger = logging.getLogger(__name__)

PROJECT_ROOT = Path(__file__).resolve().parents[2]
DATABASE_PATH = PROJECT_ROOT / "data" / "readings.db"
CA_CERT_PATH = PROJECT_ROOT / "certs" / "ca.crt"

def initialize_database():
    DATABASE_PATH.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(DATABASE_PATH)
    cursor = connection.cursor()
    sql = """
    CREATE TABLE IF NOT EXISTS readings(
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        sensor_id TEXT NOT NULL,
        timestamp TEXT NOT NULL,
        value REAL NOT NULL,
        unit TEXT NOT NULL
    )
    """
    cursor.execute(sql)
    connection.commit()
    connection.close()

def save_reading(reading):
    connection = sqlite3.connect(DATABASE_PATH)
    cursor = connection.cursor()
    sql = """
    INSERT INTO readings(sensor_id, timestamp, value, unit)
    VALUES(?, ?, ?, ?)
    """
    values = (reading["sensorId"], reading["timestamp"], reading["value"], reading["unit"])
    cursor.execute(sql, values)
    connection.commit()
    connection.close()

def decode_payload(payload_bytes):
    reading = json.loads(payload_bytes)
    return reading

required_fields = ["sensorId", "timestamp", "value", "unit"]

def validate_reading(reading):
    for field in required_fields:
        if field not in reading:
            return False, f"Payload is missing {field}"
    if not isinstance(reading["timestamp"], str):
        return False, "timestamp not a string"
    return True, "Payload validated"

def process_payload(payload_bytes):
    try:
        reading = decode_payload(payload_bytes)
    except json.JSONDecodeError:
        logger.warning("Payload contains invalid JSON")
        return
    is_valid, reason = validate_reading(reading)
    if is_valid:
        logger.info(f"{reason}: {reading}")
        save_reading(reading)
    else:
        logger.warning(reason)


def on_message(client, userdata, message):
    logger.info(f"Received message on topic: {message.topic}")
    process_payload(message.payload)

def on_connect(client, userdata, connect_flags, reason_code, properties):
    if reason_code == 0:
        logger.info("Connected to MQTT broker")
        client.subscribe("building/room-a/climate/#")
    else:
        logger.warning(f"Connection failed: {reason_code}")

def on_disconnect(client, userdata, disconnect_flags, reason_code, properties):
    if reason_code == 0:
        logger.info("Disconnected from MQTT broker")
    else:
        logger.warning(f"Connection to MQTT broker lost: {reason_code}")

def main():
    initialize_database()
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set(
        os.getenv("MQTT_USERNAME"),
        os.getenv("MQTT_PASSWORD")
    )
    broker = os.getenv("MQTT_BROKER", "100.84.116.70")
    port = int(os.getenv("MQTT_PORT", "8883"))
    ca_cert_path = os.getenv("MQTT_CA_PATH", str(CA_CERT_PATH))
    client.tls_set(ca_certs=ca_cert_path)
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_disconnect = on_disconnect
    client.connect(broker, port, 60)
    client.loop_forever()

if __name__ == "__main__":
    main()