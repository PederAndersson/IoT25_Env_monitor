# Felsökning

Dokumentet beskriver hur vanliga fel lokaliseras och vilka observationer som
redan finns. Det ersätter inte [testprotokollets](../testprotokoll.md)
resultatfält för de två planerade, avsiktliga felproven F-01 och F-02.

## Börja med att lokalisera felet

Följ dataflödet ett steg i taget:

1. **ESP32 och DHT11:** Visar seriell logg en lyckad fysisk sensorläsning?
2. **WiFi och NTP:** Fick enheten IP-adress och synkroniserades tiden innan
   MQTT startade?
3. **MQTT:** Visar enhetens logg anslutning och en QoS 1-publicering på
   `esp-test/<device-id>/telemetry`?
4. **Pythonmottagaren:** Loggas rätt topic och en godkänd validering?
5. **SQLite och API:** Ökar `storedReadings`, och returnerar
   `/api/v1/readings/latest` den förväntade tidsstämpeln?

Ett fel i ett tidigare steg kan inte rättas i API:t. [Arkitekturen](arkitektur.md)
visar vilka delar som ansvarar för varje övergång.

### Lokala kontroller

| Kontroll | Vad resultatet betyder |
| --- | --- |
| `docker compose ps` | Visar om lokala containrar kör och om API-healthcheck är healthy. Kommandot ändrar inget. |
| `docker compose logs --tail=50 mqtt_receiver` | Visar de senaste 50 loggraderna från mottagaren. `--tail=50` begränsar mängden utdata; kommandot är skrivskyddat. |
| `curl -i http://127.0.0.1:8000/health` | `-i` visar även HTTP-status och headers. `200` bekräftar att API-processen svarar, inte att databasen eller brokern fungerar. |
| `curl -i http://127.0.0.1:8000/api/v1/status` | Visar om API:t kan läsa antalet databasrader. Kör kommandot före och efter en mätning. |
| `python -m pytest` | Kör isolerade tester för validering, databas, API-funktioner och MQTT-konfiguration. Ingen verklig broker eller sensor används. |

Kör kommandona från projektroten, med den lokala miljön enligt
[README](../README.md). Vid firmwarefel används `idf.py monitor` i en terminal
där ESP-IDF-miljön är laddad. Undvik att kopiera hela miljövariabler eller
loggar som kan innehålla känsliga värden till en felrapport.

## Vanliga symptom

| Symptom | Sannolik orsak och kontroll |
| --- | --- |
| Mottagaren avslutas direkt | Saknad/ogiltig `MQTT_BROKER` eller `MQTT_PORT`; kontrollera lokal `.env` och felmeddelandet. |
| ESP32 startar inte MQTT | WiFi-initiering eller NTP-synkronisering misslyckades; läs första felraden i seriell logg. |
| TLS-anslutning misslyckas | Fel CA, fel hostname/SAN, fel port eller klocka som inte synkroniserats; kontrollera vilken klient som ger felet. Stäng inte av certifikatkontrollen för att få anslutning. |
| MQTT är anslutet men inga mätningar når SQLite | Kontrollera att publiceringen ligger exakt på `esp-test/<device-id>/telemetry`, att ACL tillåter topicet, att DHT11-läsningen lyckas och att payloaden godkänns. Statuspayload `online`/`offline` ignoreras avsiktligt av telemetrivalideringen. |
| `/health` svarar men databasendpoints felar | API-processen är igång men databasen kan sakna tabellen `readings` eller ha fel filrättigheter. Kontrollera delad datakatalog och att `HOST_UID`/`HOST_GID` i `.env` matchar `id -u`/`id -g`. |
| `/api/v1/readings/latest` ger 404 | Tabellen finns men saknar godkända mätningar. Kontrollera mottagarens valideringslogg och `/api/v1/status`. |
| Flera identiska poster syns | Nya kopior med samma `sensorId` och `timestamp` ska loggas som ignorerade och inte skapa en ny rad. Kontrollera att den senaste mottagarimagen kör, och jämför loggens `qos`, `dup` och `mid`. Historiska dubbletter finns kvar. |

### O-01: upprepade mätningar

Vid körningen 2026-10-05 köades en fysisk mätning med tidsstämpeln
`12:16:22Z` en gång i den fångade ESP32-loggen och fick en MQTT-kvittens.
Mottagaren loggade ändå tre ankomster och tre databasrader skapades. En
kontroll före åtgärden fann totalt 121 rader men bara 89 unika kombinationer
av sensor-ID och tidsstämpel, alltså 32 historiska överskottsrader.

ESP-MQTT kan sända om okvitterade QoS 1-publiceringar. Det är därför en
rimlig hypotes, inte en bevisad förklaring för just dessa tre ankomster.
[ESP-MQTT](https://docs.espressif.com/projects/esp-mqtt/en/latest/esp32/)
beskriver omsändning, men den exakta transportorsaken för de historiska
kopiorna är inte bevisad. MQTT:s `message_id` identifierar inte en mätning
genom hela kedjan.

Åtgärden 2026-10-07 var att prenumerera på `esp-test/+/telemetry` med QoS 1
och göra lagringen villkorad på att kombinationen `sensorId` och `timestamp`
inte redan finns. Ett runtime-test publicerade samma giltiga QoS 1-payload
två gånger. Båda leveranserna nådde callbacken, den andra loggades som
ignorerad och exakt en rad lagrades. Den syntetiska testraden raderades
efteråt. O-01 är därmed åtgärdad på lagringsnivå för den nuvarande enda
mottagarprocessen; de äldre raderna bevaras. Flera samtidiga skrivprocesser
bör kompletteras med en unik databasbegränsning.

## Bygg- och flashproblem för ESP32

Om bygget stoppar med att
`configUSE_LIST_DATA_INTEGRITY_CHECK_BYTES` är omdefinierad ska lokala
komponenter inte inkludera den interna headern `freertos/projdefs.h`
direkt. Projektets WiFi-komponent använder nu enbart de publika FreeRTOS-
headerkedjorna. Efter att den direkta inkluderingen togs bort byggde
firmwaren med ESP-IDF 6.0.

Om flashningen först visar att alla byte skrivits och verifierats men sedan
slutar med pySerial-felet `Could not configure port`, inträffade felet efter
själva skrivningen vid hard reset. Kontrollera USB-kabel, portnamn och om en
annan monitor håller porten; koppla vid behov om enheten och starta monitorn
separat. På den verifierade körningen startade den nya firmwaren och dess
seriella logg kunde därefter följas.

## Fall 1: avsiktligt ogiltig MQTT-port vid start

Detta är en historisk lokal kontroll beskriven i `docs/STARTPROMPT.md` den
2026-10-02. Den visar felhantering för konfiguration, men är inte en ny
end-to-end-körning av F-01 i testprotokollet.

1. **Observerat symptom:** När port saknades avslutades mottagarprocessen
   med status 1. Icke-numeriska värden och värden utanför 1–65535 avvisades.
2. **Hur felet identifierades:** Flera medvetet felaktiga `MQTT_PORT`-värden
   testades separat i en isolerad miljö och processens avslutningsstatus
   kontrollerades.
3. **Verktyg eller loggar:** Mottagarens felmeddelanden för saknad variabel,
   icke-heltal och ogiltigt portintervall; terminalens exit status.
4. **Felets orsak:** `MQTT_PORT` saknades eller innehöll ett ogiltigt värde.
5. **Genomförd åtgärd:** Mottagarkoden validerar numera porten vid start och
   avslutar tydligt med status 1 i stället för att fortsätta med fel
   konfiguration. Användaren behöver ange en riktig TLS-port i lokal `.env`.
6. **Verifiering av åtgärd:** Felvägarna och exit status 1 för saknad port
   verifierades manuellt 2026-10-02. Automatiska tester verifierar även
   ogiltiga värden och en giltig, mockad startväg. Återhämtning mot en
   verklig broker efter ändrad `.env` är inte dokumenterad i detta fall.

## Fall 2: avsiktligt brokeravbrott och återanslutning

Detta är en historisk hårdvaruobservation från 2026-10-02 enligt
`docs/STARTPROMPT.md`. Den prövar MQTT-återanslutning, men inte brokerpublicerad
Last Will medan brokern är stoppad.

1. **Observerat symptom:** ESP32 tappade MQTT-kontakten när brokern inte var
   tillgänglig. Loggen visade successiva väntetider omkring 10, 20 och 40
   sekunder.
2. **Hur felet identifierades:** Brokeravbrott infördes avsiktligt och
   ESP32:s seriella logg följdes under avbrott och efter att brokern återkom.
3. **Verktyg eller loggar:** Seriell firmwarelogg för frånkoppling,
   återanslutningsförsök, TLS-validering och ny MQTT-anslutning.
4. **Felets orsak:** Brokern var tillfälligt otillgänglig trots att ESP32:s
   WiFi-förbindelse fanns kvar.
5. **Genomförd åtgärd:** Brokern gjordes tillgänglig igen. Firmwarens
   återanslutning med exponential backoff hanterade avbrottet utan omstart
   av enheten.
6. **Verifiering av åtgärd:** Loggen visade att certifikatet validerades och
   klienten anslöt igen. Tidigare observationer visade även återställning
   till ungefär 10 sekunders väntan efter minst 60 sekunders stabil
   anslutning och bibehållen högre backoff efter en kort anslutning.

Detta fall bevisar inte att Last Will `offline` publicerades eller att samma
mätning nådde SQLite och API. De delarna kräver separata körningar.

## Felprov för slutleveransen

[F-01 i testprotokollet](../testprotokoll.md) genomfördes 2026-10-05:
avsiktligt trasig JSON avvisades, varningen loggades och antalet lagrade
poster var oförändrat i den kontrollerade omkörningen. Testet är **Godkänt**.

F-02 genomfördes först 2026-10-05 och underkändes eftersom WiFi-försöken tog
slut efter ett långt avbrott. En separat WiFi-task med fortsatt återförsök,
exponentiell väntetid och jitter infördes därefter. Omtestet 2026-10-07
visade retained Last Will `offline`, fortsatt sampling till RAM-kön,
WiFi-backoff upp till 30 sekunder och automatisk återhämtning av WiFi, TLS,
MQTT, SQLite och API när nätverket återkom. F-02 är därför **Godkänt**.
