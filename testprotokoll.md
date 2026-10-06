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
- **Faktiskt resultat:** Nyflashad firmware rapporterade appversion `66fc445`.
  WiFi var redo 13:54:14 och NTP synkroniserades 13:54:18. MQTT-klienten
  startade 13:54:18, men första sensormätningen köades samma sekund,
  innan certifikatkedjan validerades 13:54:21 och MQTT anslöts 13:54:23.
  Ytterligare mätningar köades 13:55:18 och 13:56:18; alla tre publicerades.
  Intervallen var 60 sekunder enligt lokal konfiguration. I firmwarekoden
  nås köningen endast efter lyckad `dht_11_read` och tidsstämpling; själva
  mätvärdena skrivs inte ut i denna monitorlogg. Den första mätningen
  väntade i kön medan MQTT anslöt och publicerades därefter. Därmed är
  WiFi och NTP före TLS verifierade, liksom sensoravläsning och periodicitet,
  men strikt ordning med färdig MQTT/TLS-anslutning före telemetri uppnåddes
  inte. Mätvärden och hela kedjan verifieras separat i M-03.
- **Status:** Underkänd enligt testplanens strikta uppstartsordning; de
  övriga delarna ovan fungerade. Behöver kravtolkning eller ändring och
  omtest innan M-02 kan markeras Godkänd.
- **Datum och bevis:** 2026-10-05, ignorerad rålogg
  `data/esp32-monitor-2026-10-05-postflash.log`, rader 38, 149, 153,
  159–184. Råloggen innehåller lokala nätverksidentifierare och ska inte
  läggas till i Git.

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
  **Öppen avvikelse O-01:** Mätningen 12:16:22Z togs emot och lagrades tre
  gånger (databas-ID 22–24), trots en köning och en publiceringsbekräftelse
  i den fångade firmwareloggen. Sju tidsstämplar hade 2–4 poster vardera
  vid senare kontroll. Mottagaren sparar varje mottaget meddelande separat;
  orsaken till upprepade leveranser är inte fastställd.
- **Status:** Godkänd för sammanhängande sexfältsflöde och periodisk lagring.
  O-01 är en olöst begränsning; antal databasrader är inte säkert antal
  unika sensormätningar.
- **Datum och bevis:** 2026-10-05, ignorerad rålogg
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
  verifierades i mottagarloggen, men dess QoS och keepalive mättes inte
  separat i detta test.
- **Status:** Delvis verifierad; ännu inte Godkänd för hela M-04.
- **Datum och bevis:** 2026-10-05, anonymiserade prenumerantrader och
  firmware-/mottagarloggar. Enhets-ID maskerat i detta protokoll.

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
  anslutning. Pythonklienterna i de redan körande containrarna anslöt med
  TLS. En ny Pythonanslutning från värdmiljön avvisades däremot med
  `SSLCertVerificationError: Hostname mismatch`; ingen osäker fallback
  användes. Om värdmiljön, containerns lagrade miljö och broker-certifikat
  överensstämmer är ännu inte utrett. Separata negativa CA- och hostname-
  injektioner har inte körts.
- **Status:** Ej färdigverifierad; värdnamnsavvikelsen måste utredas före
  Godkänd status.
- **Datum och bevis:** 2026-10-05, anonymiserad TLS-feltext och
  `Certificate validated` i firmwareloggen. Inga värdnamn eller hemligheter
  återges här.

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
- **Faktiskt resultat:** `docker compose config -q` avslutades utan fel.
  Befintliga containrar startades utan ombygge; `docker compose ps` visade
  API som healthy och mottagaren som up. API:t returnerade lagrade och nya
  mätningar. Återskapande av containrar och efterföljande persistenskontroll
  genomfördes inte; den körande imagen kan vara äldre än aktuell källkod.
- **Status:** Delvis verifierad; persistens efter återskapande återstår.
- **Datum och bevis:** 2026-10-05, lokal Compose-status och API-svar.

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
  mottagen och validerad data samt avvisad ogiltig JSON. Återanslutning
  efter kort avbrott observerades, men inte efter långt avbrott (F-02).
  En fullständig logggranskning efter hemligheter är inte genomförd; råa
  firmwareloggar innehåller lokala nätverksidentifierare.
- **Status:** Delvis verifierad; ännu inte Godkänd för hela M-08.
- **Datum och bevis:** 2026-10-05, anonymiserade loggutdrag i M-02, M-03,
  F-01 och F-02.

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
  firmware kön växa till 8/60 samt kommunikationsfel. Måttet räknar
  databasrader, inte unika mätningar (se O-01); tappade mätningar
  provocerades inte eftersom kön inte fylldes.
- **Status:** Godkänd för statusmått och köövervakning; dubbletter begränsar
  tolkningen av `storedReadings`.
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

1. **Observerat symptom:** Efter att endast ESP32:s WiFi brutits kom
   `offline` från brokern. ESP32 loggade WiFi- och MQTT-fel och köade nya
   mätningar, men anslöt inte automatiskt när hotspotten återkom.
2. **Hur felet identifierades:** En separat prenumerant förblev ansluten och
   såg `offline` 12:52:54 UTC. En ny prenumerant fick samma `offline` med
   `retained=True` och QoS 1. Firmwareloggen visade fem WiFi-försök och
   därefter inga nya; API:ts senaste tidsstämpel låg kvar på 12:51:52Z.
3. **Verktyg eller loggar:** ESP-IDF-monitor, två TLS-verifierade
   MQTT-prenumeranter och lokalt REST-API.
4. **Felets orsak:** Avsiktligt avbruten WiFi-kontakt. I `wifi.c` stoppas
   försöken när `CONFIG_APP_WIFI_MAXIMUM_RETRY` nås (5/5 kl. 12:53:03 UTC).
   Inget senare försök schemaläggs, medan MQTT:s återanslutning väntar på
   WiFi. Detta är en kodbegränsning, inte ett bevisat brokerfel.
5. **Genomförd åtgärd:** Hotspotten återställdes, men det återupptog inte
   anslutningen efter det långa avbrottet. En ändring för fortsatta WiFi-
   återförsök med tidsavstånd behövs; ingen sådan kodändring hade ännu
   verifierats vid denna körning.
6. **Verifiering av åtgärd:** Fram till att monitorloggen avslutades kl.
   13:01:41 UTC syntes ingen ny MQTT-anslutning; kön nådde minst 8/60 och
   API:t stod kvar på 12:51:52Z. Ett separat, kortare avbrott återhämtade
   sig senare utan reset: `offline` 13:09:38 och ny API-mätning 13:11:07Z.
   Det visar att korta avbrott kan fungera, men löser inte det långa fallet.

- **Faktiskt resultat:** Last Will, retained `offline`, felrapportering och
  köning fungerade. Automatisk återanslutning efter längre avbrott och
  leverans av de köade mätningarna uteblev.
- **Status:** Underkänd. Kräver ändrad WiFi-återanslutning och ny körning
  med samma långa avbrott innan status kan bli Godkänd.
- **Datum och bevis:** 2026-10-05, ignorerad rålogg
  `data/esp32-monitor-2026-10-05-f02.log` samt anonymiserade
  prenumerantrader 12:50:46 `online retained=True`, 12:52:54 `offline`,
  12:56:57 `offline retained=True` och lokalt API-svar 12:51:52Z efter
  återställd hotspot. Råloggar med nätverksidentifierare ska inte läggas
  till i Git.

## Sammanfattning

Endast faktiskt observerade och daterade körningar markeras **Godkända**.
M-03, M-09 och F-01 är godkända med O-01 som öppen dubblettavvikelse; M-02
och F-02 är underkända enligt sina testkriterier. M-04, M-05, M-07 och
M-08 är bara delvis verifierade; M-06 och M-10 är inte körda. M-05 har
dessutom en ännu outredd värdnamnsavvikelse för en färsk Pythonanslutning.
Ett underkänt test behåller statusen **Underkänd** tills orsaken har
åtgärdats och samma test har körts om.
