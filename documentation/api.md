# REST-API

API:t gör mätvärden i SQLite tillgängliga för andra lokala program via HTTP.
Det finns inget skriv-API. Starta det enligt [README](../README.md). Vid
lokal Pythonstart används basadressen `http://127.0.0.1:8000`.

Ett eget läs-API valdes eftersom mätningarna redan lagras lokalt och flera
klienter kan hämta dem utan direkt åtkomst till SQLite eller MQTT-brokern.
Det undviker också ett beroende på en extern webbtjänst för att visa
projektets egna sensordata.

Alla svar är JSON. Kommandona nedan läser data och ändrar inte databasen.

| Metod och sökväg | Parameter | Normal respons | Fel |
| --- | --- | --- | --- |
| `GET /health` | Ingen | `200`, `{"status":"ok"}` | Ingen särskild felkod definierad i applikationen |
| `GET /api/v1/readings/latest` | Ingen | `200`, senaste lagrade mätningen | `404` om tabellen saknar mätningar |
| `GET /api/v1/readings` | `limit`: heltal 1–100, standard 100 | `200`, lista med nyaste poster först; `[]` om den är tom | `422` om `limit` är ogiltig |
| `GET /api/v1/status` | Ingen | `200`, antal lagrade mätningar | Ingen särskild felkod definierad i applikationen |

### Hälsa

```bash
curl -sS http://127.0.0.1:8000/health
```

```json
{"status":"ok"}
```

`/health` visar att API-processen svarar. Den provar inte databas, broker,
ESP32 eller färskheten hos mätningarna.

### Senaste mätningen

```bash
curl -sS http://127.0.0.1:8000/api/v1/readings/latest
```

Exempel på ett `200`-svar:

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

När tabellen är tom blir status `404` med svaret:

```json
{"detail":"Reading not available."}
```

### Historik

```bash
curl -sS 'http://127.0.0.1:8000/api/v1/readings?limit=2'
```

Exempel på ett `200`-svar:

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

Sorteringen använder databasens fallande `id`, inte en ny sortering efter
`timestamp`. En tom tabell ger `200` och `[]`. Om `limit` utelämnas används
100. Till exempel ger `limit=0`, `limit=101` och `limit=hej` status `422`.
FastAPI returnerar då ett JSON-objekt med `detail`, en lista som beskriver
vilket frågefält som var felaktigt. Den exakta texten kan bero på versionen
av FastAPI och dess valideringsbibliotek.

### Övervakningsmått

```bash
curl -sS http://127.0.0.1:8000/api/v1/status
```

```json
{"storedReadings":12}
```

`storedReadings` är antalet rader i databasen. Det är inte antalet unika
fysiska mätningar, eftersom QoS 1-dubbletter inte tas bort.

## Fel och åtkomst

API:t har ingen autentisering och använder HTTP utan TLS. Lokal Pythonstart
enligt README lyssnar på `127.0.0.1`, medan Compose publicerar port `8000`
från containern på värddatorn. Exponering mot andra nät behöver begränsas
utanför applikationen. Om databastabellen ännu inte har skapats kan
databasberoende endpoints misslyckas med ett serverfel; `/health` kan ändå
svara `200`.

De automatiska testerna kontrollerar endpointfunktioner och FastAPI:s
`limit`-validering. Riktiga HTTP-statusar ska dessutom verifieras enligt
[M-06 i testprotokollet](../testprotokoll.md).
