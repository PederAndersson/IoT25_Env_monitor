from copy import deepcopy

import pytest

from src.iot_receiver import api, mqtt_receiver


@pytest.fixture
def valid_reading():
    return {
        "sensorId": "esp32-aabbccddeeff",
        "timestamp": "2026-10-03T12:34:56Z",
        "humidity_value": 45.0,
        "humidity_unit": "%",
        "temperature_value": 21.5,
        "temperature_unit": "C",
    }


@pytest.fixture
def reading_factory(valid_reading):
    def make_reading(**changes):
        reading = deepcopy(valid_reading)
        reading.update(changes)
        return reading

    return make_reading


@pytest.fixture
def temporary_database(tmp_path, monkeypatch):
    database_path = tmp_path / "readings.db"
    monkeypatch.setattr(mqtt_receiver, "DATABASE_PATH", database_path)
    monkeypatch.setattr(api, "DATABASE_PATH", database_path)
    mqtt_receiver.initialize_database()
    return database_path
