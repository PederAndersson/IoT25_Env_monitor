# Test protocol

## Purpose and status rules

This protocol shows how the solution is verified against
`Kravspecifikation/Inlämningsuppgift.pdf`. Automated tests run locally with
pytest. Firmware, the physical sensor, broker, TLS, and the full data flow
require separate manual tests.

Allowed status values are:

- **Not run**: the test is defined but has no observed run.
- **Passed**: the expected result was observed and evidence is recorded.
- **Failed**: the result differed from the expectation.
- **Blocked**: the test could not run, and the reason is recorded.

## Requirements traceability

| Requirement | Test ID | Verification |
|---|---|---|
| Physical IoT device and real reading | M-02, M-03 | DHT11 reading and continuous data flow |
| Periodic transmission | M-02, M-03 | Multiple readings at the expected interval |
| MQTT and documented addressing | M-03, M-04 | TLS port, topics, QoS, and client ID |
| Structured, consistent JSON | A-01, A-02, M-03 | Automated contract tests and actual payload |
| API integration and error handling | A-04, M-03 | Endpoint tests, 404, and 422 |
| Secure communication | M-05, M-10 | TLS chain, hostname, and secret handling |
| Logging | A-02, A-05, M-08 | Validation, connection, and communication logs |
| Monitoring | A-04, M-09 | Status endpoint and queue/error counters |
| Two deliberate faults | F-01, F-02 | Invalid JSON and network outage |
| Installation and local operation | M-01, M-06, M-07 | Firmware build, Python, and Compose |
| Robust communication handling | M-04, F-02 | Last Will, reconnection, backoff, and queueing |

## Automated tests

### Regression on 2026-10-07

The full automated suite ran with Python 3.14.8 and pytest 9.1.1 after the
firmware, receiver, and documentation work. All 68 tests passed in
0.09 seconds: 11 API tests, 5 database tests, 11 MQTT receiver tests, and 41
validation tests. Tests used isolated or mocked dependencies as described in
`tests/README.md`.

### A-01 – Valid data contract

- **Requirement:** Structured data and a consistent contract.
- **Purpose:** Verify that the six-field firmware format is accepted.
- **Prerequisites:** Development dependencies installed.
- **Steps:** Run `python -m pytest tests/test_validation.py`.
- **Expected:** Valid payloads, boundary values, and timestamps with `Z` or a
  UTC offset are accepted.
- **Actual result:** All valid contract cases passed within the full suite of
  68 passing tests.
- **Status:** Passed.
- **Date and evidence:** 2026-10-03, `python -m pytest`.

### A-02 – Invalid payload and logging

- **Requirement:** Validation, logging, and communication errors.
- **Purpose:** Ensure invalid data is rejected without being stored.
- **Prerequisites:** Development dependencies installed.
- **Steps:** Run `python -m pytest tests/test_validation.py`.
- **Expected:** Malformed JSON, wrong types, missing fields, invalid
  timestamps, wrong units, and out-of-range values are rejected and logged.
- **Actual result:** All negative payload cases passed. Mock storage remained
  empty and the expected warnings were captured.
- **Status:** Passed.
- **Date and evidence:** 2026-10-03, `python -m pytest`.

### A-03 – SQLite persistence

- **Requirement:** Processing and safe storage of external data.
- **Purpose:** Verify table creation, storage, and parameterized queries.
- **Prerequisites:** Development dependencies installed.
- **Steps:** Run `python -m pytest tests/test_database.py`.
- **Expected:** A temporary table can be initialized repeatedly, SQL
  characters in a sensor ID remain data without changing the table, and an
  existing sensor ID/timestamp combination is not stored again.
- **Actual result:** Five database tests passed against separate temporary
  databases. They also verified that a duplicate is ignored while two
  different sensors can share a timestamp. A checksum check showed that
  `data/readings.db` was unchanged.
- **Status:** Passed.
- **Date and evidence:** 2026-10-03 and 2026-10-07, `python -m pytest` and
  checksums before and after the run.

### A-04 – REST API

- **Requirement:** API operation, responses, error handling, and monitoring.
- **Purpose:** Verify all endpoints against an isolated database.
- **Prerequisites:** Development dependencies installed.
- **Steps:** Run `python -m pytest tests/test_api.py`.
- **Expected:** Endpoint functions return the correct data, an empty
  database produces a 404 exception, and FastAPI's query model rejects an
  invalid `limit`.
- **Actual result:** Eleven API tests passed. Actual HTTP responses are also
  covered by the manual local test M-06.
- **Status:** Passed.
- **Date and evidence:** 2026-10-03 and 2026-10-07, `python -m pytest`.

### A-05 – MQTT configuration and callbacks

- **Requirement:** Configuration, connection, addressing, and logging.
- **Purpose:** Verify startup validation without network access.
- **Prerequisites:** Development dependencies installed.
- **Steps:** Run `python -m pytest tests/test_mqtt_receiver.py`.
- **Expected:** Missing or invalid ports are rejected; valid configuration
  reaches the mocked client, only the telemetry topic is subscribed with
  QoS 1, and the callback ignores other topics.
- **Actual result:** Eleven tests passed with a mocked MQTT client and no
  network connection. They verified topic filtering and logging of QoS,
  duplicate, and message ID metadata.
- **Status:** Passed.
- **Date and evidence:** 2026-10-03 and 2026-10-07, `python -m pytest`.

## Manual test cases

### M-01 – Local firmware build

- **Requirement:** Buildable IoT software.
- **Purpose:** Verify compilation and linking for the ESP32-C6.
- **Local prerequisites:** ESP-IDF 6.0 installed and its environment loaded.
- **Steps:** Run `idf.py build` from `firmware/esp32/`.
- **Expected:** The command finishes without errors and creates firmware
  artifacts.
- **Actual result:** ESP-IDF 6.0 compiled and linked the project and created
  `iot_sensor.bin`. After WiFi change `9d64205`, the first build failed
  because the internal FreeRTOS header `freertos/projdefs.h` was included
  directly before `freertos/FreeRTOS.h`. The unnecessary direct include was
  removed, and a new build ended with `Project build complete`. The firmware
  was flashed, and the serial log reported app version `9d64205-dirty`;
  the suffix came from local uncommitted changes.
- **Status:** Passed.
- **Date and evidence:** 2026-10-07, `idf.py build` ended with
  `Project build complete`, and the flashed device started that app version.

### M-02 – Startup order and physical DHT11

- **Requirement:** Physical sensor, periodic readings, and NTP before TLS.
- **Purpose:** Verify the real sensor and secure startup order.
- **Local prerequisites:** Firmware flashed, ESP32-C6 and DHT11 connected,
  with WiFi and NTP available.
- **Steps:** Start the serial monitor, restart the board, and observe at least
  three measurement intervals.
- **Expected:** The log shows WiFi and NTP before MQTT/TLS and at least three
  successful physical readings at roughly the configured interval. A reading
  may enter the RAM queue while MQTT connects, but must not be published
  before a verified MQTT/TLS connection.
- **Actual result:** In the 2026-10-05 run, WiFi was ready before NTP, time
  synchronized before TLS, and MQTT connected only after certificate
  validation. The first DHT11 reading entered the RAM queue while MQTT was
  connecting and was published afterward. Further readings were taken and
  published at 60-second intervals. The 2026-10-07 retest with app version
  `9d64205-dirty` showed the same secure order, a successful physical DHT11
  reading, and continued periodic publication. The assignment requires
  physical and periodic sensor data but does not require sampling to wait
  for an established MQTT connection; the earlier stricter interpretation
  was therefore dropped.
- **Status:** Passed.
- **Date and evidence:** 2026-10-05 and 2026-10-07, ESP-IDF monitor and a
  read-only review of `Kravspecifikation/Inlämningsuppgift.pdf`. Raw logs
  contain local network identifiers and must not be added to Git.

### M-03 – Continuous end-to-end flow

- **Requirement:** Working data flow, JSON, MQTT, storage, and API.
- **Purpose:** Trace the same physical reading through the whole system.
- **Local prerequisites:** ESP32 running, TLS broker reachable, and local
  Python services or Compose started.
- **Steps:** Note the sensor ID and a timestamped publication in the ESP32
  log. Compare a matching receiver payload with the API history. The
  firmware log does not print the whole JSON payload.
- **Expected:** The same six fields and values can be traced from DHT11 to
  API, and at least three periodic rows are stored.
- **Actual result:** The ESP32 queued a physical reading at 14:16:22 local
  time (12:16:22 UTC) and reported MQTT publication at 14:16:25. The local
  receiver got `esp-test/<device-id>/telemetry` at 12:16:25–26 UTC and
  validated six fields with timestamp `2026-10-05T12:16:22Z`, humidity
  48.0%, and temperature 25.0°C. The API history returned matching fields
  and values. The receiver log also showed readings about once per minute
  from 12:09 to 12:13; API status increased from 12 to 13 rows between the
  12:10:18Z and 12:11:18Z readings.
  **Historical issue O-01:** The 12:16:22Z reading was received and stored
  three times (database IDs 22–24), despite one enqueue and one publication
  acknowledgment in the captured firmware log. Seven timestamps later had
  2–4 rows each. At the time, the receiver stored every received message;
  the cause of repeated deliveries was not established.
- **Investigation of O-01, 2026-10-06:** The firmware enqueued telemetry
  with QoS 1. The Python receiver subscribed without specifying QoS and
  therefore used the default QoS 0. ESP-MQTT can resend unacknowledged QoS 1
  messages. This could explain why one enqueue and final acknowledgment
  coincided with several received copies, but it is a hypothesis, not an
  established cause. Broker logs or a packet trace for that publication are
  unavailable. MQTT's `message_id` belongs to one connection; it is not an
  end-to-end ID.
- **Fix and retest of O-01, 2026-10-07:** Before the fix, 121 database rows
  represented 89 unique sensor ID/timestamp combinations, leaving 32
  historical surplus rows. An atomic duplicate guard for that combination
  was added to the receiver. During a runtime retest, the same valid
  synthetic QoS 1 payload was published twice. Both copies arrived, the
  second was logged as an ignored duplicate, and exactly one row was
  stored. The test row was deleted afterward. Historical duplicates remain;
  their exact transport cause cannot be determined without older broker
  logs or a packet trace.
- **Status:** Passed for the continuous six-field flow, periodic storage,
  and protection against new duplicate rows in the current receiver.
  O-01 is fixed at the storage layer; its historical transport cause is
  unknown.
- **Date and evidence:** 2026-10-05 and retest on 2026-10-07, ignored raw
  log `data/esp32-monitor-2026-10-05-e2e.log`, anonymized receiver logs
  from 12:09–12:16 UTC, and `/api/v1/readings?limit=10` and
  `/api/v1/status` responses. Raw logs with network identifiers must not
  be added to Git.

### M-04 – MQTT identity and status

- **Requirement:** Topics, client ID, QoS, keepalive, Last Will, and online
  status.
- **Purpose:** Verify the MQTT contract and broker status.
- **Local prerequisites:** An authorized MQTT subscriber and the ESP32 are
  connected to the broker.
- **Steps:** Subscribe to `esp-test/<device-id>/#`, restart the device, and
  observe status and telemetry without deliberately interrupting the network.
- **Expected:** A stable MAC-derived ID is used, retained `online` is present
  on the status topic, and telemetry uses the correct topic with QoS 1.
- **Actual result:** The firmware logged the same device ID and topics on
  multiple starts. A separate subscriber received `online retained=True`
  with QoS 1 at 12:50:46 UTC. During F-02, live `offline` arrived with
  QoS 1; a new subscriber got `offline retained=True`. The receiver log
  confirmed the telemetry topic. A read-only code review on 2026-10-07
  confirmed the stable client ID, Last Will and online status with QoS 1 and
  retain, telemetry enqueue with QoS 1, and 60-second keepalive. A separate
  TLS-verified subscriber to the firmware endpoint then received a physical
  telemetry payload with QoS 1 and `retained=False`.
- **Status:** Passed.
- **Date and evidence:** 2026-10-05 and 2026-10-07, anonymized subscriber
  lines, firmware/receiver logs, and `mqtt_service.c`. The device ID is
  masked in this protocol.

### M-05 – TLS chain and hostname

- **Requirement:** Basic secure communication.
- **Purpose:** Confirm that both MQTT clients verify the root CA and
  hostname.
- **Local prerequisites:** The broker certificate's SAN and trusted root CA
  are known; no secrets are written to this protocol.
- **Steps:** Connect the Python receiver and ESP32 using the correct
  hostname. Separately test the Python client with a wrong CA or a reachable
  name/IP absent from the certificate's SAN; then restore local settings.
- **Expected:** Correct configuration connects. A wrong CA or hostname
  causes a TLS error without insecure fallback or publication.
- **Actual result:** The ESP32 logged certificate validation before MQTT
  connection and reconnected with a verified certificate during F-02. A new
  Python connection from the host was rejected on 2026-10-05 with
  `SSLCertVerificationError: Hostname mismatch`. The error was reproduced
  on 2026-10-07 in a freshly built Compose image: the receiver rejected the
  broker certificate and entered a restart loop. No insecure fallback was
  used. A separate TLS check showed that the Python receiver and ESP32 used
  the same host but different ports. The receiver's port presented a chain
  trusted by `certs/ca.crt` but the wrong hostname; the firmware's port had
  the correct hostname but a different, public certificate chain. A
  temporary client using the system CA bundle connected securely to the
  firmware endpoint. The failing Compose service was stopped after that
  observation. The local `.env` was then changed to the firmware endpoint
  and system CA bundle. A recreated receiver container connected with TLS
  and remained stable. An isolated negative test against that endpoint used
  an intentionally wrong CA and was rejected with
  `SSLCertVerificationError` without fallback.
- **Status:** Passed for secure communication in the tested configuration.
  The ESP32 connected with certificate verification; the Python client
  rejected both a wrong hostname and a wrong CA. A separate negative ESP32
  test is needed only to claim observed rejection by the ESP32 itself.
- **Date and evidence:** 2026-10-05 and 2026-10-07, ESP32 log, newly built
  Compose receiver, OpenSSL checks, and an isolated negative Python client.
  No credentials are included here.

### M-06 – Local Python startup

- **Requirement:** Another developer can install and run the solution.
- **Purpose:** Verify the README instructions without Raspberry Pi-specific
  steps.
- **Local prerequisites:** Clean local virtual environment and local `.env`.
- **Steps:** Follow the Python environment, configuration, and local startup
  sections of `README.md`; request `/health` and `/api/v1/status`.
- **Expected:** Both processes start and the endpoints return HTTP 200.
- **Actual result:** A new virtual environment was created under `/tmp`
  with Python 3.14.8, and all pinned production dependencies were installed
  from `requirements.txt`. The MQTT receiver started according to the README,
  connected with TLS, and received and validated a physical telemetry
  payload. It then crashed with `sqlite3.OperationalError: attempt to write a
  readonly database`. After the Compose run, `data/readings.db` was owned by
  `nobody:nobody` with mode `644`. The host user owned the directory but
  could not write to the file. The API started locally on `127.0.0.1:8000`;
  `/health` and `/api/v1/status` returned HTTP 200, with status showing 115
  existing rows. Compose was changed to run both services with configurable
  `HOST_UID` and `HOST_GID`, defaulting to `1000:1000`. The existing database
  was replaced by a byte-identical, integrity-checked copy with correct
  ownership, and the original was saved as a local backup. Compose then
  stored row 116 without changing ownership. In a retest from the same
  clean Python environment, the receiver connected, validated, and stored
  several new physical readings without SQLite errors. The API still
  returned HTTP 200 and row 119. Database ownership remained `1000:1000`.
- **Status:** Passed after the fix and retest.
- **Date and evidence:** 2026-10-07, clean virtual environment, receiver/API
  logs, HTTP 200 responses, API history, SQLite integrity check, identical
  SHA-256 checksums before replacement, and `stat` on the database file.

### M-07 – Local Docker Compose

- **Requirement:** Installation, operation, monitoring, and persistent
  storage.
- **Purpose:** Verify Compose configuration, health check, and persistence.
- **Local prerequisites:** Docker and Compose installed; local `.env` exists.
- **Steps:** Run `docker compose config`; start with
  `docker compose up --build -d`; wait for healthy; store a valid row; run
  `docker compose down` and restart; retrieve the same row through the API.
- **Expected:** Configuration is valid, the API becomes healthy, and the row
  survives container recreation.
- **Actual result:** `docker compose config -q` exited without error, and
  `docker compose up --build -d` rebuilt the current image and recreated both
  containers. The API became healthy, `/health` returned HTTP 200, and the
  recreated container read the persistent database with 111 rows and latest
  row 111. The receiver repeatedly restarted because of the verified
  hostname mismatch in M-05. After correcting local `.env`, only the
  receiver was recreated; both services remained `Up`, the API stayed
  healthy, and the receiver connected with TLS. A new physical reading was
  validated and stored as row 112 while earlier row 111 remained after
  container recreation. No additional explicit `down`/`up` was run, but
  `up --build -d` recreated both containers and persistence was observed
  afterward. After the UID/GID change, both services were recreated as
  `1000:1000`; the API became healthy, and the receiver stored row 116 while
  database ownership remained `1000:1000`.
- **Status:** Passed.
- **Date and evidence:** 2026-10-07, Compose build/status output, HTTP 200,
  `/api/v1/status`, API history, and receiver connection/data logs.

### M-08 – Logging

- **Requirement:** Relevant connection, data, validation, and error logs.
- **Purpose:** Check observability without leaking secrets.
- **Local prerequisites:** The system runs locally and can receive a valid
  and an invalid payload.
- **Steps:** Observe connection, valid reading, validation error, outage,
  and reconnection in firmware and Python logs; visually check for secrets.
- **Expected:** All event types are logged with useful context but without
  passwords or tokens.
- **Actual result:** The firmware logged WiFi/MQTT connection, publication,
  communication errors, and queue size. The Python receiver logged received
  and validated data and rejected malformed JSON. During the F-02 retest,
  logs showed a long WiFi outage, continued connection attempts, backoff up
  to 30 seconds, and successful WiFi, TLS, and MQTT reconnection.
  The earlier receiver also subscribed to the status topic and tried to
  parse `online` and `offline` as sensor JSON. This produced misleading
  invalid-JSON warnings and was recorded as O-02. In the 2026-10-07 retest,
  subscription was restricted to `esp-test/+/telemetry` with QoS 1, and the
  callback gained a separate guard against other topics. After reconnection,
  no retained status payload reached the telemetry callback and no false
  JSON warning appeared. A physical telemetry payload arrived and was
  logged with QoS, duplicate, and message ID metadata.
  Runtime logs showed no passwords or tokens, and a static search found no
  log calls referring to configured WiFi or MQTT passwords. Raw firmware
  logs do contain local network identifiers and must be anonymized before
  version control.
- **Status:** Passed. O-02 was fixed and retested.
- **Date and evidence:** 2026-10-05 and 2026-10-07, anonymized log excerpts
  in M-02, M-03, F-01, and F-02.

### M-09 – Monitoring metric

- **Requirement:** At least one monitoring function.
- **Purpose:** Verify the status endpoint and firmware statistics.
- **Local prerequisites:** The system runs and can receive readings.
- **Steps:** Read `/api/v1/status` before and after a valid reading; observe
  queue size and the dropped-reading counter in the relevant firmware test.
- **Expected:** `storedReadings` increases by one and firmware logs current
  queue/error statistics.
- **Actual result:** `/api/v1/status` increased from 12 to 13 stored rows
  between readings at 12:10:18Z and 12:11:18Z. During F-02, firmware showed
  the queue growing to 8/60 and logged communication errors. Since the O-01
  fix, the receiver ignores new duplicates with the same sensor ID and
  timestamp. Historical duplicate rows remain; dropped readings were not
  induced because the queue did not fill.
- **Status:** Passed for the status metric and queue monitoring.
- **Date and evidence:** 2026-10-05, two local API responses and ignored
  firmware log `data/esp32-monitor-2026-10-05-f02.log`.

### M-10 – Secrets and version control

- **Requirement:** No credentials in Git.
- **Purpose:** Check that only example values and a public CA are tracked.
- **Local prerequisites:** Run from the project root.
- **Steps:** Run `git ls-files` and confirm that `.env`, `sdkconfig`, private
  keys, and password files are absent. Use `git grep -l` if needed so only
  filenames, not matching secret values, are shown.
- **Expected:** No secret files or real credentials are tracked;
  `.env.example` contains only neutral placeholders.
- **Actual result:** Neither the current Git tree nor the filename history
  contained `.env`, `sdkconfig`, private keys, password files, or other
  checked secret files. `.env` and `firmware/esp32/sdkconfig` were confirmed
  ignored. `.env.example` uses neutral placeholders, and WiFi/MQTT Kconfig
  files have empty credential defaults. The only tracked file in `certs/` is
  `ca.crt`; OpenSSL confirmed it is the project's public, self-signed root
  CA, and no private key header was found.
- **Status:** Passed.
- **Date and evidence:** 2026-10-07, read-only checks with `git ls-files`,
  `git check-ignore`, `git grep`, `git log --all`, `rg`, and `openssl x509`.
  Only filenames, ignore rules, and the public certificate subject/issuer
  were checked; no secret values were printed.

## Deliberate fault F-01 – Invalid JSON

- **Related requirements:** Validation, logging, and troubleshooting.
- **Prerequisites:** Broker, receiver, and API running, with the initial
  `/api/v1/status` value recorded. Use a topic authorized for the test account.
- **Injection:** Publish, for example, `{invalid-json` on
  `esp-test/<device-id>/telemetry` using a locally configured MQTT client.
- **Expected:** Invalid-JSON warning, receiver continues running, and
  `storedReadings` does not change.

Required fault analysis after the run:

1. **Observed symptom:** The receiver logged `Payload contains invalid JSON`
   when the test message arrived, but continued running.
2. **How it was identified:** A deliberate `{invalid-json` payload was
   published with QoS 1 on an allowed telemetry topic. A repeat run showed
   64 stored rows both before and two seconds after the faulty message.
3. **Tools or logs:** Local TLS-verified MQTT test client,
   `docker compose logs`, and a read-only SQLite count.
4. **Cause:** Deliberately malformed JSON syntax.
5. **Action taken:** The receiver rejected the payload without restarting;
   the physical sender then continued with valid six-field messages.
6. **How the action was verified:** The row count remained 64 → 64 in the
   repeat run. After the first fault injection, new sensor messages at
   12:34:22 and 12:35:22 UTC were validated. Exactly one row per valid
   reading is not claimed for this historical run because of the separate
   duplicate issue O-01.

- **Actual result:** The receiver received malformed JSON and logged a
  warning; the faulty message did not stop the process or change the stored
  row count.
- **Status:** Passed.
- **Date and evidence:** 2026-10-05; anonymized receiver logs at 12:33:41,
  12:34:22, 12:35:22, and 12:41:42 UTC, plus before/after count 64 → 64.

## Deliberate fault F-02 – Unexpected network outage

- **Related requirements:** Communication faults, robust reconnection, Last
  Will, logging, and troubleshooting.
- **Prerequisites:** Broker and separate subscriber remain running, the
  ESP32 has been connected for more than 60 seconds, and the sensor interval
  is known.
- **Injection:** Unexpectedly interrupt ESP32 network access without
  stopping the broker. Keep the outage longer than at least one sensor
  interval, then restore the network.
- **Expected:** The broker publishes retained `offline`; the ESP32 logs WiFi
  and MQTT errors and retains readings in the RAM queue; after restoration,
  backoff, retained `online`, and new or queued API rows appear.

Required fault analysis after the run:

1. **Observed symptom:** In the original 2026-10-05 run, the ESP32 stopped
   trying after five WiFi attempts and did not recover when the hotspot
   returned. In the 2026-10-07 retest, retries continued throughout the
   outage and the device reconnected automatically.
2. **How it was identified:** The ESP-IDF monitor showed retries stopping at
   5/5 in the original run. In the retest it showed continued delays of
   about 1.7, 2.5, 5.0, 8.5, and 16.1 seconds, then a 30-second cap. A
   separate TLS-verified MQTT subscriber received broker-published `offline`
   with `retained=True` and QoS 1.
3. **Tools or logs:** ESP-IDF monitor, a temporary TLS-verified MQTT status
   subscriber in the receiver container, receiver Compose logs, and the
   local REST API.
4. **Cause:** The earlier WiFi logic stopped retries when
   `CONFIG_APP_WIFI_MAXIMUM_RETRY` was reached. MQTT correctly waited for
   WiFi, but no component kept initiating WiFi attempts.
5. **Action taken:** A separate WiFi reconnection task was introduced in
   commit `9d64205`. After a previously successful IP connection it keeps
   retrying with exponential delay, jitter, and a maximum wait of 30
   seconds.
6. **How the action was verified:** The ESP32 had been stably connected for
   more than 60 seconds before the hotspot was switched off. During the
   outage, two physical DHT11 readings were taken and retained for later
   publication. After the hotspot returned, the device obtained IP, waited
   through MQTT's 10-second backoff, verified the TLS certificate, and
   reconnected to the broker. The two queued payloads were acknowledged,
   validated by the Python receiver, and stored as rows 107 and 108 with
   measurement times 18:18:59Z and 18:19:59Z. A live subscriber got
   `online` with QoS 1, and a new subscriber got `online retained=True`.

- **Actual result:** Last Will, retained `offline`, error logging, continued
  sampling, the RAM queue, exponential WiFi backoff, automatic WiFi/TLS/MQTT
  reconnection, retained `online`, and delivery through SQLite to the REST
  API all worked in the same retest.
- **Status:** Passed.
- **Date and evidence:** Original failed test on 2026-10-05 and passed
  retest on 2026-10-07. Evidence was observed in the ESP-IDF monitor,
  anonymized status subscriptions, `docker compose logs --since=5m
  mqtt_receiver`, and `/api/v1/readings?limit=6`. Raw logs with network
  identifiers must not be added to Git.

## Summary

Only observed, dated runs are marked **Passed**. M-01 through M-10 and F-01
and F-02 passed. M-06 passed after the UID/GID fix and retest. O-01 was
fixed at the storage layer and retested with two identical QoS 1
publications; 32 historical surplus rows remain and their exact transport
cause is unknown. O-02 was fixed by the narrow telemetry subscription and
topic filtering and retested during runtime.
