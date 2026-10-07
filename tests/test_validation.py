import logging

import pytest

from src.iot_receiver import mqtt_receiver


def test_valid_firmware_payload_is_accepted(valid_reading):
    is_valid, reason = mqtt_receiver.validate_reading(valid_reading)

    assert is_valid is True
    assert reason == "Payload validated"


@pytest.mark.parametrize("invalid_reading", [None, [], "reading", 42])
def test_non_object_payload_is_rejected(invalid_reading):
    is_valid, _ = mqtt_receiver.validate_reading(invalid_reading)

    assert is_valid is False


@pytest.mark.parametrize("missing_field", mqtt_receiver.required_fields)
def test_missing_required_field_is_rejected(valid_reading, missing_field):
    del valid_reading[missing_field]

    is_valid, reason = mqtt_receiver.validate_reading(valid_reading)

    assert is_valid is False
    assert missing_field in reason


@pytest.mark.parametrize("sensor_id", [None, 123, "", "   "])
def test_invalid_sensor_id_is_rejected(reading_factory, sensor_id):
    is_valid, _ = mqtt_receiver.validate_reading(
        reading_factory(sensorId=sensor_id)
    )

    assert is_valid is False


@pytest.mark.parametrize(
    "timestamp",
    ["2026-10-03 12:34:56", "not-a-timestamp", "", 123],
)
def test_invalid_or_timezone_naive_timestamp_is_rejected(
    reading_factory, timestamp
):
    is_valid, _ = mqtt_receiver.validate_reading(
        reading_factory(timestamp=timestamp)
    )

    assert is_valid is False


@pytest.mark.parametrize(
    "timestamp",
    ["2026-10-03T12:34:56Z", "2026-10-03T14:34:56+02:00"],
)
def test_timestamp_with_timezone_is_accepted(reading_factory, timestamp):
    is_valid, _ = mqtt_receiver.validate_reading(
        reading_factory(timestamp=timestamp)
    )

    assert is_valid is True


@pytest.mark.parametrize(
    ("field", "value"),
    [
        ("humidity_value", 45),
        ("humidity_value", "45.0"),
        ("temperature_value", 21),
        ("temperature_value", "21.0"),
        ("humidity_unit", 1),
        ("humidity_unit", "percent"),
        ("temperature_unit", 1),
        ("temperature_unit", "F"),
    ],
)
def test_invalid_measurement_type_or_unit_is_rejected(
    reading_factory, field, value
):
    is_valid, _ = mqtt_receiver.validate_reading(
        reading_factory(**{field: value})
    )

    assert is_valid is False


@pytest.mark.parametrize(
    ("field", "value"),
    [
        ("humidity_value", 20.0),
        ("humidity_value", 90.0),
        ("temperature_value", 0.0),
        ("temperature_value", 50.0),
    ],
)
def test_measurement_boundary_is_accepted(reading_factory, field, value):
    is_valid, _ = mqtt_receiver.validate_reading(
        reading_factory(**{field: value})
    )

    assert is_valid is True


@pytest.mark.parametrize(
    ("field", "value"),
    [
        ("humidity_value", 19.9),
        ("humidity_value", 90.1),
        ("temperature_value", -0.1),
        ("temperature_value", 50.1),
    ],
)
def test_measurement_outside_range_is_rejected(reading_factory, field, value):
    is_valid, _ = mqtt_receiver.validate_reading(
        reading_factory(**{field: value})
    )

    assert is_valid is False


def test_invalid_json_is_logged_and_not_saved(monkeypatch, caplog):
    saved_readings = []
    monkeypatch.setattr(mqtt_receiver, "save_reading", saved_readings.append)

    with caplog.at_level(logging.WARNING):
        mqtt_receiver.process_payload(b'{"sensorId":')

    assert saved_readings == []
    assert "invalid JSON" in caplog.text


def test_invalid_reading_is_logged_and_not_saved(
    reading_factory, monkeypatch, caplog
):
    saved_readings = []
    monkeypatch.setattr(mqtt_receiver, "save_reading", saved_readings.append)

    with caplog.at_level(logging.WARNING):
        mqtt_receiver.process_payload(
            mqtt_receiver.json.dumps(
                reading_factory(temperature_value=75.0)
            ).encode()
        )

    assert saved_readings == []
    assert "Temperature value invalid" in caplog.text


def test_valid_reading_is_saved(valid_reading, monkeypatch):
    saved_readings = []
    monkeypatch.setattr(mqtt_receiver, "save_reading", saved_readings.append)

    mqtt_receiver.process_payload(
        mqtt_receiver.json.dumps(valid_reading).encode()
    )

    assert saved_readings == [valid_reading]


def test_duplicate_reading_is_logged(valid_reading, monkeypatch, caplog):
    monkeypatch.setattr(mqtt_receiver, "save_reading", lambda reading: False)

    with caplog.at_level(logging.INFO):
        mqtt_receiver.process_payload(
            mqtt_receiver.json.dumps(valid_reading).encode()
        )

    assert "Duplicate reading ignored" in caplog.text
