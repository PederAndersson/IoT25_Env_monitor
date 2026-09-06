import os
import paho.mqtt.client as mqtt
import json

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
        print("Payload contains invalid JSON")
        return
    is_valid, reason = validate_reading(reading)
    if is_valid:
        print(reason)
        print(reading)
    else:
        print(reason)

def on_message(client, userdata, message):
    print(message.topic)
    process_payload(message.payload)

def on_connect(client, userdata, connect_flags, reason_code, properties):
    if reason_code == 0:
        print("Connected to MQTT broker")
        client.subscribe("building/room-a/climate/#")
    else:
        print(f"Connection failed: {reason_code}")

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.username_pw_set(
    os.getenv("MQTT_USERNAME"),
    os.getenv("MQTT_PASSWORD")
)
client.on_connect = on_connect
client.on_message = on_message

client.connect("bengt", 1883, 60)
client.loop_forever()
test_payload1 = b'{"sensorId":"temp-01","timestamp":"2026-09-03T14:15:00+02:00","value":21.7,"unit":"C"}'
test_payload2 = b'{"sensorId":"temp-01","timestamp":14.15,"value":21.7,"unit":"C"}'
test_payload3 = b'{"sensorId":"temp-01","value":21.7,"unit":"C"'

process_payload(test_payload1)
process_payload(test_payload2)
process_payload(test_payload3)