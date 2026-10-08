# Secure and integrated IoT solution

This project reads temperature and humidity from a DHT11 connected to an
ESP32-C6. The device sends the readings as JSON over MQTT with TLS. A local
Python service validates and stores them in SQLite, and a FastAPI service
makes them available over HTTP.

These instructions cover local development and testing. Raspberry Pi
deployment is outside the scope of this repository.

## Data flow

```text
DHT11 -> ESP32-C6 -> MQTT over TLS -> Python receiver -> SQLite -> FastAPI
```

MQTT messages use this data contract:

```json
{
  "sensorId": "esp32-aabbccddeeff",
  "timestamp": "2026-10-03T12:34:56Z",
  "humidity_value": 45.0,
  "humidity_unit": "%",
  "temperature_value": 21.5,
  "temperature_unit": "C"
}
```

## Local requirements

- Python 3.13 or a compatible later version
- Docker Engine with Docker Compose for container operation
- ESP-IDF 6.0 to build the firmware
- An ESP32-C6 and DHT11 for hardware tests
- Your own MQTT broker that accepts ordinary MQTT over TLS and allows the
  clients to use `esp-test/<device-id>/`

## Python environment

The following commands create a local virtual environment and install the
production dependencies. They only change `.venv/`, which Git ignores.

```bash
python -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
```

Install the development dependencies in the same environment to run the test
suite:

```bash
python -m pip install -r requirements-dev.txt
```

## Local configuration

Set up your own MQTT broker with TLS and a username/password before starting
the clients. Choose a hostname that appears in the broker certificate's
subject alternative names (SAN) and is reachable by both the ESP32 and the
receiver. Copy the example configuration and replace the placeholders with
your broker's hostname, TLS port, and credentials:

```bash
cp .env.example .env
```

`.env` should contain:

- `MQTT_BROKER`: the hostname in the broker certificate;
- `MQTT_PORT`: the TLS port chosen for your broker; `8883` in
  `.env.example` is only an example;
- `MQTT_USERNAME` and `MQTT_PASSWORD`: local MQTT credentials;
- `MQTT_CA_PATH`: a path to a CA file that trusts your broker's certificate
  chain. If unset, the receiver uses `certs/ca.crt`, which works only when
  that CA issued the broker certificate. For a publicly trusted certificate,
  you can specify the environment's system CA bundle. With Compose, the path
  must exist inside the container;
- `HOST_UID` and `HOST_GID`: the local user's user and group IDs. The default
  `1000` suits many Linux installations. Check your values with `id -u` and
  `id -g`.

Compose runs both services with these IDs so that SQLite in the bind-mounted
`data/` directory remains writable from both the containers and a local
Python process. If either ID differs from `1000`, set it in `.env`.

Set the same hostname and port in the ESP32 MQTT URI through
`idf.py menuconfig`. The firmware uses the ESP-IDF CA bundle, so that bundle
must trust the broker certificate. A private CA requires adapting the
firmware configuration. Git ignores `.env`. Never add real passwords or
private keys to tracked files.

## Start the Python services locally

First activate `.venv`. Then start the MQTT receiver in one terminal:

```bash
set -a
source .env
set +a
python -m src.iot_receiver.mqtt_receiver
```

`set -a` exports variables read from `.env` to the process. `set +a` turns
automatic export off again. The command starts the receiver, creates the
SQLite table if needed, and waits for telemetry on `esp-test/+/telemetry`
with QoS 1. Status messages do not reach telemetry validation. An existing
combination of `sensorId` and `timestamp` is ignored.

Start the API in a second terminal from the project root:

```bash
source .venv/bin/activate
python -m uvicorn src.iot_receiver.api:app --host 127.0.0.1 --port 8000
```

The API is then available at `http://127.0.0.1:8000`.

## Run locally with Docker Compose

The following command builds the image and starts the API and MQTT receiver.
It creates containers and writes local runtime data under `data/`.

```bash
docker compose up --build -d
```

Check service status and logs with:

```bash
docker compose ps
docker compose logs api mqtt_receiver
```

Stop the local containers without deleting the SQLite file:

```bash
docker compose down
```

## Build the ESP32 firmware locally

Open a terminal with ESP-IDF 6.0 installed. If the environment is not yet
loaded, run the installation's `export.sh`. `IDF_PATH` must point to your
local ESP-IDF installation.

```bash
source "$IDF_PATH/export.sh"
cd firmware/esp32
idf.py set-target esp32c6
idf.py menuconfig
idf.py build
```

`set-target` and `menuconfig` create or modify the local, Git-ignored
`sdkconfig` file. Enter WiFi settings, the MQTT URI, username, and password
there. `idf.py build` compiles and links the firmware but does not verify the
sensor or network communication.

With the ESP32-C6 connected, flash the firmware and open the serial monitor:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Adjust the serial port for your computer. Exit the monitor with `Ctrl+]`.

## Verify the local data flow

First check the API health. These `curl` requests only read data:

```bash
curl -sS http://127.0.0.1:8000/health
curl -sS http://127.0.0.1:8000/api/v1/status
curl -sS http://127.0.0.1:8000/api/v1/readings/latest
curl -sS 'http://127.0.0.1:8000/api/v1/readings?limit=10'
```

A complete test requires an ESP32 log showing a physical DHT11 reading, a
matching message in the MQTT receiver log, and the stored reading returned
by the API. The full procedure is in `testprotokoll.md`.

## Run automated tests

```bash
source .venv/bin/activate
python -m pytest
```

The tests use temporary databases and a mocked MQTT client. They do not
connect to the broker or change `data/readings.db`. See `tests/README.md` for
details.

## Documentation

The [documentation index](documentation/README.md) links to the architecture,
API, security analysis, and troubleshooting guides. Observed results and
remaining verification are in the [test protocol](testprotokoll.md).

## Known limitations

- The telemetry queue is held in RAM and is lost when the ESP32 restarts.
- QoS 1 can deliver duplicates; the receiver therefore ignores an existing
  `sensorId` and `timestamp` combination.
- The API has no authentication of its own and should only be exposed in a
  trusted local environment until access control is added.
- A successful firmware build does not replace testing with the physical
  sensor, network, and broker.
