# Testprotokoll

## Syfte och statusregler

Protokollet visar hur lösningen verifieras mot kraven i
`Kravspecifikation/Inlämningsuppgift.pdf`. Automatiska tester körs lokalt
med pytest. Firmware, fysisk sensor, broker, TLS och hela dataflödet kräver
separata manuella tester.

Tillåtna statusvärden är:

- **Ej körd**: testet är definierat men saknar en ny observerad körning.
- **Godkänd**: förväntat resultat observerades och bevis anges.
- **Underkänd**: resultatet avvek från det förväntade.
- **Blockerad**: testet kunde inte köras och orsaken anges.

Historiska uppgifter i `docs/STARTPROMPT.md` räknas inte som nya testbevis.
Loggutdrag och skärmbilder ska rensas från användarnamn, lösenord, personliga
värdnamn och andra hemligheter innan de sparas.

## Kravspårning

| Krav | Test-ID | Verifiering |
|---|---|---|
| Fysisk IoT-enhet och verkligt mätvärde | M-02, M-03 | DHT11-läsning och sammanhängande dataflöde |
| Periodisk överföring | M-02, M-03 | Flera mätningar med förväntat intervall |
| MQTT och dokumenterad adressering | M-03, M-04 | TLS-port, topics, QoS och klient-ID |
| Strukturerad och konsekvent JSON | A-01, A-02, M-03 | Automatiska kontraktstester och verklig payload |
| API-integration och felhantering | A-04, M-03 | Endpointtester, 404 och 422 |
| Säker kommunikation | M-05, M-10 | TLS-kedja, hostname, hemlighetshantering |
| Loggning | A-02, A-05, M-08 | Validerings-, anslutnings- och kommunikationsloggar |
| Övervakning | A-04, M-09 | Statusendpoint och kö-/felräknare |
| Två avsiktliga fel | F-01, F-02 | Ogiltig JSON och nätverksavbrott |
| Installation och lokal körning | M-01, M-06, M-07 | Firmwarebygge, Python och Compose |
| Robust kommunikationshantering | M-04, F-02 | Last Will, återanslutning, backoff och köning |

## Automatiska tester

### A-01 – Giltigt datakontrakt

- **Krav:** Strukturerad data och konsekvent datakontrakt.
- **Syfte:** Verifiera att firmwareformatet med sex fält godtas.
- **Förutsättningar:** Utvecklingsberoenden installerade.
- **Steg:** Kör `python -m pytest tests/test_validation.py`.
- **Förväntat:** Giltig payload, gränsvärden samt tidsstämplar med `Z` och
  UTC-offset godtas.
- **Faktiskt resultat:** Samtliga giltiga kontraktsfall passerade som del av
  den fullständiga sviten med 63 godkända tester.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-03, `python -m pytest`.

### A-02 – Ogiltig payload och loggning

- **Krav:** Validering, loggning och kommunikationsfel.
- **Syfte:** Säkerställa att felaktig data avvisas utan lagring.
- **Förutsättningar:** Utvecklingsberoenden installerade.
- **Steg:** Kör `python -m pytest tests/test_validation.py`.
- **Förväntat:** Trasig JSON, fel typ, saknade fält, ogiltig tidsstämpel,
  fel enhet och värden utanför intervall avvisas och loggas.
- **Faktiskt resultat:** Samtliga negativa payloadfall passerade. Mockad
  lagring förblev tom och förväntade varningar fångades.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-03, `python -m pytest`.

### A-03 – SQLite-lagring

- **Krav:** Behandling och säker lagring av extern data.
- **Syfte:** Verifiera tabellskapande, lagring och parametriserade frågor.
- **Förutsättningar:** Utvecklingsberoenden installerade.
- **Steg:** Kör `python -m pytest tests/test_database.py`.
- **Förväntat:** Temporär tabell kan initieras flera gånger och SQL-tecken i
  ett sensor-ID lagras som data utan att ändra tabellen.
- **Faktiskt resultat:** Tre databastester passerade mot separata temporära
  databaser. Checksumkontroll visade att `data/readings.db` var oförändrad.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-03, `python -m pytest` samt checksumma före
  och efter körningen.

### A-04 – REST-API

- **Krav:** API-operation, svar, felhantering och övervakning.
- **Syfte:** Verifiera samtliga endpoints mot en isolerad databas.
- **Förutsättningar:** Utvecklingsberoenden installerade.
- **Steg:** Kör `python -m pytest tests/test_api.py`.
- **Förväntat:** Endpointfunktionerna ger rätt data, tom databas skapar ett
  404-undantag och FastAPI:s frågefältsmodell avvisar ogiltig `limit`.
- **Faktiskt resultat:** Elva API-tester passerade. Faktiska HTTP-svar ingår
  fortfarande i det manuella lokala testet M-06.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-03, `python -m pytest`.

### A-05 – MQTT-konfiguration och callbacks

- **Krav:** Konfiguration, anslutning, adressering och loggning.
- **Syfte:** Verifiera startvalidering utan nätverkskontakt.
- **Förutsättningar:** Utvecklingsberoenden installerade.
- **Steg:** Kör `python -m pytest tests/test_mqtt_receiver.py`.
- **Förväntat:** Saknade eller ogiltiga portar avvisas; giltig konfiguration
  når den mockade klienten och rätt topics prenumereras.
- **Faktiskt resultat:** Nio tester passerade med mockad MQTT-klient och utan
  nätverksanslutning.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-03, `python -m pytest`.

## Manuella testfall

### M-01 – Lokalt firmwarebygge

- **Krav:** Byggbar IoT-programvara.
- **Syfte:** Verifiera kompilering och länkning för ESP32-C6.
- **Lokala förutsättningar:** ESP-IDF 6.0 är installerat och miljön laddad.
- **Steg:** Kör `idf.py build` från `firmware/esp32/`.
- **Förväntat:** Kommandot slutar utan fel och skapar firmwareartefakter.
- **Faktiskt resultat:** ESP-IDF 6.0 kompilerade och länkade projektet samt
  skapade `iot_sensor.bin`. Bygget rapporterade en lokal `sdkconfig`-notis:
  larmkön är 20 där aktuell Kconfig-standard är 10; detta stoppade inte
  bygget och `sdkconfig` är inte versionshanterad.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-03, `idf.py build` avslutades med
  `Project build complete`.

### M-02 – Startordning och fysisk DHT11

- **Krav:** Fysisk sensor, periodisk avläsning och NTP före TLS.
- **Syfte:** Verifiera verklig sensor och säker startordning.
- **Lokala förutsättningar:** Firmware är flashad; ESP32-C6 och DHT11 är
  anslutna; WiFi och NTP är tillgängliga.
- **Steg:** Starta seriell monitor, starta om kortet och observera minst tre
  mätintervall.
- **Förväntat:** Loggen visar WiFi, NTP, MQTT och telemetry i den ordningen
  samt tre lyckade fysiska mätningar med ungefär konfigurerat intervall.
- **Faktiskt resultat:** Inte registrerat.
- **Status:** Ej körd.
- **Datum och bevis:** Anteckna tider och relevanta anonymiserade loggrader.

### M-03 – Sammanhängande end-to-end-flöde

- **Krav:** Fungerande dataflöde, JSON, MQTT, lagring och API.
- **Syfte:** Följa samma fysiska mätning genom hela systemet.
- **Lokala förutsättningar:** ESP32 kör, TLS-brokern är nåbar och de lokala
  Python-tjänsterna eller Compose är startade.
- **Steg:** Notera ett sensor-ID och en tidsstämpel i ESP32-loggen; bekräfta
  samma payload i mottagarloggen; hämta senaste mätningen och historiken via
  API:t.
- **Förväntat:** Samma sex fält och värden kan följas från DHT11 till API,
  och minst tre periodiska poster lagras.
- **Faktiskt resultat:** Inte registrerat.
- **Status:** Ej körd.
- **Datum och bevis:** Spara anonymiserade loggutdrag och API-svar.

### M-04 – MQTT-identitet och status

- **Krav:** Topics, klient-ID, QoS, keepalive, Last Will och onlinestatus.
- **Syfte:** Verifiera MQTT-kontraktet och brokerstatus.
- **Lokala förutsättningar:** Auktoriserad MQTT-prenumerant och ESP32 är
  anslutna till brokern.
- **Steg:** Prenumerera på `esp-test/<device-id>/#`; starta om enheten;
  observera status och telemetri utan att avsiktligt bryta nätverket.
- **Förväntat:** MAC-baserat stabilt ID används; retained `online` finns på
  status-topic; telemetri ligger på rätt topic med QoS 1.
- **Faktiskt resultat:** Inte registrerat.
- **Status:** Ej körd.
- **Datum och bevis:** Anteckna anonymiserat topic, status och message-ID.

### M-05 – TLS-kedja och hostname

- **Krav:** Grundläggande säker kommunikation.
- **Syfte:** Bekräfta att båda MQTT-klienterna verifierar root-CA och hostname.
- **Lokala förutsättningar:** Broker-certifikatets SAN och betrodda root-CA
  är kända; inga hemligheter skrivs i protokollet.
- **Steg:** Anslut Pythonmottagare och ESP32 med korrekt värdnamn; upprepa
  separat med fel CA eller ett nåbart namn/IP som inte finns i certifikatets
  SAN; återställ därefter lokal konfiguration.
- **Förväntat:** Korrekt konfiguration ansluter. Fel CA eller hostname ger
  TLS-fel och ingen osäker fallback eller publicering.
- **Faktiskt resultat:** Inte registrerat.
- **Status:** Ej körd.
- **Datum och bevis:** Spara endast certifikatets publika metadata och
  anonymiserade felmeddelanden.

### M-06 – Lokal Pythonstart

- **Krav:** En annan utvecklare ska kunna installera och köra lösningen.
- **Syfte:** Verifiera README:s instruktioner utan Pi-specifika steg.
- **Lokala förutsättningar:** Ren lokal virtuell miljö och lokal `.env`.
- **Steg:** Följ avsnitten Pythonmiljö, konfiguration och lokal start i
  `README.md`; anropa `/health` och `/api/v1/status`.
- **Förväntat:** Båda processerna startar och endpointsen svarar med 200.
- **Faktiskt resultat:** Inte registrerat.
- **Status:** Ej körd.
- **Datum och bevis:** Anteckna Pythonversion och HTTP-status.

### M-07 – Lokal Docker Compose

- **Krav:** Installation, körning, övervakning och beständig lagring.
- **Syfte:** Verifiera Compose-konfiguration, healthcheck och persistens.
- **Lokala förutsättningar:** Docker och Compose är installerade; lokal
  `.env` finns.
- **Steg:** Kör `docker compose config`; starta med `docker compose up
  --build -d`; invänta healthy; lagra en giltig post; kör `docker compose
  down` och starta igen; hämta samma post via API.
- **Förväntat:** Konfigurationen är giltig, API blir healthy och posten finns
  kvar efter att containrarna återskapats.
- **Faktiskt resultat:** Inte registrerat.
- **Status:** Ej körd.
- **Datum och bevis:** Spara anonymiserad `docker compose ps` och API-svar.

### M-08 – Loggning

- **Krav:** Relevanta anslutnings-, data-, validerings- och felloggar.
- **Syfte:** Kontrollera observerbarhet utan läckta hemligheter.
- **Lokala förutsättningar:** Systemet kör lokalt och en giltig samt en
  ogiltig payload kan skickas.
- **Steg:** Observera anslutning, giltig mätning, valideringsfel, avbrott och
  återanslutning i firmware- och Pythonloggar; sök visuellt efter hemligheter.
- **Förväntat:** Alla händelsetyper loggas med användbar kontext men utan
  lösenord eller tokens.
- **Faktiskt resultat:** Inte registrerat.
- **Status:** Ej körd.
- **Datum och bevis:** Spara korta anonymiserade loggutdrag.

### M-09 – Övervakningsmått

- **Krav:** Minst en övervakningsfunktion.
- **Syfte:** Verifiera statusendpoint och firmwarestatistik.
- **Lokala förutsättningar:** Systemet kör och kan ta emot mätningar.
- **Steg:** Läs `/api/v1/status` före och efter en giltig mätning; observera
  köstorlek och räknare för tappade mätningar i relevant firmwaretest.
- **Förväntat:** `storedReadings` ökar med ett och firmwareloggar aktuell
  kö-/felstatistik.
- **Faktiskt resultat:** Inte registrerat.
- **Status:** Ej körd.
- **Datum och bevis:** Spara API-svaren och anonymiserade loggrader.

### M-10 – Hemligheter och versionshantering

- **Krav:** Inga autentiseringsuppgifter i Git.
- **Syfte:** Kontrollera att endast exempelvärden och publik CA finns spårade.
- **Lokala förutsättningar:** Kör från projektroten.
- **Steg:** Kör `git ls-files` och bekräfta att `.env`, `sdkconfig`, privata
  nycklar och lösenordsfiler saknas. Använd vid behov `git grep -l` så att
  endast filnamn, inte matchande hemliga värden, visas.
- **Förväntat:** Inga hemliga filer eller riktiga autentiseringsuppgifter är
  spårade; `.env.example` innehåller endast neutrala platshållare.
- **Faktiskt resultat:** Inte registrerat.
- **Status:** Ej körd.
- **Datum och bevis:** Anteckna kontrollerade filtyper, inte hemliga värden.

## Avsiktligt fel F-01 – Ogiltig JSON

- **Kopplade krav:** Validering, loggning och felsökning.
- **Förutsättningar:** Broker, mottagare och API kör; startvärdet från
  `/api/v1/status` är noterat. Använd ett topic som testkontot har rätt till.
- **Införande:** Publicera exempelvis `{invalid-json` på
  `esp-test/<device-id>/telemetry` med en lokalt konfigurerad MQTT-klient.
- **Förväntat:** Varning om ogiltig JSON, mottagaren fortsätter köra och
  `storedReadings` ändras inte.

Obligatorisk felanalys efter körning:

1. **Observerat symptom:** Ej registrerat.
2. **Hur felet identifierades:** Ej registrerat.
3. **Verktyg eller loggar:** Ej registrerat.
4. **Felets orsak:** Avsiktligt syntaktiskt ogiltig JSON.
5. **Genomförd åtgärd:** Avsändaren ska rätta payloaden till dokumenterat
   sexfältsformat; mottagaren ska fortsätta utan omstart.
6. **Verifiering av åtgärd:** Ej registrerat; skicka därefter en giltig
   payload och verifiera exakt en ny post.

- **Faktiskt resultat:** Inte registrerat.
- **Status:** Ej körd.
- **Datum och bevis:** Saknas.

## Avsiktligt fel F-02 – Oväntat nätverksavbrott

- **Kopplade krav:** Kommunikationsfel, robust återanslutning, Last Will,
  loggning och felsökning.
- **Förutsättningar:** Broker och separat prenumerant förblir igång; ESP32
  har varit ansluten mer än 60 sekunder; sensorintervallet är känt.
- **Införande:** Bryt ESP32:s nätverkskontakt oväntat utan att stoppa brokern,
  och håll avbrottet längre än minst ett sensorintervall. Återställ sedan
  nätverket.
- **Förväntat:** Brokern publicerar retained `offline`; ESP32 loggar WiFi-
  och MQTT-fel och behåller mätningar i RAM-kön; efter återställning syns
  backoff, retained `online` och nya eller köade poster i API:t.

Obligatorisk felanalys efter körning:

1. **Observerat symptom:** Ej registrerat.
2. **Hur felet identifierades:** Ej registrerat.
3. **Verktyg eller loggar:** Ej registrerat.
4. **Felets orsak:** Avsiktligt avbruten nätverkskontakt mellan ESP32 och
   brokern medan brokern fortsatte köra.
5. **Genomförd åtgärd:** Återställ nätverket; klienterna ska återansluta utan
   omstart eller osäker fallback.
6. **Verifiering av åtgärd:** Ej registrerat; kontrollera `online`,
   återanslutningslogg och en efterföljande fysisk mätning i API:t.

- **Faktiskt resultat:** Inte registrerat.
- **Status:** Ej körd.
- **Datum och bevis:** Saknas.

## Sammanfattning

Inga manuella tester eller avsiktliga fel är godkända förrän fälten för
faktiskt resultat, datum och bevis har fyllts i efter en ny körning. Ett
underkänt test behåller statusen **Underkänd** tills orsaken har åtgärdats och
samma test har körts om.
