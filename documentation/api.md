# REST-API

The API makes readings stored in SQLite available to other local programs
over HTTP. It has no write operations. Start it as described in the
[README](../README.md). The base URL for a local Python start is
`http://127.0.0.1:8000`.

A read-only API was chosen because the measurements are already stored
locally. Multiple clients can retrieve them without direct access to SQLite
or the MQTT broker. Displaying the project's sensor data also avoids a
dependency on an external web service.

All responses are JSON. The commands below only read data.

| Method and path | Parameter | Normal response | Error |
| --- | --- | --- | --- |
| `GET /health` | None | `200`, `{"status":"ok"}` | No application-specific error code |
| `GET /api/v1/readings/latest` | None | `200`, latest stored reading | `404` if there are no readings |
| `GET /api/v1/readings` | `limit`: integer 1–100, default 100 | `200`, newest readings first; `[]` if empty | `422` if `limit` is invalid |
| `GET /api/v1/status` | None | `200`, number of stored readings | No application-specific error code |

### Health

```bash
curl -sS http://127.0.0.1:8000/health
```

```json
{"status":"ok"}
```

`/health` shows that the API process responds. It does not check the
database, broker, ESP32, or freshness of the readings.

### Latest reading

```bash
curl -sS http://127.0.0.1:8000/api/v1/readings/latest
```

Example `200` response:

```json
{
  "id": 12,
  "sensorId": "esp32-aabbccddeeff",
  "timestamp": "2026-10-03T12:34:56Z",
  "humidity_value": 45.0,
  "humidity_unit": "%",
  "temperature_value": 21.5,
  "temperature_unit": "C"
}
```

If the table is empty, the API returns `404` with:

```json
{"detail":"Reading not available."}
```

### History

```bash
curl -sS 'http://127.0.0.1:8000/api/v1/readings?limit=2'
```

Example `200` response:

```json
[
  {
    "id": 12,
    "sensorId": "esp32-aabbccddeeff",
    "timestamp": "2026-10-03T12:34:56Z",
    "humidity_value": 45.0,
    "humidity_unit": "%",
    "temperature_value": 21.5,
    "temperature_unit": "C"
  },
  {
    "id": 11,
    "sensorId": "esp32-aabbccddeeff",
    "timestamp": "2026-10-03T12:33:56Z",
    "humidity_value": 44.0,
    "humidity_unit": "%",
    "temperature_value": 21.4,
    "temperature_unit": "C"
  }
]
```

Results are ordered by descending database `id`, not re-sorted by
`timestamp`. An empty table returns `200` and `[]`. If `limit` is omitted,
it defaults to 100. For example, `limit=0`, `limit=101`, and `limit=hej`
return `422`. FastAPI then returns a JSON object whose `detail` list
describes the invalid query parameter. Exact wording can vary with the
FastAPI and validation-library versions.

### Monitoring metric

```bash
curl -sS http://127.0.0.1:8000/api/v1/status
```

```json
{"storedReadings":12}
```

`storedReadings` counts database rows. The current MQTT receiver ignores
new copies with the same `sensorId` and `timestamp`. The database still
contains 32 historical surplus rows from before duplicate protection was
added, so this metric is not a reliable historical count of unique physical
measurements.

## Errors and access

The API has no authentication and uses HTTP without TLS. A local Python
start according to the README listens on `127.0.0.1`, while Compose publishes
the container's port `8000` on the host. Access from other networks must be
restricted outside the application. If the `readings` table has not yet been
created, database-dependent endpoints may return a server error even while
`/health` returns `200`.

The automated tests check endpoint functions and FastAPI's `limit`
validation. Actual HTTP statuses are also verified in
[M-06 of the test protocol](../testprotokoll.md).
