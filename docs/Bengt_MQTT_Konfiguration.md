# MQTT-broker på Raspberry Pi med Docker Compose

Detta dokument beskriver min MQTT-broker för en skoluppgift. Brokern körs
på en Raspberry Pi 4 med Raspberry Pi OS Lite (Debian 13) och Docker.

## Syfte

MQTT används för kommunikation enligt publish/subscribe-modellen:

- en **broker** tar emot och distribuerar meddelanden
- en **publisher** skickar ett meddelande till en topic
- en **subscriber** tar emot meddelanden från en topic som den prenumererar
  på

I denna lösning är brokern Eclipse Mosquitto, som körs i en Docker-container.

## Nätverk och åtkomst

- Server: Raspberry Pi med hostname `bengt`
- LAN-adress vid installationen: `192.168.1.54`
- Tailscale-adress: `100.84.116.70`
- MQTT-port: `1883/tcp`
- Åtkomst: användarnamn och lösenord krävs

Port 1883 använder vanlig MQTT över TCP. Den är inte TLS-krypterad i denna
version och ska därför användas på ett betrott lokalt nätverk.

För säker åtkomst utanför hemnätet används Tailscale. En klient som är
ansluten till samma Tailnet kan ansluta till `100.84.116.70` på port `1883`;
trafiken i Tailnet är krypterad. Ingen portvidarebefordran i routern används.

## Projektstruktur

Projektet ligger på Pi:n i `~/mqtt-broker`:

```text
mqtt-broker/
├── compose.yaml
├── config/
│   ├── mosquitto.conf
│   └── passwordfile        # lösenordshash, lämnas inte in
├── data/                   # persistent brokerdata
└── log/                    # Mosquitto-loggar
```

## Docker Compose-konfiguration

Filen `compose.yaml` startar Mosquitto och publicerar port 1883:

```yaml
services:
  mosquitto:
    image: eclipse-mosquitto:2
    container_name: mosquitto
    restart: unless-stopped
    ports:
      - "1883:1883"
    volumes:
      - ./config:/mosquitto/config:ro
      - ./data:/mosquitto/data
      - ./log:/mosquitto/log
```

`config` är skrivskyddad i containern (`:ro`). Det minskar risken att en
process i containern ändrar brokerinställningarna eller lösenordsfilen.

## Mosquitto-konfiguration

Filen `config/mosquitto.conf` innehåller:

```conf
listener 1883
allow_anonymous false
password_file /mosquitto/config/passwordfile

persistence true
persistence_location /mosquitto/data/

log_dest file /mosquitto/log/mosquitto.log
```

Den öppnar MQTT på port 1883, kräver autentisering, lagrar data utanför
containerns tillfälliga skrivbara lager och skriver loggar till en persistent
katalog.

## Lösenordshantering

En MQTT-användare skapades med Mosquittos eget verktyg:

```bash
docker run --rm -it --user "$(id -u):$(id -g)" -v "$PWD/config:/mosquitto/config" eclipse-mosquitto:2 mosquitto_passwd -c /mosquitto/config/passwordfile mqttuser
```

Kommandot frågar efter lösenordet interaktivt och sparar en hash i
`passwordfile`; lösenordet skrivs inte i Compose-filen eller detta dokument.

## Start, kontroll och drift

Starta från projektkatalogen:

```bash
docker compose up -d
```

Kontrollera status:

```bash
docker compose ps
```

Läs Mosquittos logg:

```bash
sudo cat log/mosquitto.log
```

Stoppa respektive starta igen:

```bash
docker compose down
docker compose up -d
```

## Verifiering

Efter start visade brokerloggen att Mosquitto läste konfigurationen och
öppnade IPv4- och IPv6-lyssnare på port 1883. Från en annan dator på LAN:et
verifierades porten med:

```bash
nc -vz 192.168.1.54 1883
```

Resultatet bekräftade anslutning till MQTT-porten.

MQTT-porten verifierades också via Tailscale med:

```bash
nc -vz 100.84.116.70 1883
```

## Vidare utveckling

Nästa säkerhetsförbättring är TLS, så att MQTT-trafik och inloggningsuppgifter
krypteras. Det kräver certifikat och att klienterna ansluter med TLS.
