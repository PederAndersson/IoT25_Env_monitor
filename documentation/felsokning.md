# Troubleshooting

This guide explains how to locate common faults and summarizes observations
already made. The [test protocol](../testprotokoll.md) contains the result
records for the two deliberate fault tests F-01 and F-02.

## First locate the fault

Follow the data flow one step at a time:

1. **ESP32 and DHT11:** Does the serial log show a successful physical sensor
   reading?
2. **WiFi and NTP:** Did the device obtain an IP address and synchronize time
   before MQTT started?
3. **MQTT:** Does the device log show a connection and QoS 1 publication on
   `esp-test/<device-id>/telemetry`?
4. **Python receiver:** Does it log the correct topic and successful
   validation?
5. **SQLite and API:** Does `storedReadings` increase, and does
   `/api/v1/readings/latest` return the expected timestamp?

An error earlier in the flow cannot be fixed at the API. The
[architecture guide](arkitektur.md) shows which component owns each step.

### Local checks

| Check | What the result means |
| --- | --- |
| `docker compose ps` | Shows whether local containers run and whether the API health check reports healthy. Makes no changes. |
| `docker compose logs --tail=50 mqtt_receiver` | Shows the receiver's last 50 log lines. `--tail=50` limits output; this is read-only. |
| `curl -i http://127.0.0.1:8000/health` | `-i` also shows HTTP status and headers. `200` confirms the API process responds, not that the database or broker works. |
| `curl -i http://127.0.0.1:8000/api/v1/status` | Shows whether the API can count database rows. Run before and after a reading. |
| `python -m pytest` | Runs isolated tests for validation, database, API functions, and MQTT configuration. No real broker or sensor is used. |

Run these commands from the project root in the local environment described
in the [README](../README.md). For firmware faults, use `idf.py monitor` in a
terminal with the ESP-IDF environment loaded. Avoid copying complete
environment variables or sensitive logs into a fault report.

## Common symptoms

| Symptom | Likely cause and check |
| --- | --- |
| Receiver exits immediately | Missing or invalid `MQTT_BROKER` or `MQTT_PORT`; check local `.env` and the error message. |
| ESP32 does not start MQTT | WiFi initialization or NTP synchronization failed; read the first error in the serial log. |
| TLS connection fails | Wrong CA, hostname/SAN, port, or unsynchronized clock; identify which client reports the failure. Do not disable certificate verification to connect. |
| MQTT connects but no readings reach SQLite | Check that publication uses exactly `esp-test/<device-id>/telemetry`, broker ACLs allow it, DHT11 reads succeed, and the payload passes validation. Status payloads `online`/`offline` are deliberately ignored by telemetry validation. |
| `/health` responds but database endpoints fail | The API process runs, but the database may lack the `readings` table or have incorrect file permissions. Check the shared data directory and that `HOST_UID`/`HOST_GID` in `.env` match `id -u`/`id -g`. |
| `/api/v1/readings/latest` returns 404 | The table exists but has no valid readings. Check receiver validation logs and `/api/v1/status`. |
| Multiple identical rows appear | New copies with the same `sensorId` and `timestamp` should be logged as ignored without creating another row. Check that the current receiver image runs and compare `qos`, `dup`, and `mid` in logs. Historical duplicates remain. |

### O-01: repeated readings

During the 2026-10-05 run, one physical reading with timestamp `12:16:22Z`
was queued once in the captured ESP32 log and received an MQTT
acknowledgment. The receiver nevertheless logged three arrivals and created
three database rows. A check before the fix found 121 total rows but only 89
unique sensor ID/timestamp combinations: 32 historical surplus rows.

ESP-MQTT can resend unacknowledged QoS 1 publications. That is a plausible
hypothesis, not a proven cause of these particular arrivals.
[ESP-MQTT](https://docs.espressif.com/projects/esp-mqtt/en/latest/esp32/)
describes retransmission, but the exact transport cause of the historical
copies is unproven. MQTT's `message_id` does not identify a reading across
the entire data flow.

The 2026-10-07 fix subscribed to `esp-test/+/telemetry` with QoS 1 and made
storage conditional on the `sensorId` and `timestamp` combination not already
existing. A runtime test published the same valid synthetic QoS 1 payload
twice. Both copies reached the callback; the second was logged as ignored,
and exactly one row was stored. The synthetic test row was deleted afterward.
O-01 is thus addressed at the storage layer for the current single receiver
process; the older rows remain. Multiple concurrent writers should also be
protected by a unique database constraint.

## ESP32 build and flashing problems

If the build stops because `configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES` is
redefined, local components must not include the internal
`freertos/projdefs.h` header directly. The project's WiFi component now
uses only public FreeRTOS include paths. After the direct include was removed,
the firmware built with ESP-IDF 6.0.

If flashing reports that all bytes were written and verified, then ends with
pySerial's `Could not configure port` error, the failure occurred during the
hard reset after writing. Check the USB cable, port name, and whether another
monitor holds the port. Reconnect the device if needed and start the monitor
separately. In the verified run, the new firmware started and its serial log
could then be observed.

## Case 1: deliberately invalid MQTT port at startup

This historical local check is described in `docs/STARTPROMPT.md` for
2026-10-02. It shows configuration error handling, but it is not a new
end-to-end run of F-01 in the test protocol.

1. **Observed symptom:** When the port was missing, the receiver process
   exited with status 1. Nonnumeric values and values outside 1–65535 were
   rejected.
2. **How it was identified:** Several deliberately invalid `MQTT_PORT`
   values were tested separately in an isolated environment, and the process
   exit status was checked.
3. **Tools or logs:** Receiver errors for a missing variable, noninteger
   value, and invalid port range; terminal exit status.
4. **Cause:** `MQTT_PORT` was missing or invalid.
5. **Action taken:** The receiver now validates the port at startup and
   exits clearly with status 1 instead of continuing with bad configuration.
   The user must set a valid TLS port in local `.env`.
6. **How the action was verified:** Error paths and exit status 1 for a
   missing port were checked manually on 2026-10-02. Automated tests also
   verify invalid values and a valid mocked startup path. This case does not
   document reconnection to a real broker after changing `.env`.

## Case 2: deliberate broker outage and reconnection

This historical hardware observation from 2026-10-02 is recorded in
`docs/STARTPROMPT.md`. It tests MQTT reconnection, but a stopped broker
cannot publish the Last Will.

1. **Observed symptom:** The ESP32 lost MQTT contact while the broker was
   unavailable. The log showed successive delays of about 10, 20, and 40
   seconds.
2. **How it was identified:** The broker outage was introduced deliberately,
   and the ESP32 serial log was followed during and after the outage.
3. **Tools or logs:** Firmware serial logs for disconnection, reconnection
   attempts, TLS validation, and a new MQTT connection.
4. **Cause:** The broker was temporarily unavailable while the ESP32 still
   had WiFi connectivity.
5. **Action taken:** The broker was made available again. Firmware
   reconnection with exponential backoff handled the outage without a device
   restart.
6. **How the action was verified:** Logs showed certificate validation and
   a new connection. Earlier observations also showed a reset to roughly
   10 seconds after at least 60 seconds of stable connection and continued
   higher backoff after a short connection.

This case does not prove that Last Will `offline` was published or that the
same reading reached SQLite and the API. Those require separate runs.

## Fault tests for the final submission

[F-01 in the test protocol](../testprotokoll.md) was run on 2026-10-05:
deliberately malformed JSON was rejected and logged, and the stored row
count did not change in the controlled repeat run. The test **passed**.

F-02 initially failed on 2026-10-05 because WiFi retries stopped after a
long outage. A separate WiFi task with continued retries, exponential delay,
and jitter was then added. The 2026-10-07 retest showed retained Last Will
`offline`, continued sampling into the RAM queue, WiFi backoff up to
30 seconds, and automatic recovery of WiFi, TLS, MQTT, SQLite, and API after
the network returned. F-02 therefore **passed**.
