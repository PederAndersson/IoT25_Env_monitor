# Automated test suite

The suite verifies the Python receiver's data contract, SQLite persistence,
FastAPI endpoints, and MQTT configuration without using a real network or
project data.

## Installation

Run these commands from the project root. They create a local virtual
environment and install test dependencies there; they do not change
production data.

```bash
python -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements-dev.txt
```

## Running tests

Run the full suite:

```bash
python -m pytest
```

Run one part of the suite by naming its test file:

```bash
python -m pytest tests/test_validation.py
python -m pytest tests/test_database.py
python -m pytest tests/test_api.py
python -m pytest tests/test_mqtt_receiver.py
```

The `-q` flag produces shorter output, while `-v` displays every test name:

```bash
python -m pytest -q
python -m pytest -v
```

## Isolation

- Each database test uses a new database in pytest's temporary directory.
- The receiver and API functions are pointed at the same temporary database.
- FastAPI's constructed query model is tested directly for `limit` validation;
  actual HTTP statuses are covered by the manual local API test.
- A fake MQTT client replaces the real one in the startup test.
- No test reads `.env`, contacts the broker, or uses the ESP32.
- The checksum of `data/readings.db` should therefore be unchanged before
  and after a test run.

Manual firmware, TLS, Compose, and end-to-end tests are documented in
`testprotokoll.md`.
