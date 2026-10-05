# Automatisk testsvit

Testsviten verifierar Pythonmottagarens datakontrakt, SQLite-lagring,
FastAPI-endpoints och MQTT-konfiguration utan att använda verkligt nätverk
eller verklig projektdata.

## Installation

Kör från projektroten. Kommandona skapar en lokal virtuell miljö och
installerar testberoenden i den; de ändrar inte produktionsdata.

```bash
python -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements-dev.txt
```

## Körning

Kör hela sviten:

```bash
python -m pytest
```

Kör en del av sviten genom att ange filen:

```bash
python -m pytest tests/test_validation.py
python -m pytest tests/test_database.py
python -m pytest tests/test_api.py
python -m pytest tests/test_mqtt_receiver.py
```

Flaggan `-q` ger kortare utdata och `-v` visar varje testnamn:

```bash
python -m pytest -q
python -m pytest -v
```

## Isolering

- Varje databastest använder en ny databas under pytest:s temporära katalog.
- Både mottagaren och API-funktionerna pekas om till samma temporära databas.
- FastAPI:s byggda frågefältsmodell testas direkt för `limit`-validering;
  riktiga HTTP-statusar ingår i det manuella lokala API-testet.
- MQTT-klienten ersätts med en fejkklient i starttestet.
- Inga tester läser `.env`, kontaktar brokern eller använder ESP32.
- `data/readings.db` ska därför ha oförändrad checksumma före och efter en
  testkörning.

Manuella firmware-, TLS-, Compose- och end-to-end-tester dokumenteras i
`testprotokoll.md`.
