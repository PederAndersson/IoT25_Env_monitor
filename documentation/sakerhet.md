# Säkerhetsanalys

## Tillgångar och angreppspunkter

Lösningen hanterar WiFi- och MQTT-inloggning, brokertrafik, sensorvärden och
en lokal SQLite-databas. ESP32, brokern, Pythonmottagaren och HTTP-klienten
har olika roller och behöver inte samma åtkomst. Säkerhetsåtgärderna nedan
är beskrivna utifrån koden och konfigurationen i repot; effektiviteten mot
den verkliga brokern ska kontrolleras enligt [testprotokollet](../testprotokoll.md).

| Risk | Genomförd åtgärd | Kvarvarande risk eller verifieringsbehov |
| --- | --- | --- |
| Någon avlyssnar eller förfalskar MQTT-trafik | ESP32 ansluter med `mqtts://` och ESP-IDF:s CA-bundle. Pythonmottagaren använder Pahos `tls_set()` med `certs/ca.crt` eller `MQTT_CA_PATH`. Ingen kod aktiverar osäker certifikatverifiering. Negativa test med fel hostname och fel CA avvisades 2026-10-07. | Vald TLS-minimiversion är inte explicit satt i koden. |
| MQTT-inloggning eller WiFi-lösenord läcker via Git | Exempelkonfigurationen innehåller platshållare. `.env`, `sdkconfig`, nyckelfiler och databasfiler ignoreras av huvudrepots Git. Lösenord loggas inte av applikationskoden. | `sdkconfig` och byggd firmware innehåller lokala inloggningsuppgifter; skydda datorn, byggartefakter och fysisk enhet. Git-ignore hindrar inte att hemligheter kopieras manuellt till andra spårade filer. |
| Felaktig eller fientlig MQTT-payload skadar lagring eller ger SQL-injektion | Mottagaren prenumererar endast på `esp-test/+/telemetry`, kontrollerar topicstrukturen och kräver ett JSON-objekt med sex validerade fält. `INSERT` och historikens `LIMIT` använder parametriserade SQL-frågor. | JSON-storlek begränsas inte i applikationen. Icke ändliga flyttal och mycket långa strängar avvisas inte uttryckligen. |
| Obehörig läsning via API | Lokal Pythonstart enligt README binder API:t till `127.0.0.1`. | API:t saknar autentisering och HTTPS. Compose publicerar port `8000:8000`, vilket kan göra API:t nåbart från andra datorer beroende på värdens nät och brandvägg. Begränsa exponeringen innan annan användning. |
| Obehörig MQTT-publicering eller prenumeration | Firmware använder användarnamn/lösenord och publicerar endast på enhetsspecifika topics under `esp-test/<device-id>/`. Mottagaren prenumererar endast på `esp-test/+/telemetry`. | Broker-ACL och kontobehörigheter finns inte i detta repo och har inte verifierats här. |
| Containerprocess skapar SQLite-filer som den lokala användaren inte kan hantera | Compose kör båda tjänsterna med konfigurerbara `HOST_UID` och `HOST_GID` mot den bind-mountade datakatalogen. | Fel lokala ID:n i `.env` kan fortfarande orsaka behörighetsfel. |

## Kommunikationsskydd

Firmwaren startar WiFi och väntar på NTP-synkronisering innan MQTT-klienten
startas. Det behövs för att certifikatets giltighetstider ska kunna
kontrolleras. MQTT använder TCP med TLS, inte WebSockets. ESP32 anger ett
betrott CA-bundle och Python anger ett CA-certifikat. Anslutningen ska göras
med certifikatets värdnamn så att certifikatkedja och hostname kan prövas.
Det finns ingen `tls_insecure_set(True)` eller motsvarande inställning i
projektkoden. [M-05](../testprotokoll.md) beskriver de positiva och negativa
TLS-test som genomfördes för båda klienterna 2026-10-07.

WiFi-klienten kräver minst WPA2-PSK enligt firmwarekonfigurationen. NTP
hämtas från `pool.ntp.org`, men koden autentiserar inte NTP-svaren; tid är
därför ett beroende som kan påverka både mätstämplar och TLS-start. Om NTP
misslyckas inom 15 sekunder avbryts MQTT-starten i det aktuella startflödet.

## Hemlig konfiguration

| Värde | Var det anges lokalt | Versionshantering |
| --- | --- | --- |
| WiFi-SSID och lösenord | `idf.py menuconfig` → `firmware/esp32/sdkconfig` | `sdkconfig` ignoreras av Git |
| ESP32:s MQTT-URI, användarnamn och lösenord | `idf.py menuconfig` → samma `sdkconfig` | Endast tomma standardvärden i spårade Kconfig-filer |
| Pythonmottagarens broker, port och MQTT-inloggning | Lokal `.env` enligt `.env.example` | `.env` ignoreras av Git |
| Betrodd CA för Python | `certs/ca.crt` eller `MQTT_CA_PATH` | Publikt CA-certifikat får versionshanteras; privat nyckel får inte göra det |

Compose läser `.env` för MQTT-mottagaren. Pythonstart utan Compose kräver
att samma variabler exporteras i terminalen. Publicera inte `.env`,
`sdkconfig`, privata nycklar, kompletta miljöutskrifter eller loggar med
hemligheter. Root-CA-filen är inte ett lösenord, men den måste verkligen
vara en betrodd CA för den broker man använder; ett leaf-certifikat ska inte
användas som trust anchor.

## Loggning, övervakning och begränsningar

Firmware loggar WiFi-/MQTT-anslutning, fel, publiceringsbekräftelser och
köstatus. Pythonmottagaren loggar anslutning, mottaget topic,
valideringsfel och hela godkända mätobjekt. Loggarna innehåller därmed
sensor-ID, mätvärden och tidsstämplar, även om de inte ska innehålla
inloggningsuppgifter. Begränsa åtkomst och lagringstid om sådana mätvärden
bedöms känsliga.

`GET /api/v1/status` visar bara antalet lagrade rader. Det är ett enkelt
övervakningsmått, men upptäcker inte på egen hand en tystnad från sensorn.
API-koden har ingen egen loggning av varje HTTP-anrop och inga särskilda
åtgärder mot hög anropsfrekvens. Uvicorns accesslogg beror på hur servern
startas. För högre säkerhetsnivå bör man lägga till API-åtkomstkontroll,
HTTPS eller en begränsad proxy, tydlig felräkning/färskhetskontroll och ett
testat sätt att rotera inloggning.
