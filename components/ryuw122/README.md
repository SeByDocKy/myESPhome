# ryuw122 — composant ESPHome pour le module UWB REYAX RYUW122

Composant natif ESPHome pour piloter le module UWB (Ultra-Wideband) REYAX
RYUW122 en UART via jeu de commandes AT, en mode **ANCHOR** ou **TAG**.

Basé sur le même socle architectural que le composant `rylr998` :
- Parsing UART non bloquant, ligne par ligne (`\r\n`).
- File d'attente de commandes AT avec rate-limiting (une commande à la fois,
  attente du `+OK`/`+ERR` avant d'envoyer la suivante) pour ne pas saturer
  le petit buffer RX du module.
- Timeout de commande (1.5s) pour ne jamais rester bloqué si le module ne
  répond pas (mauvais baud rate, module en sommeil, etc.).
- Machine à état de configuration au boot qui pousse séquentiellement
  MODE → NETWORKID → ADDRESS → CPIN → CHANNEL → BANDWIDTH → CRFOP → CAL →
  RSSI → TAGD.

## Installation

```yaml
external_components:
  - source:
      type: local
      path: components   # dossier contenant ryuw122/
    components: [ryuw122]
```

Ou directement depuis GitHub une fois poussé sur ton repo `myESPhome` :

```yaml
external_components:
  - source: github://SeByDocKy/myESPhome
    components: [ryuw122]
```

## Exemple ANCHOR (reçoit les distances)

```yaml
uart:
  id: uart_ryuw122
  tx_pin: GPIO21
  rx_pin: GPIO20
  baud_rate: 115200

ryuw122:
  id: ryuw122_1
  uart_id: uart_ryuw122
  mode: ANCHOR
  network_id: "REYAX123"
  address: "REYAX003"
  password: "FABC0002EEDCAA90FABC0002EEDCAA90"   # AT+CPIN, 32 caractères hexa
  channel: 5             # 5 = 6489.6MHz, 9 = 7987.2MHz
  bandwidth: 850KBPS     # 850KBPS ou 6.8MBPS
  tx_power: 5            # 0=-65dBm ... 5=-32dBm
  calibration: 0         # AT+CAL, correction en cm (-100 à +100)
  rssi_enable: true

  on_anchor_rcv:
    - logger.log:
        format: "TAG %s -> %s, distance=%.1fcm rssi=%d"
        args: ["tag_address.c_str()", "payload.c_str()", "distance", "rssi"]

sensor:
  - platform: ryuw122
    ryuw122_id: ryuw122_1
    distance:
      name: "Distance UWB"
    rssi:
      name: "RSSI UWB"

text_sensor:
  - platform: ryuw122
    ryuw122_id: ryuw122_1
    last_data:
      name: "Dernière donnée UWB"

# Interroge périodiquement le TAG pour obtenir la distance
interval:
  - interval: 1s
    then:
      - ryuw122.anchor_send:
          id: ryuw122_1
          tag_address: "DAVID123"
          payload: "PING"
```

## Exemple TAG

```yaml
ryuw122:
  id: ryuw122_1
  uart_id: uart_ryuw122
  mode: TAG
  network_id: "REYAX123"
  address: "DAVID123"
  password: "FABC0002EEDCAA90FABC0002EEDCAA90"
  # Duty cycle RF pour économiser l'énergie (0,0 = RF toujours actif)
  tag_rf_enable_ms: 1000
  tag_rf_disable_ms: 1000

  on_tag_rcv:
    - logger.log:
        format: "Reçu de l'ANCHOR: %s (rssi=%d)"
        args: ["payload.c_str()", "rssi"]

  on_...   # rien de spécifique requis, le TAG répond automatiquement
           # aux requêtes AT+ANCHOR_SEND et calcule la distance côté ANCHOR
```

## Options de configuration

| Clé                  | Type    | Défaut      | Description                                             |
|----------------------|---------|-------------|----------------------------------------------------------|
| `mode`               | enum    | `TAG`       | `TAG`, `ANCHOR` ou `SLEEP` (AT+MODE)                     |
| `network_id`         | string  | `REYAX123`  | 1–8 caractères ASCII (AT+NETWORKID)                      |
| `address`            | string  | `DAVID123`  | 1–8 caractères ASCII, adresse unique du module (AT+ADDRESS) |
| `password`           | string  | (aucun)     | 32 caractères hexadécimaux, clé AES128 (AT+CPIN)         |
| `channel`            | int     | `5`         | `5` (6489.6MHz) ou `9` (7987.2MHz) (AT+CHANNEL)          |
| `bandwidth`          | enum    | `850KBPS`   | `850KBPS` ou `6.8MBPS` (AT+BANDWIDTH)                    |
| `tx_power`           | int     | `5`         | 0 à 5, voir tableau AT+CRFOP                             |
| `calibration`        | int     | `0`         | -100 à 100 cm, correction de distance (AT+CAL)           |
| `rssi_enable`        | bool    | `true`      | Active le RSSI dans les trames RCV (AT+RSSI)             |
| `tag_rf_enable_ms`   | int     | `0`         | Durée RF active du TAG, ms (AT+TAGD, 1er paramètre)      |
| `tag_rf_disable_ms`  | int     | `0`         | Durée RF inactive du TAG, ms (AT+TAGD, 2e paramètre)     |

## Triggers

- `on_anchor_rcv` (côté ANCHOR uniquement) : variables `tag_address` (string),
  `payload` (string), `distance` (float, cm), `rssi` (int, dBm).
- `on_tag_rcv` (côté TAG uniquement) : variables `payload` (string),
  `rssi` (int, dBm).

## Actions

- `ryuw122.tag_send`: envoie des données depuis un TAG (`AT+TAG_SEND`).
  ```yaml
  - ryuw122.tag_send:
      id: ryuw122_1
      payload: "HELLO"
  ```
- `ryuw122.anchor_send`: envoie des données à un TAG donné et déclenche le
  calcul de distance (`AT+ANCHOR_SEND`).
  ```yaml
  - ryuw122.anchor_send:
      id: ryuw122_1
      tag_address: "DAVID123"
      payload: "PING"
  ```

`payload` fait au maximum 12 octets ASCII (limite matérielle du module).
D'après la doc REYAX, la différence de longueur de payload entre ANCHOR et
TAG ne doit pas dépasser 3 octets, sous peine de fausser le calcul de
distance — complète avec des octets nuls si besoin.

## Capteurs disponibles

- `sensor` `distance` (cm) : mis à jour à chaque `+ANCHOR_RCV` (ANCHOR
  uniquement).
- `sensor` `rssi` (dBm) : mis à jour à chaque trame reçue (ANCHOR ou TAG),
  seulement si `rssi_enable: true`.
- `text_sensor` `last_data` : dernière charge utile ASCII reçue.

## Notes d'implémentation

- Le parsing de `+ANCHOR_RCV`/`+TAG_RCV` utilise la longueur de payload
  annoncée dans la trame pour découper le champ `<DATA>` de façon fiable,
  plutôt qu'un simple `split(',')`, car les données ASCII peuvent en théorie
  contenir des virgules.
- `AT+CPIN` est marqué comme champ sensible (`cv.sensitive`) : il est
  automatiquement masqué dans les logs de configuration ESPHome.
- Testé avec succès en validation de configuration ESPHome et génération de
  code C++ (`esphome config` / `esphome compile` jusqu'à l'étape de
  compilation firmware, qui nécessite le toolchain PlatformIO complet côté
  utilisateur final).

## Limitations connues / TODO

- Pas encore de gestion du mode `SLEEP` au-delà du simple `AT+MODE=2`
  (pas de réveil programmatique documenté dans la doc AT fournie).
- `AT+IPR` (changement de baud rate à chaud) n'est pas exposé en YAML pour
  éviter de désynchroniser le composant `uart:` d'ESPHome — modifie plutôt
  `baud_rate:` du bloc `uart:` directement si tu changes celui du module.
- `AT+UID?` (UID 96 bits) n'est pas encore exposé en YAML ; peut être ajouté
  facilement en `text_sensor` si besoin.
