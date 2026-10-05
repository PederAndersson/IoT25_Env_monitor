import pytest
from fastapi import HTTPException

from src.iot_receiver import api, mqtt_receiver


def get_limit_field():
    route = next(
        route
        for route in api.app.routes
        if route.path == "/api/v1/readings"
    )
    return route.dependant.query_params[0]


def test_health_endpoint():
    assert api.health() == {"status": "ok"}


def test_latest_returns_404_when_database_is_empty(temporary_database):
    with pytest.raises(HTTPException) as raised:
        api.latest_reading()

    assert raised.value.status_code == 404
    assert raised.value.detail == "Reading not available."

def test_latest_returns_newest_reading(
    temporary_database, valid_reading, reading_factory
):
    mqtt_receiver.save_reading(valid_reading)
    newest = reading_factory(
        timestamp="2026-10-03T12:35:56Z", temperature_value=22.5
    )
    mqtt_receiver.save_reading(newest)

    response = api.latest_reading()

    assert response["temperature_value"] == 22.5
    assert response["timestamp"] == newest["timestamp"]


def test_history_is_newest_first_and_respects_limit(
    temporary_database, valid_reading, reading_factory
):
    mqtt_receiver.save_reading(valid_reading)
    mqtt_receiver.save_reading(
        reading_factory(
            timestamp="2026-10-03T12:35:56Z", temperature_value=22.5
        )
    )

    response = api.get_readings(limit=1)

    assert len(response) == 1
    assert response[0]["temperature_value"] == 22.5


def test_history_default_returns_available_readings(
    temporary_database, valid_reading
):
    mqtt_receiver.save_reading(valid_reading)

    response = api.get_readings(limit=100)

    assert len(response) == 1


@pytest.mark.parametrize(("raw_limit", "expected"), [("1", 1), ("100", 100)])
def test_history_accepts_limit_boundaries(raw_limit, expected):
    value, errors = get_limit_field().validate(
        raw_limit, {}, loc=("query", "limit")
    )

    assert errors == []
    assert value == expected


@pytest.mark.parametrize("invalid_limit", ["0", "101", "not-a-number"])
def test_history_rejects_invalid_limit(invalid_limit):
    value, errors = get_limit_field().validate(
        invalid_limit, {}, loc=("query", "limit")
    )

    assert value is None
    assert errors


def test_status_returns_stored_reading_count(
    temporary_database, valid_reading, reading_factory
):
    mqtt_receiver.save_reading(valid_reading)
    mqtt_receiver.save_reading(
        reading_factory(timestamp="2026-10-03T12:35:56Z")
    )

    response = api.get_status()

    assert response == {"storedReadings": 2}
