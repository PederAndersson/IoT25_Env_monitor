import sqlite3

from src.iot_receiver import mqtt_receiver


def test_database_initialization_is_idempotent(temporary_database):
    mqtt_receiver.initialize_database()

    with sqlite3.connect(temporary_database) as connection:
        table = connection.execute(
            "SELECT name FROM sqlite_master WHERE type = 'table' AND name = ?",
            ("readings",),
        ).fetchone()

    assert table == ("readings",)


def test_reading_is_persisted(temporary_database, valid_reading):
    was_inserted = mqtt_receiver.save_reading(valid_reading)

    with sqlite3.connect(temporary_database) as connection:
        stored = connection.execute(
            "SELECT sensor_id, timestamp, humidity_value, humidity_unit, "
            "temperature_value, temperature_unit FROM readings"
        ).fetchone()

    assert stored == (
        valid_reading["sensorId"],
        valid_reading["timestamp"],
        valid_reading["humidity_value"],
        valid_reading["humidity_unit"],
        valid_reading["temperature_value"],
        valid_reading["temperature_unit"],
    )
    assert was_inserted is True


def test_duplicate_sensor_and_timestamp_is_not_persisted_twice(
    temporary_database, valid_reading
):
    first_insert = mqtt_receiver.save_reading(valid_reading)
    duplicate_insert = mqtt_receiver.save_reading(valid_reading)

    with sqlite3.connect(temporary_database) as connection:
        row_count = connection.execute(
            "SELECT COUNT(*) FROM readings"
        ).fetchone()[0]

    assert first_insert is True
    assert duplicate_insert is False
    assert row_count == 1


def test_same_timestamp_from_different_sensors_is_persisted(
    temporary_database, valid_reading, reading_factory
):
    mqtt_receiver.save_reading(valid_reading)
    mqtt_receiver.save_reading(
        reading_factory(sensorId="esp32-112233445566")
    )

    with sqlite3.connect(temporary_database) as connection:
        row_count = connection.execute(
            "SELECT COUNT(*) FROM readings"
        ).fetchone()[0]

    assert row_count == 2


def test_external_input_is_stored_as_data_not_sql(
    temporary_database, reading_factory
):
    sensor_id = "sensor'); DROP TABLE readings; --"
    mqtt_receiver.save_reading(reading_factory(sensorId=sensor_id))

    with sqlite3.connect(temporary_database) as connection:
        stored_sensor_id = connection.execute(
            "SELECT sensor_id FROM readings"
        ).fetchone()[0]
        row_count = connection.execute(
            "SELECT COUNT(*) FROM readings"
        ).fetchone()[0]

    assert stored_sensor_id == sensor_id
    assert row_count == 1
