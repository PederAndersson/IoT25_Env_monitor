# Säker och integrerad IoT-lösning

Projektet läser temperatur och luftfuktighet från en DHT11 ansluten till en
ESP32-C6. Enheten skickar mätningarna som JSON över MQTT med TLS. En lokal
Python-tjänst validerar och lagrar mätningarna i SQLite, och ett FastAPI-API
gör dem tillgängliga över HTTP.

Den här instruktionen beskriver lokal utveckling och testning. Driftsättning
på Raspberry Pi ingår inte.

## Dataflöde

```text
DHT11 -> ESP32-C6 -> MQTT över TLS -> Pythonmottagare -> SQLite -> FastAPI
```

MQTT-meddelandet använder följande kontrakt:

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

## Lokala beroenden

- Python 3.13 eller en kompatibel senare version
- Docker Engine med Docker Compose för containerkörning
- ESP-IDF 6.0 för firmwarebygge
- ESP32-C6 och DHT11 för hårdvarutester
- Tillgång till en MQTT-broker som accepterar MQTT över TLS

## Pythonmiljö

Följande kommandon skapar en lokal virtuell miljö och installerar
produktionsberoendena. De ändrar endast `.venv/`, som ignoreras av Git.

```bash
python -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements.txt
```

För att även köra testsviten installerar du utvecklingsberoendena i samma
miljö:

```bash
python -m pip install -r requirements-dev.txt
```

## Lokal konfiguration

Kopiera exempelkonfigurationen och ersätt platshållarna med värden för din
lokala testmiljö:

```bash
cp .env.example .env
```

`.env` ska innehålla:

- `MQTT_BROKER`: broker-certifikatets värdnamn;
- `MQTT_PORT`: TLS-porten, normalt `8883`;
- `MQTT_USERNAME` och `MQTT_PASSWORD`: lokala MQTT-uppgifter;
- valfritt `MQTT_CA_PATH`: sökväg till betrodd root-CA. Standard är
  `certs/ca.crt`.

Filen `.env` är ignorerad av Git. Lägg aldrig riktiga lösenord eller privata
nycklar i versionshanterade filer.

## Starta Python-tjänster lokalt

Aktivera först `.venv`. Starta sedan MQTT-mottagaren i en terminal:

```bash
set -a
source .env
set +a
python -m src.iot_receiver.mqtt_receiver
```

`set -a` gör att variabler som läses från `.env` exporteras till processen.
`set +a` stänger av automatisk export igen. Kommandot startar mottagaren,
skapar SQLite-tabellen vid behov och väntar på MQTT-meddelanden.

Starta API:t i en andra terminal från projektroten:

```bash
source .venv/bin/activate
python -m uvicorn src.iot_receiver.api:app --host 127.0.0.1 --port 8000
```

API:t nås därefter på `http://127.0.0.1:8000`.

## Starta lokalt med Docker Compose

Följande kommando bygger imagen och startar API och MQTT-mottagare. Det
skapar containrar och skriver lokal runtime-data under `data/`.

```bash
docker compose up --build -d
```

Kontrollera status och loggar med:

```bash
docker compose ps
docker compose logs api mqtt_receiver
```

Stoppa de lokala containrarna utan att radera SQLite-filen:

```bash
docker compose down
```

## Bygg ESP32-firmware lokalt

Öppna en terminal där ESP-IDF 6.0 är installerat. Om miljön inte redan är
laddad, kör ESP-IDF-installationens `export.sh`. Variabeln `IDF_PATH` ska
peka på din lokala ESP-IDF-installation.

```bash
source "$IDF_PATH/export.sh"
cd firmware/esp32
idf.py set-target esp32c6
idf.py menuconfig
idf.py build
```

`set-target` och `menuconfig` skapar eller ändrar den lokala, Git-ignorerade
filen `sdkconfig`. Ange WiFi, MQTT-URI, användarnamn och lösenord där.
`idf.py build` kompilerar och länkar firmwaren men verifierar inte sensorn
eller nätverkskommunikationen.

När ESP32-C6 är ansluten kan firmware flashas och seriell logg öppnas med:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

Anpassa porten till datorn. Avsluta monitorn med `Ctrl+]`.

## Verifiera det lokala dataflödet

Kontrollera först API-hälsan. `curl`-anropen är skrivskyddade och ändrar
ingen data:

```bash
curl -sS http://127.0.0.1:8000/health
curl -sS http://127.0.0.1:8000/api/v1/status
curl -sS http://127.0.0.1:8000/api/v1/readings/latest
curl -sS 'http://127.0.0.1:8000/api/v1/readings?limit=10'
```

Ett komplett test kräver att ESP32-loggen visar en fysisk DHT11-läsning,
MQTT-mottagaren loggar samma meddelande och API:t returnerar den lagrade
mätningen. Det fullständiga förfarandet finns i `testprotokoll.md`.

## Kör automatiska tester

```bash
source .venv/bin/activate
python -m pytest
```

Testerna använder temporära databaser och mockad MQTT. De ansluter inte till
brokern och ändrar inte `data/readings.db`. Se `tests/README.md` för fler
detaljer.

## Dokumentation

[Dokumentationsöversikten](documentation/README.md) länkar till arkitektur,
API, säkerhetsanalys och felsökning. Observerade och återstående verifieringar
finns i [testprotokollet](testprotokoll.md).

## Kända begränsningar

- Telemetrikön ligger i RAM och förloras om ESP32 startas om.
- QoS 1 kan ge dubbletter; mottagaren deduplicerar inte meddelanden.
- API:t har ingen egen autentisering och ska endast exponeras i en betrodd
  lokal miljö tills åtkomstkontroll har införts.
- Godkänt firmwarebygge ersätter inte test med fysisk sensor, nätverk och
  broker.
