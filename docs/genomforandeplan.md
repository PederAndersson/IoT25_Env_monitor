# Genomförandeplan: säker IoT-klimatmätning

## Syfte

Projektet ska visa ett komplett men litet IoT-dataflöde: en fysisk sensor på
en ESP32 skickar strukturerade mätvärden via MQTT till en Raspberry Pi. En
tjänst på Pi:n validerar och lagrar datan och gör den tillgänglig via ett
REST-API. Lösningen ska vara säker, övervakningsbar och dokumenterad så att
den går att installera, använda och felsöka.

Planen utgår från kursens mål inom MQTT och TCP/IP, portar, API:er, JSON,
tjänsteorienterad arkitektur, säker kommunikation, loggning och felsökning.

## Valda komponenter och motivering

| Del | Val | Varför |
| --- | --- | --- |
| Fysisk datakälla | DHT22 på ESP32 | Ger verkliga temperatur- och luftfuktighetsvärden som passar fastighetsscenariot. Sensorn är redan bekant, vilket minskar hårdvarurisken. |
| ESP32-firmware | C med ESP-IDF | Espressifs officiella ramverk ger direkt stöd för Wi-Fi, MQTT, TLS och FreeRTOS, och ligger nära professionell ESP32-utveckling. |
| Meddelandeprotokoll | MQTT över TCP/IP och TLS | Publish/subscribe passar återkommande sensorvärden. TLS gör kryptering till en praktiskt demonstrerbar säkerhetsåtgärd. |
| Broker | Befintlig Eclipse Mosquitto på `bengt` | Brokern är redan dokumenterad, körs i Docker och kräver inloggning. |
| Bearbetningstjänst | Python med FastAPI | Tydlig JSON-validering och ett litet REST-API. FastAPI gör det lätt att koppla datakontrakt, felhantering och API-dokumentation till kursmålen. |
| Drift | Egen Docker-container på Raspberry Pi | Gör tjänsten reproducerbar och håller API-tjänsten separat från MQTT-brokern. |
| Lagring | SQLite | Liten lokal databas som behåller historik över omstarter utan att införa en extra server. |

Alternativ som väljs bort: en knapp eller potentiometer är enklare men visar
inte ett lika relevant klimatmätningsscenario. Arduino-ramverket väljs bort
eftersom ESP-IDF ger mer direkt erfarenhet av Espressifs officiella verktyg
och API:er. Flask skulle fungera men kräver mer manuell validering och
API-dokumentation. Node.js/Express är ett rimligt val vid JavaScript-fokus,
men Python/FastAPI ger en mindre inlärningströskel för detta avgränsade
projekt. En tidsseriedatabas ger fler funktioner men är onödigt omfattande
för kraven.

## Målarkitektur

```text
DHT22
  │ fysisk avläsning var 60:e sekund
  ▼
ESP32 med C och ESP-IDF
  │ MQTT över TLS, port 8883, JSON
  ▼
Mosquitto-broker på Raspberry Pi (bengt)
  │ MQTT över TLS
  ▼
Python/FastAPI-tjänst i Docker på Raspberry Pi
  ├── SQLite: mätvärdeshistorik
  ├── loggar: anslutning, data och fel
  └── HTTP/REST: senaste värde, historik och status
```

DHT22 läses var 60:e sekund. Intervallet är långt över sensorns minsta
läsintervall och är rimligt för klimatdata. Två MQTT-meddelanden publiceras,
ett för temperatur och ett för luftfuktighet. Det håller fast vid ett enkelt,
konsekvent datakontrakt:

```json
{
  "sensorId": "room-a-climate-01-temp",
  "timestamp": "2026-09-01T12:00:00Z",
  "value": 21.7,
  "unit": "C"
}
```

Luftfuktighet använder samma fält med `sensorId` som slutar på `-humidity`
och `unit` `%`. Föreslagna topics är
`building/room-a/climate/temperature` och
`building/room-a/climate/humidity`.

## Arbetssteg

Varje steg ska vara klart och testat innan nästa påbörjas.

Den första delen av projektet använder simulerad MQTT-data. Det gör att
serverkedjan kan byggas och felsökas utan att Wi-Fi, firmware och sensor
introduceras samtidigt. ESP32 och DHT22 integreras efter att kedjan fungerar.

### Förberedelser genomförda

Följande är verifierat på Raspberry Pi:n `bengt`:

- Docker Engine och Docker Compose är installerade och Mosquitto-containern
  körs på port 1883.
- OpenSSL, curl och Git finns installerade.
- `mosquitto-clients` är installerat; både `mosquitto_sub` och
  `mosquitto_pub` fungerar och har TLS-stöd.

Systemuppdateringar, inklusive Docker- och kärnuppdatering, genomförs senare
när fysisk åtkomst till Pi:n finns. Efter omstart ska containerstatus
kontrolleras med `docker ps`.

1. **Verifiera MQTT med simulerad JSON-data.**
   - Dokumentera brokeradress, port, inloggningsprincip, topics och
     datakontrakt. Använd `mosquitto_sub` och `mosquitto_pub` för att ta emot
     respektive skicka ett giltigt testmeddelande.
   - Test: subscriber och brokerlogg visar samma JSON-payload.

2. **Skapa mottagartjänsten i Python.**
   - Prenumerera på topics, validera JSON-fälten och logga anslutning,
     återanslutning, mottagen data och valideringsfel.
   - Test: giltig simulerad data behandlas; ogiltig JSON avvisas och loggas
     utan att tjänsten avslutas.

3. **Lagra data och bygg REST-API.**
   - Spara giltiga mätvärden i SQLite. Implementera `GET /health`,
     `GET /api/v1/readings/latest`, `GET /api/v1/readings?limit=...` och
     `GET /api/v1/status`.
   - Test: API:t visar rätt senaste värde och historik efter manuell
     MQTT-publicering.

4. **Driftsätt den simulerade kedjan på Raspberry Pi.**
   - Paketera Python/FastAPI-tjänsten i Docker med lokal miljöfil och
     datavolym. Kör den separat från broker-containern.
   - Test: tjänsten startar efter omstart och SQLite-data finns kvar.

5. **Aktivera och verifiera MQTT-TLS.**
   - Konfigurera Mosquitto med en TLS-lyssnare på port 8883 och ett lokalt
     certifikat. ESP32 och mottagartjänsten ska verifiera certifikatet.
   - Test: en MQTT-klient ansluter med TLS; anslutning med fel certifikat eller
     fel inloggning avvisas. Mottagartjänsten fortsätter fungera över TLS.

6. **Integrera ESP32 med C och ESP-IDF.**
   - Skapa ett ESP-IDF-projekt där ESP32 ansluter till Wi-Fi och publicerar ett
     statiskt JSON-testvärde. Lägg till återanslutning för Wi-Fi och MQTT/TLS.
   - Test: ESP32:ns testvärde visas i logg, SQLite och REST-API.

7. **Integrera DHT22 som fysisk datakälla.**
   - Läs temperatur och luftfuktighet med ESP-IDF och ersätt det statiska
     testvärdet med två JSON-meddelanden var 60:e sekund.
   - Test: rimliga fysiska värden når API:t och ändras när sensorns miljö
     förändras.

8. **Genomför övervakning och felsökning.**
   - Statusendpoint ska visa mottagna meddelanden, valideringsfel och tid för
     senaste data. Återskapa felaktiga MQTT-inloggningsuppgifter och ogiltig
     JSON.
   - Test: dokumentera symptom, loggar, orsak, åtgärd och verifiering för
     båda felen.

9. **Slutför dokumentation och redovisning.**
   - Skriv README, `arkitektur.md`, `api.md`, `sakerhet.md`,
     `felsokning.md` och ett testprotokoll. Förbered en kort demonstration av
     dataflödet, API:t, TLS och ett felsökningsfall.

## Säkerhet och begränsningar

Basåtgärderna är MQTT-användare/lösenord, hemligheter i lokala miljöfiler,
strikt JSON-validering och ingen portvidarebefordran. TLS är obligatoriskt
för MQTT-trafiken eftersom kursen och VG-målet kräver praktiskt tillämpad
säker nätverkskommunikation. Tailscale kan användas för krypterad fjärråtkomst
till Raspberry Pi, men ersätter inte TLS mellan ESP32 och broker på LAN.

REST-API:t börjar som ett lokalt API för demonstration. API-autentisering och
HTTPS kan beskrivas som framtida förbättringar om API:t ska exponeras utanför
det betrodda nätet.

## Klart när

- Ett verkligt DHT22-värde når REST-API:t via ESP32, MQTT och mottagartjänsten.
- MQTT-trafiken är autentiserad och TLS-krypterad.
- API, loggar, SQLite-historik och övervakningsstatus fungerar.
- Två medvetna fel är analyserade och dokumenterade.
- En annan utvecklare kan följa README för att installera, köra, testa och
  felsöka lösningen.
