import os
import paho.mqtt.client as mqtt
import json
import logging
import sqlite3
from pathlib import Path
from datetime import datetime


logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s"
)
logger = logging.getLogger(__name__)

PROJECT_ROOT = Path(__file__).resolve().parents[2]
DATABASE_PATH = PROJECT_ROOT / "data" / "readings.db"
CA_CERT_PATH = PROJECT_ROOT / "certs" / "ca.crt"
TELEMETRY_TOPIC_FILTER = "esp-test/+/telemetry"

def initialize_database():
    DATABASE_PATH.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(DATABASE_PATH)
    cursor = connection.cursor()
    sql = """
    CREATE TABLE IF NOT EXISTS readings(
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        sensor_id TEXT NOT NULL,
        timestamp TEXT NOT NULL,
        humidity_value REAL NOT NULL,
        humidity_unit TEXT NOT NULL,
        temperature_value REAL NOT NULL,
        temperature_unit TEXT NOT NULL
    )
    """
    cursor.execute(sql)
    connection.commit()
    connection.close()

def save_reading(reading):
    connection = sqlite3.connect(DATABASE_PATH)
    cursor = connection.cursor()
    sql = """
    INSERT INTO readings(sensor_id, timestamp, humidity_value, humidity_unit, temperature_value, temperature_unit)
    SELECT ?, ?, ?, ?, ?, ?
    WHERE NOT EXISTS (
        SELECT 1 FROM readings
        WHERE sensor_id = ? AND timestamp = ?
    )
    """
    values = (
        reading["sensorId"],
        reading["timestamp"],
        reading["humidity_value"],
        reading["humidity_unit"],
        reading["temperature_value"],
        reading["temperature_unit"],
        reading["sensorId"],
        reading["timestamp"],
    )
    cursor.execute(sql, values)
    was_inserted = cursor.rowcount == 1
    connection.commit()
    connection.close()
    return was_inserted

def decode_payload(payload_bytes):
    reading = json.loads(payload_bytes)
    return reading

def is_valid_timestamp(timestamp):
    try:
        parsed_timestamp = datetime.fromisoformat(timestamp)
        return (
            parsed_timestamp.tzinfo is not None
            and parsed_timestamp.utcoffset() is not None
        )
    except (TypeError, ValueError):
        return False

required_fields = ["sensorId", "timestamp", "humidity_value", "humidity_unit", "temperature_value", "temperature_unit"]

def validate_reading(reading):
    if not isinstance(reading, dict):
        return False, "Reading not a python object"
    for field in required_fields:
        if field not in reading:
            return False, f"Payload is missing {field}"
    if not isinstance(reading["sensorId"], str) or not reading["sensorId"].strip():
        return False, "Sensor id must be a non-empty string"
    if not isinstance(reading["timestamp"], str):
        return False, "Timestamp must be a string"
    if is_valid_timestamp(reading["timestamp"]) != True:
        return False, "Timestamp must be ISO 8601 with a timezone"
    if not isinstance(reading["humidity_value"], float):
        return False, "Humidity value must be a float"
    if (reading["humidity_value"] > 90 or reading["humidity_value"] < 20):
        return False, f"Humidity value invalid {reading["humidity_value"]}"
    if not isinstance(reading["humidity_unit"], str):
        return False, "Humidity unit must be a string"
    if reading["humidity_unit"] != "%":
        return False, "Humidity unit must be a %"
    if not isinstance(reading["temperature_value"], float):
        return False, "Temperature value must be a float"
    if (reading["temperature_value"] > 50 or reading["temperature_value"] < 0):
        return False, f"Temperature value invalid {reading["temperature_value"]}"
    if not isinstance(reading["temperature_unit"], str):
        return False, "Temperature unit must be a string"
    if reading["temperature_unit"] != "C":
        return False, "Temperature unit must be a C"
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
        if not save_reading(reading):
            logger.info(
                "Duplicate reading ignored: sensorId=%s timestamp=%s",
                reading["sensorId"],
                reading["timestamp"],
            )
    else:
        logger.warning(reason)


def is_telemetry_topic(topic):
    parts = topic.split("/")
    return (
        len(parts) == 3
        and parts[0] == "esp-test"
        and bool(parts[1])
        and parts[2] == "telemetry"
    )


def on_message(client, userdata, message):
    if not is_telemetry_topic(message.topic):
        logger.info(
            "Ignoring non-telemetry MQTT message on topic: %s",
            message.topic,
        )
        return
    logger.info(
        "Received telemetry on topic: %s qos=%s dup=%s mid=%s",
        message.topic,
        message.qos,
        message.dup,
        message.mid,
    )
    process_payload(message.payload)

def on_connect(client, userdata, connect_flags, reason_code, properties):
    if reason_code == 0:
        logger.info("Connected to MQTT broker")
        result, _ = client.subscribe(TELEMETRY_TOPIC_FILTER, qos=1)
        if result != mqtt.MQTT_ERR_SUCCESS:
            logger.error("MQTT telemetry subscription failed: %s", result)
    else:
        logger.warning(f"Connection failed: {reason_code}")

def on_disconnect(client, userdata, disconnect_flags, reason_code, properties):
    if reason_code == 0:
        logger.info("Disconnected from MQTT broker")
    else:
        logger.warning(f"Connection to MQTT broker lost: {reason_code}")

def main():
    broker = os.getenv("MQTT_BROKER")
    port = os.getenv("MQTT_PORT")
    if not broker or not port:
        logger.error("Env variable missing")
        return 1
    else:
        try:
            port = int(port)
        except ValueError:
            logger.error("MQTT_PORT must be an integer")
            return 1
        if port < 1 or port > 65535:
            logger.error("Invalid port value, must be within 1-65535")
            return 1
        initialize_database()
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        client.username_pw_set(
            os.getenv("MQTT_USERNAME"),
            os.getenv("MQTT_PASSWORD")
        )
        ca_cert_path = os.getenv("MQTT_CA_PATH", str(CA_CERT_PATH))
        client.tls_set(ca_certs=ca_cert_path)
        client.on_connect = on_connect
        client.on_message = on_message
        client.on_disconnect = on_disconnect
        client.connect(broker, port, 60)
        client.loop_forever()

if __name__ == "__main__":
    raise SystemExit(main())
