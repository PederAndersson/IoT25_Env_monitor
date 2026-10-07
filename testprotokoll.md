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

### Regression 2026-10-07

Hela den automatiska sviten kördes med Python 3.14.8 och pytest 9.1.1 efter
firmware-, mottagar- och dokumentationsarbetet. Samtliga 68 tester passerade
på 0,09 sekunder: 11 API-tester, 5 databastester, 11 MQTT-mottagartester och 41
valideringstester. Testerna använde isolerade eller mockade beroenden enligt
`tests/README.md`.

### A-01 – Giltigt datakontrakt

- **Krav:** Strukturerad data och konsekvent datakontrakt.
- **Syfte:** Verifiera att firmwareformatet med sex fält godtas.
- **Förutsättningar:** Utvecklingsberoenden installerade.
- **Steg:** Kör `python -m pytest tests/test_validation.py`.
- **Förväntat:** Giltig payload, gränsvärden samt tidsstämplar med `Z` och
  UTC-offset godtas.
- **Faktiskt resultat:** Samtliga giltiga kontraktsfall passerade som del av
  den fullständiga sviten med 68 godkända tester.
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
- **Förväntat:** Temporär tabell kan initieras flera gånger, SQL-tecken i ett
  sensor-ID lagras som data utan att ändra tabellen och en redan lagrad
  kombination av sensor-ID och tidsstämpel lagras inte igen.
- **Faktiskt resultat:** Fem databastester passerade mot separata temporära
  databaser. De verifierade även att en dubblett ignoreras medan två olika
  sensorer med samma tidsstämpel lagras. Checksumkontroll visade att
  `data/readings.db` var oförändrad.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-03 och 2026-10-07, `python -m pytest` samt
  checksumma före och efter körningen.

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
- **Datum och bevis:** 2026-10-03 och 2026-10-07, `python -m pytest`.

### A-05 – MQTT-konfiguration och callbacks

- **Krav:** Konfiguration, anslutning, adressering och loggning.
- **Syfte:** Verifiera startvalidering utan nätverkskontakt.
- **Förutsättningar:** Utvecklingsberoenden installerade.
- **Steg:** Kör `python -m pytest tests/test_mqtt_receiver.py`.
- **Förväntat:** Saknade eller ogiltiga portar avvisas; giltig konfiguration
  når den mockade klienten, endast telemetritopic prenumereras med QoS 1 och
  övriga topics ignoreras av callbacken.
- **Faktiskt resultat:** Elva tester passerade med mockad MQTT-klient och utan
  nätverksanslutning. Topicfiltrering och loggning av QoS-, dubblett- och
  meddelande-ID-metadata verifierades.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-03 och 2026-10-07, `python -m pytest`.

## Manuella testfall

### M-01 – Lokalt firmwarebygge

- **Krav:** Byggbar IoT-programvara.
- **Syfte:** Verifiera kompilering och länkning för ESP32-C6.
- **Lokala förutsättningar:** ESP-IDF 6.0 är installerat och miljön laddad.
- **Steg:** Kör `idf.py build` från `firmware/esp32/`.
- **Förväntat:** Kommandot slutar utan fel och skapar firmwareartefakter.
- **Faktiskt resultat:** ESP-IDF 6.0 kompilerade och länkade projektet samt
  skapade `iot_sensor.bin`. Efter WiFi-ändringen `9d64205` stoppades det
  första bygget av att interna FreeRTOS-headern `freertos/projdefs.h`
  inkluderades direkt före `freertos/FreeRTOS.h`. Den överflödiga direkta
  inkluderingen togs bort och ett nytt bygge avslutades med
  `Project build complete`. Firmwaren flashades och serielloggen visade
  appversion `9d64205-dirty`; suffixet beror på lokala, ännu ocommittade
  ändringar.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-07, `idf.py build` avslutades med
  `Project build complete` och den flashade enheten startade den byggda
  appversionen.

### M-02 – Startordning och fysisk DHT11

- **Krav:** Fysisk sensor, periodisk avläsning och NTP före TLS.
- **Syfte:** Verifiera verklig sensor och säker startordning.
- **Lokala förutsättningar:** Firmware är flashad; ESP32-C6 och DHT11 är
  anslutna; WiFi och NTP är tillgängliga.
- **Steg:** Starta seriell monitor, starta om kortet och observera minst tre
  mätintervall.
- **Förväntat:** Loggen visar WiFi och NTP före MQTT/TLS samt minst tre
  lyckade fysiska mätningar med ungefär konfigurerat intervall. En mätning
  får placeras i RAM-kön medan MQTT ansluter; den får inte publiceras före
  en verifierad MQTT/TLS-anslutning.
- **Faktiskt resultat:** Vid körningen 2026-10-05 blev WiFi redo före NTP,
  NTP synkroniserades före TLS och MQTT anslöt först efter
  certifikatvalidering. Den första DHT11-mätningen placerades i RAM-kön
  medan MQTT anslöt och publicerades därefter. Ytterligare mätningar lästes
  och publicerades med 60 sekunders intervall. Omtestet 2026-10-07 med
  appversion `9d64205-dirty` visade samma säkra startordning, lyckad fysisk
  DHT11-läsning och fortsatt periodisk publicering. Kravspecifikationen
  kräver fysisk och periodisk sensordata men kräver inte att sampling väntar
  på färdig MQTT-anslutning; den tidigare striktare tolkningen har därför
  tagits bort.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-05 och 2026-10-07, ESP-IDF-monitor samt
  skrivskyddad kontroll av `Kravspecifikation/Inlämningsuppgift.pdf`.
  Råloggar innehåller lokala nätverksidentifierare och ska inte läggas till
  i Git.

### M-03 – Sammanhängande end-to-end-flöde

- **Krav:** Fungerande dataflöde, JSON, MQTT, lagring och API.
- **Syfte:** Följa samma fysiska mätning genom hela systemet.
- **Lokala förutsättningar:** ESP32 kör, TLS-brokern är nåbar och de lokala
  Python-tjänsterna eller Compose är startade.
- **Steg:** Notera sensor-ID och en tidsstämplad publicering i ESP32-loggen;
  jämför en tidsmässigt matchande payload i mottagarloggen med mätningen i
  API-historiken. Firmwareloggen skriver inte ut hela JSON-payloaden.
- **Förväntat:** Samma sex fält och värden kan följas från DHT11 till API,
  och minst tre periodiska poster lagras.
- **Faktiskt resultat:** ESP32 köade en fysisk sensormätning 14:16:22 lokal
  tid (12:16:22 UTC) och rapporterade MQTT-publicering 14:16:25. Den lokala
  mottagaren tog emot `esp-test/<device-id>/telemetry` 12:16:25–26 UTC och
  validerade sex fält med tidsstämpeln `2026-10-05T12:16:22Z`, luftfuktighet
  48,0 % och temperatur 25,0 °C. API-historiken returnerade samma fält och
  värden. Mottagarloggen visade även mätningar ungefär varje minut 12:09–12:13;
  API-status ökade från 12 till 13 poster mellan mätningarna 12:10:18Z och
  12:11:18Z.
  **Historisk avvikelse O-01:** Mätningen 12:16:22Z togs emot och lagrades tre
  gånger (databas-ID 22–24), trots en köning och en publiceringsbekräftelse
  i den fångade firmwareloggen. Sju tidsstämplar hade 2–4 poster vardera
  vid senare kontroll. Mottagaren sparar varje mottaget meddelande separat;
  orsaken till upprepade leveranser är inte fastställd.
- **Utredningsläge O-01, 2026-10-06:** Firmware köar telemetri med QoS 1.
  Pythonmottagaren prenumererar utan angivet QoS och får därför som standard
  QoS 0 på prenumerationen. ESP-MQTT kan sända om okvitterade QoS 1-
  meddelanden. Det kan förklara varför en köning och en slutlig kvittens
  sammanföll med flera mottagna kopior. Det är en hypotes, inte en fastställd
  orsak; brokerlogg eller paketspårning för samma publicering saknas. MQTT:s
  `message_id` gäller en förbindelse och är inte ett end-to-end-ID.
- **Åtgärd och omtest O-01, 2026-10-07:** En kontroll före åtgärden fann 121
  databasrader men 89 unika kombinationer av sensor-ID och tidsstämpel, alltså
  32 historiska överskottsrader. Mottagaren fick atomärt dubblettskydd för
  denna kombination. Vid runtime-omtest publicerades samma giltiga syntetiska
  QoS 1-payload två gånger. Båda leveranserna togs emot, den andra loggades som
  ignorerad dubblett och exakt en rad lagrades. Testraden raderades efter
  kontrollen. De historiska dubbletterna bevarades och den exakta orsaken till
  deras transport kan inte fastställas utan äldre brokerlogg eller paketspårning.
- **Status:** Godkänd för sammanhängande sexfältsflöde, periodisk lagring och
  skydd mot framtida dubblettlagring i den nuvarande mottagartjänsten. O-01 är
  åtgärdad på lagringsnivå; den historiska transportorsaken är inte fastställd.
- **Datum och bevis:** 2026-10-05 och omtest 2026-10-07, ignorerad rålogg
  `data/esp32-monitor-2026-10-05-e2e.log`, anonymiserade mottagarloggar
  12:09–12:16 UTC och svar från `/api/v1/readings?limit=10` samt
  `/api/v1/status`. Råloggar med nätverksidentifierare ska inte läggas till
  i Git.

### M-04 – MQTT-identitet och status

- **Krav:** Topics, klient-ID, QoS, keepalive, Last Will och onlinestatus.
- **Syfte:** Verifiera MQTT-kontraktet och brokerstatus.
- **Lokala förutsättningar:** Auktoriserad MQTT-prenumerant och ESP32 är
  anslutna till brokern.
- **Steg:** Prenumerera på `esp-test/<device-id>/#`; starta om enheten;
  observera status och telemetri utan att avsiktligt bryta nätverket.
- **Förväntat:** MAC-baserat stabilt ID används; retained `online` finns på
  status-topic; telemetri ligger på rätt topic med QoS 1.
- **Faktiskt resultat:** Firmware loggade samma enhets-ID och topics vid
  flera starter. En separat prenumerant fick `online retained=True` och
  QoS 1 vid anslutning 12:50:46 UTC. Under F-02 kom live-`offline` med
  QoS 1; en ny prenumerant fick `offline retained=True`. Telemetry-topic
  verifierades i mottagarloggen. En skrivskyddad kodkontroll 2026-10-07
  bekräftade stabilt client ID, Last Will och online-status med QoS 1 och
  retain, telemetriköning med QoS 1 samt 60 sekunders keepalive. En separat
  TLS-verifierad prenumerant mot firmwarens endpoint tog därefter emot en
  fysisk telemetripayload med QoS 1 och `retained=False`.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-05 och 2026-10-07, anonymiserade
  prenumerantrader, firmware-/mottagarloggar och `mqtt_service.c`.
  Enhets-ID maskerat i detta protokoll.

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
- **Faktiskt resultat:** ESP32 loggade certifikatvalidering före MQTT-
  anslutning. ESP32 anslöt även på nytt med verifierat certifikat under
  F-02. En ny Pythonanslutning från värdmiljön avvisades 2026-10-05 med
  `SSLCertVerificationError: Hostname mismatch`. Felet reproducerades
  2026-10-07 i en helt nybyggd Compose-image: MQTT-mottagaren avvisade
  broker-certifikatet och hamnade i omstartsloop. Ingen osäker fallback
  användes. En separat TLS-kontroll visade att Pythonmottagaren och ESP32
  använder samma host men olika portar. Mottagarens port hade en kedja som
  `certs/ca.crt` litade på men fel hostname, medan firmwarens port hade rätt
  hostname men en annan, publik certifikatkedja. En tillfällig klient med
  systemets CA-lager anslöt säkert till firmwarens endpoint. Den felande
  Compose-tjänsten stoppades efter observationen. Den lokala `.env`-filen
  korrigerades därefter till firmwarens endpoint och systemets CA-lager.
  En återskapad mottagarcontainer anslöt då med TLS och förblev stabil.
  Ett isolerat negativt test mot samma endpoint använde avsiktligt fel CA
  och avvisades med `SSLCertVerificationError` utan fallback.
- **Status:** Godkänd. Korrekt endpoint verifierar både kedja och hostname;
  fel hostname och fel CA har båda observerats bli avvisade.
- **Datum och bevis:** 2026-10-05 och 2026-10-07, ESP32-logg, nybyggd
  Compose-mottagare, OpenSSL-kontroller och isolerad negativ Pythonklient.
  Inga autentiseringsuppgifter återges här.

### M-06 – Lokal Pythonstart

- **Krav:** En annan utvecklare ska kunna installera och köra lösningen.
- **Syfte:** Verifiera README:s instruktioner utan Pi-specifika steg.
- **Lokala förutsättningar:** Ren lokal virtuell miljö och lokal `.env`.
- **Steg:** Följ avsnitten Pythonmiljö, konfiguration och lokal start i
  `README.md`; anropa `/health` och `/api/v1/status`.
- **Förväntat:** Båda processerna startar och endpointsen svarar med 200.
- **Faktiskt resultat:** En ny virtuell miljö skapades under `/tmp` med
  Python 3.14.8 och samtliga versionslåsta produktionsberoenden installerades
  från `requirements.txt`. MQTT-mottagaren startade enligt README, anslöt
  med TLS samt tog emot och validerade en fysisk telemetripayload. Den
  kraschade därefter med `sqlite3.OperationalError: attempt to write a
  readonly database`. `data/readings.db` ägdes efter Compose-körningen av
  `nobody:nobody` med läge `644`, medan värdanvändaren ägde katalogen men
  saknade skrivrättighet till filen. API:t startade lokalt på
  `127.0.0.1:8000`; `/health` och `/api/v1/status` gav HTTP 200 och status
  visade 115 befintliga poster. Compose ändrades därefter till att köra båda
  tjänsterna med konfigurerbara `HOST_UID` och `HOST_GID`, normalt
  `1000:1000`. Den befintliga databasen ersattes med en byte-identisk,
  integritetskontrollerad kopia med rätt ägarskap; originalet sparades som
  lokal backup. Compose skrev därefter post 116 utan att ändra ägarskapet.
  Vid omtest från samma rena Pythonmiljö anslöt mottagaren, validerade och
  lagrade flera nya fysiska mätningar utan SQLite-fel. API:t gav fortsatt
  HTTP 200 och returnerade post 119. Databasen förblev `1000:1000`.
- **Status:** Godkänd efter åtgärd och omtest.
- **Datum och bevis:** 2026-10-07, ren virtuell miljö, mottagar-/API-loggar,
  HTTP 200-svar, API-historik, SQLite-integritetskontroll, identiska
  SHA-256-kontrollsummor före ersättning samt `stat` av databasfilen.

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
- **Faktiskt resultat:** `docker compose config -q` avslutades utan fel och
  `docker compose up --build -d` byggde aktuell image från grunden samt
  återskapade båda containrarna. API:t blev healthy, `/health` gav HTTP 200
  och den återskapade containern läste den beständiga databasen med 111
  poster och senaste post 111. MQTT-mottagaren startade om upprepade gånger
  på det verifierade hostname-felet i M-05. Efter
  korrigerad lokal `.env` återskapades endast mottagaren; båda tjänsterna
  förblev `Up`, API:t var healthy och mottagaren anslöt med TLS. En ny
  fysisk mätning validerades och lagrades som post 112 medan den tidigare
  posten 111 fanns kvar efter containeråterskapandet. Ett ytterligare
  explicit `down`/`up` genomfördes inte, men båda containrarna återskapades
  av `up --build -d` och persistensen observerades efter återskapandet.
  Efter UID/GID-ändringen återskapades båda tjänsterna som `1000:1000`;
  API:t blev healthy och mottagaren lagrade post 116 medan databasfilens
  ägarskap förblev `1000:1000`.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-07, Compose build-/statusutdata, HTTP 200,
  `/api/v1/status`, API-historik och mottagarens anslutnings-/datalogg.

### M-08 – Loggning

- **Krav:** Relevanta anslutnings-, data-, validerings- och felloggar.
- **Syfte:** Kontrollera observerbarhet utan läckta hemligheter.
- **Lokala förutsättningar:** Systemet kör lokalt och en giltig samt en
  ogiltig payload kan skickas.
- **Steg:** Observera anslutning, giltig mätning, valideringsfel, avbrott och
  återanslutning i firmware- och Pythonloggar; sök visuellt efter hemligheter.
- **Förväntat:** Alla händelsetyper loggas med användbar kontext men utan
  lösenord eller tokens.
- **Faktiskt resultat:** Firmware loggade WiFi/MQTT-anslutning,
  publicering, kommunikationsfel och köstorlek. Pythonmottagaren loggade
  mottagen och validerad data samt avvisad ogiltig JSON. Vid omtestet av
  F-02 loggades ett långt WiFi-avbrott, fortsatta anslutningsförsök,
  backoff upp till 30 sekunder och lyckad WiFi-, TLS- och MQTT-
  återanslutning.
  Den tidigare mottagaren prenumererade även på status-topic och försökte
  därför tolka payloadarna `online` och `offline` som sensor-JSON. Det gav
  missvisande varningar om ogiltig JSON och registrerades som O-02.
  Vid omtest 2026-10-07 begränsades prenumerationen till
  `esp-test/+/telemetry` med QoS 1, och callbacken fick ett separat skydd som
  ignorerar andra topics. Efter återanslutning kom ingen retained statuspayload
  till telemetricallbacken och ingen falsk JSON-varning uppstod. En fysisk
  telemetripayload togs emot och loggades med QoS-, dubblett- och
  meddelande-ID-metadata.
  Runtime-loggarna granskades utan observerade lösenord eller tokens, och en
  statisk sökning hittade inga logganrop som refererar till konfigurerade
  WiFi- eller MQTT-lösenord. Råa firmwareloggar innehåller däremot lokala
  nätverksidentifierare och måste anonymiseras före versionshantering.
- **Status:** Godkänd. O-02 är åtgärdad och omtestad.
- **Datum och bevis:** 2026-10-05 och 2026-10-07, anonymiserade loggutdrag i
  M-02, M-03, F-01 och F-02.

### M-09 – Övervakningsmått

- **Krav:** Minst en övervakningsfunktion.
- **Syfte:** Verifiera statusendpoint och firmwarestatistik.
- **Lokala förutsättningar:** Systemet kör och kan ta emot mätningar.
- **Steg:** Läs `/api/v1/status` före och efter en giltig mätning; observera
  köstorlek och räknare för tappade mätningar i relevant firmwaretest.
- **Förväntat:** `storedReadings` ökar med ett och firmwareloggar aktuell
  kö-/felstatistik.
- **Faktiskt resultat:** `/api/v1/status` ökade från 12 till 13 lagrade
  poster mellan mätningarna 12:10:18Z och 12:11:18Z. Under F-02 visade
  firmware kön växa till 8/60 samt kommunikationsfel. Sedan O-01-åtgärden
  ignorerar mottagaren nya dubbletter med samma sensor-ID och tidsstämpel.
  Historiska dubblettrader finns kvar; tappade mätningar provocerades inte
  eftersom kön inte fylldes.
- **Status:** Godkänd för statusmått och köövervakning.
- **Datum och bevis:** 2026-10-05, två lokala API-svar och ignorerad
  firmwarelogg `data/esp32-monitor-2026-10-05-f02.log`.

### M-10 – Hemligheter och versionshantering

- **Krav:** Inga autentiseringsuppgifter i Git.
- **Syfte:** Kontrollera att endast exempelvärden och publik CA finns spårade.
- **Lokala förutsättningar:** Kör från projektroten.
- **Steg:** Kör `git ls-files` och bekräfta att `.env`, `sdkconfig`, privata
  nycklar och lösenordsfiler saknas. Använd vid behov `git grep -l` så att
  endast filnamn, inte matchande hemliga värden, visas.
- **Förväntat:** Inga hemliga filer eller riktiga autentiseringsuppgifter är
  spårade; `.env.example` innehåller endast neutrala platshållare.
- **Faktiskt resultat:** Varken aktuell Git-trädvy eller filnamnshistorik
  innehöll `.env`, `sdkconfig`, privata nyckelfiler, lösenordsfiler eller
  andra kontrollerade hemlighetsfiler. `.env` och
  `firmware/esp32/sdkconfig` verifierades som ignorerade. `.env.example`
  använder neutrala platshållare och WiFi-/MQTT-Kconfig har tomma
  standardvärden för autentiseringsuppgifter. Den enda spårade filen under
  `certs/` är `ca.crt`; OpenSSL bekräftade att den är projektets publika,
  självsignerade root-CA och ingen privat nyckelheader hittades.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-07, skrivskyddade kontroller med
  `git ls-files`, `git check-ignore`, `git grep`, `git log --all`, `rg` och
  `openssl x509`. Endast filnamn, ignore-regler och certifikatets publika
  subject/issuer kontrollerades; inga hemliga värden skrevs ut.

## Avsiktligt fel F-01 – Ogiltig JSON

- **Kopplade krav:** Validering, loggning och felsökning.
- **Förutsättningar:** Broker, mottagare och API kör; startvärdet från
  `/api/v1/status` är noterat. Använd ett topic som testkontot har rätt till.
- **Införande:** Publicera exempelvis `{invalid-json` på
  `esp-test/<device-id>/telemetry` med en lokalt konfigurerad MQTT-klient.
- **Förväntat:** Varning om ogiltig JSON, mottagaren fortsätter köra och
  `storedReadings` ändras inte.

Obligatorisk felanalys efter körning:

1. **Observerat symptom:** Mottagaren loggade `Payload contains invalid JSON`
   när testmeddelandet kom, men processen fortsatte köra.
2. **Hur felet identifierades:** Ett avsiktligt `{invalid-json` publicerades
   med QoS 1 på tillåtet telemetry-topic. En omkörning visade 64 lagrade
   poster både före och två sekunder efter felmeddelandet.
3. **Verktyg eller loggar:** Lokal TLS-verifierad MQTT-testklient,
   `docker compose logs` och skrivskyddad SQLite-räkning.
4. **Felets orsak:** Avsiktligt syntaktiskt ogiltig JSON.
5. **Genomförd åtgärd:** Mottagaren avvisade payloaden utan att startas om;
   därefter fortsatte den fysiska avsändaren med giltiga sexfältsmeddelanden.
6. **Verifiering av åtgärd:** Antalet poster förblev 64 → 64 vid omkörningen.
   Efter första felinjektionen validerades nya sensormeddelanden kl. 12:34:22
   och 12:35:22 UTC. Exakt en rad per giltig mätning påstås inte på grund av
   den separata dubblettavvikelsen O-01.

- **Faktiskt resultat:** Ogiltig JSON nådde mottagaren och gav en varning;
  varken processens fortsättning eller antalet lagrade poster påverkades av
  felmeddelandet.
- **Status:** Godkänd.
- **Datum och bevis:** 2026-10-05; anonymiserade mottagarloggar kl. 12:33:41,
  12:34:22, 12:35:22 och 12:41:42 UTC samt före-/efterräkning 64 → 64.

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

1. **Observerat symptom:** I den ursprungliga körningen 2026-10-05 slutade
   ESP32 försöka ansluta efter fem WiFi-försök och återhämtade sig inte när
   hotspotten återkom. I omtestet 2026-10-07 fortsatte försöken under hela
   avbrottet och enheten återanslöt automatiskt.
2. **Hur felet identifierades:** ESP-IDF-monitorn visade i den ursprungliga
   körningen att försöken stannade vid 5/5. Vid omtestet visade samma monitor
   fortsatta försök med väntetider omkring 1,7, 2,5, 5,0, 8,5 och 16,1
   sekunder och därefter 30 sekunders tak. En separat TLS-verifierad
   MQTT-prenumerant fick brokerpublicerad `offline` med `retained=True` och
   QoS 1.
3. **Verktyg eller loggar:** ESP-IDF-monitor, tillfällig TLS-verifierad
   MQTT-statusprenumerant i mottagarcontainern, mottagarens Compose-logg och
   lokalt REST-API.
4. **Felets orsak:** Den tidigare WiFi-logiken stoppade försöken när
   `CONFIG_APP_WIFI_MAXIMUM_RETRY` nåddes. MQTT väntade då korrekt på WiFi,
   men ingen del fortsatte skapa nya WiFi-anslutningsförsök.
5. **Genomförd åtgärd:** En separat WiFi-återanslutningstask infördes i
   commit `9d64205`. Efter en tidigare lyckad IP-anslutning fortsätter den
   försöka med exponentiell väntan, jitter och högst 30 sekunders väntetid.
6. **Verifiering av åtgärd:** ESP32 hade varit stabilt ansluten mer än 60
   sekunder innan hotspotten stängdes av. Under avbrottet lästes två fysiska
   DHT11-mätningar och behölls för senare publicering. Efter återställd
   hotspot fick enheten IP, väntade MQTT:s 10-sekunders backoff, validerade
   TLS-certifikatet och anslöt till brokern. Båda köade payloadarna
   kvitterades, validerades av Pythonmottagaren och lagrades som poster 107
   och 108 med mättiderna 18:18:59Z och 18:19:59Z. En liveprenumerant fick
   `online` med QoS 1 och en ny prenumerant fick `online retained=True`.

- **Faktiskt resultat:** Last Will, retained `offline`, felrapportering,
  fortsatt sampling, RAM-kö, exponentiell WiFi-backoff, automatisk WiFi-/
  TLS-/MQTT-återanslutning, retained `online` och leverans genom SQLite till
  REST-API:t fungerade i samma omtest.
- **Status:** Godkänd.
- **Datum och bevis:** Ursprungligt underkänt test 2026-10-05 och godkänt
  omtest 2026-10-07. Bevis observerades i ESP-IDF-monitor, anonymiserade
  statusprenumerationer, `docker compose logs --since=5m mqtt_receiver` och
  `/api/v1/readings?limit=6`. Råloggar med nätverksidentifierare ska inte
  läggas till i Git.

## Sammanfattning

Endast faktiskt observerade och daterade körningar markeras **Godkända**.
M-01 till M-10 samt F-01 och F-02 är godkända. M-06 godkändes efter
UID/GID-åtgärd och omtest. O-01 är åtgärdad på lagringsnivå och omtestad med
två identiska QoS 1-publiceringar; 32 historiska överskottsrader bevaras och
deras exakta transportorsak är inte fastställd. O-02 är åtgärdad genom en
avgränsad telemetriprenumeration och topicfiltrering och har omtestats i
runtime.
Ett underkänt test behåller statusen **Underkänd** tills orsaken har
åtgärdats och samma test har körts om.
