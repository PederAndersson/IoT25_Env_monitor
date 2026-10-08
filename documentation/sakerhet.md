# Security analysis

## Assets and attack surfaces

The solution handles WiFi and MQTT credentials, broker traffic, sensor
readings, and a local SQLite database. The ESP32, broker, Python receiver,
and HTTP client have different roles and do not need the same access.
The protections below are described from the code and configuration in this
repository; their effectiveness against the actual broker is evaluated in
the [test protocol](../testprotokoll.md).

| Risk | Implemented measure | Remaining risk or verification need |
| --- | --- | --- |
| MQTT traffic is intercepted or forged | The ESP32 connects through `mqtts://` using the ESP-IDF CA bundle. The Python receiver calls Paho's `tls_set()` with `certs/ca.crt` or `MQTT_CA_PATH`. The code does not enable insecure certificate verification. Negative Python-client tests rejected a wrong hostname and CA on 2026-10-07. | No separate negative ESP32 test has been run. The minimum TLS version is not set explicitly in application code. |
| MQTT or WiFi credentials leak through Git | The example configuration contains placeholders. The main repository ignores `.env`, `sdkconfig`, key files, and database files. Application code does not log passwords. | `sdkconfig` and built firmware contain local credentials; protect the computer, build artifacts, and physical device. Git ignore rules cannot prevent secrets from being copied manually into tracked files. |
| Malformed or hostile MQTT payload damages storage or causes SQL injection | The receiver subscribes only to `esp-test/+/telemetry`, checks the topic structure, and requires a JSON object with six validated fields. `INSERT` and the history query's `LIMIT` use parameterized SQL. | The application has no JSON size limit. It does not explicitly reject non-finite floating-point values or very long strings. |
| Unauthorized API reads | Local Python startup according to the README binds the API to `127.0.0.1`. | The API has no authentication or HTTPS. Compose publishes `8000:8000`, which may expose it to other computers depending on the host network and firewall. Restrict access before using it elsewhere. |
| Unauthorized MQTT publication or subscription | The firmware uses a username/password and publishes only to device-specific topics under `esp-test/<device-id>/`. The receiver subscribes only to `esp-test/+/telemetry`. | Broker ACLs and account permissions are outside this repository and have not been verified here. |
| A container creates SQLite files that the local user cannot manage | Compose runs both services with configurable `HOST_UID` and `HOST_GID` against the bind-mounted data directory. | Incorrect IDs in `.env` can still cause permission errors. |

## Communication protection

The firmware starts WiFi and waits for NTP synchronization before starting
the MQTT client. This lets TLS check certificate validity periods. MQTT uses
TCP with TLS, not WebSockets. The ESP32 supplies a trusted CA bundle and
Python supplies a CA certificate. Clients must connect using the hostname
in the certificate so that both chain and hostname can be checked. The code
does not call `tls_insecure_set(True)` or enable a similar setting.
[M-05](../testprotokoll.md) documents successful TLS connections for both
clients and negative Python-client tests with a wrong hostname and CA. A
separate negative ESP32 test has not been run.

The WiFi client requires at least WPA2-PSK in the firmware configuration.
NTP time comes from `pool.ntp.org`, but the code does not authenticate NTP
responses. Time is therefore a dependency for timestamps and TLS startup.
If NTP synchronization fails within 15 seconds, the current startup flow
does not start MQTT.

## Sensitive configuration

| Value | Where it is set locally | Version control |
| --- | --- | --- |
| WiFi SSID and password | `idf.py menuconfig` → `firmware/esp32/sdkconfig` | Git ignores `sdkconfig` |
| ESP32 MQTT URI, username, and password | `idf.py menuconfig` → the same `sdkconfig` | Tracked Kconfig files contain only empty defaults |
| Python receiver broker, port, and MQTT credentials | Local `.env` based on `.env.example` | Git ignores `.env` |
| Trusted CA for Python | `certs/ca.crt` or `MQTT_CA_PATH` | A public CA certificate may be tracked; a private key must not be |

Compose reads `.env` for the MQTT receiver. Starting Python outside Compose
requires exporting the same variables in the terminal. Do not publish
`.env`, `sdkconfig`, private keys, complete environment dumps, or logs
containing secrets. A root CA certificate is not a password, but it must
actually be trusted for the broker in use; a leaf certificate must not be
used as the trust anchor.

## Logging, monitoring, and limitations

The firmware logs WiFi/MQTT connections, errors, publication confirmations,
and queue status. The Python receiver logs connections, received topics,
validation failures, and complete accepted reading objects. Logs therefore
contain sensor IDs, values, and timestamps, although they should not contain
credentials. Restrict access and retention if those readings are sensitive.

`GET /api/v1/status` only reports the number of stored rows. It is a simple
monitoring metric but cannot detect a silent sensor by itself. The API code
does not log each HTTP request or specifically limit request rates. Whether
Uvicorn writes access logs depends on how it is started. Higher-security
deployments should add API access control, HTTPS or a restricted proxy,
clear error/freshness metrics, and a tested credential-rotation procedure.
