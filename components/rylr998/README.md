# Composant ESPHome RYLR998

Composant ESPHome natif pour les modules LoRa RYLR998 et RYLR498.

## Installation

1. Créez un dossier `custom_components/rylr998/` dans votre configuration ESPHome
2. Copiez les fichiers suivants dans ce dossier :
   - `__init__.py`
   - `rylr998.h`
   - `rylr998.cpp`

## Configuration YAML

### Configuration de base

```yaml
uart:
  id: uart_bus
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora
  uart_id: uart_bus
  address: 100
  frequency: 915MHz
  spreading_factor: 9
  signal_bandwidth: 125kHz
  coding_rate: 1
  preamble_length: 12
  network_id: 18
  tx_power: 22
```

### Paramètres disponibles

| Paramètre | Type | Défaut | Description |
|-----------|------|--------|-------------|
| `address` | int (0-65535) | 0 | Adresse du module |
| `frequency` | frequency | 915MHz | Fréquence RF (490MHz pour RYLR498) |
| `spreading_factor` | int (5-11) | 9 | Facteur d'étalement |
| `signal_bandwidth` | frequency | 125kHz | Bande passante (125/250/500 kHz) |
| `coding_rate` | int (1-4) | 1 | Taux de codage (1=4/5, 2=4/6, 3=4/7, 4=4/8) |
| `preamble_length` | int (4-24) | 12 | Longueur du préambule |
| `network_id` | int (3-15, 18) | 18 | ID du réseau |
| `tx_power` | int (0-22) | 22 | Puissance d'émission en dBm |

### Recevoir des paquets

```yaml
rylr998:
  id: lora
  uart_id: uart_bus
  address: 100
  on_packet:
    then:
      - logger.log:
          format: "Paquet reçu de %d: %s (RSSI: %d, SNR: %d)"
          args: ['address', 'data', 'rssi', 'snr']
      - lambda: |-
          std::string msg(data.begin(), data.end());
          ESP_LOGI("lora", "Données: %s", msg.c_str());
```

### Envoyer des paquets

#### Via bouton

```yaml
button:
  - platform: template
    name: "Envoyer test LoRa"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: [0x48, 0x45, 0x4C, 0x4C, 0x4F]  # "HELLO"
```

#### Via lambda

```yaml
button:
  - platform: template
    name: "Envoyer température"
    on_press:
      - lambda: |-
          std::string msg = "TEMP:25.5";
          std::vector<uint8_t> data(msg.begin(), msg.end());
          id(lora).send_data(200, data);
```

#### Broadcast (envoi à tous)

```yaml
button:
  - platform: template
    name: "Broadcast"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 0  # 0 = broadcast
          data: !lambda |-
            std::string msg = "ALERT";
            return std::vector<uint8_t>(msg.begin(), msg.end());
```

## Exemple complet

```yaml
esphome:
  name: lora-node
  platform: ESP32
  board: esp32dev

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

logger:
  level: DEBUG

api:
  encryption:
    key: !secret api_encryption_key

ota:
  password: !secret ota_password

uart:
  id: uart_bus
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora
  uart_id: uart_bus
  address: 100
  frequency: 868.5MHz  # Fréquence EU
  spreading_factor: 9
  signal_bandwidth: 125kHz
  coding_rate: 1
  preamble_length: 12
  network_id: 10
  tx_power: 14  # Max pour conformité CE
  on_packet:
    then:
      - logger.log:
          format: "Paquet de %d: RSSI=%d SNR=%d"
          args: ['address', 'rssi', 'snr']
      - lambda: |-
          std::string msg(data.begin(), data.end());
          id(received_text).publish_state(msg);

sensor:
  - platform: dht
    pin: GPIO4
    temperature:
      name: "Température"
      id: temperature
    humidity:
      name: "Humidité"
      id: humidity
    update_interval: 60s
    on_value:
      then:
        - lambda: |-
            char buffer[50];
            snprintf(buffer, sizeof(buffer), "T:%.1f,H:%.1f", 
                     id(temperature).state, id(humidity).state);
            std::string msg(buffer);
            std::vector<uint8_t> data(msg.begin(), msg.end());
            id(lora).send_data(200, data);

text_sensor:
  - platform: template
    name: "Dernier paquet LoRa"
    id: received_text

button:
  - platform: template
    name: "Test LoRa"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: [0x54, 0x45, 0x53, 0x54]  # "TEST"
```

## Connexions matérielles

### RYLR998 vers ESP32

| RYLR998 | ESP32 |
|---------|-------|
| VCC | 3.3V |
| GND | GND |
| TXD | GPIO16 (RX) |
| RXD | GPIO17 (TX) |

## Notes importantes

1. **Délais entre commandes** : Le composant attend automatiquement entre les commandes AT
2. **Limite de données** : Maximum 240 octets par message
3. **Conformité CE** : Pour l'Europe, utilisez `tx_power: 14` maximum
4. **Network ID** : Tous les modules doivent avoir le même `network_id` pour communiquer
5. **Fréquences** :
   - RYLR998 : 915 MHz (US/AS)
   - RYLR498 : 490 MHz
   - Europe : 868 MHz (vérifiez la réglementation locale)

## Recommandations

Pour une meilleure performance :

### Portée maximale
```yaml
spreading_factor: 11
signal_bandwidth: 125kHz
coding_rate: 1
preamble_length: 12
```

### Débit maximal
```yaml
spreading_factor: 7
signal_bandwidth: 500kHz
coding_rate: 1
preamble_length: 12
```

### Équilibré (recommandé)
```yaml
spreading_factor: 9
signal_bandwidth: 125kHz
coding_rate: 1
preamble_length: 12
```

## Dépannage

### Le module ne répond pas
- Vérifiez les connexions UART (TX/RX inversés)
- Vérifiez le baud rate (115200 par défaut)
- Vérifiez l'alimentation 3.3V

### Paquets non reçus
- Vérifiez que le `network_id` est identique sur tous les modules
- Vérifiez la `frequency`
- Vérifiez tous les paramètres RF (`spreading_factor`, `signal_bandwidth`, etc.)

### Portée limitée
- Augmentez le `spreading_factor`
- Réduisez le `signal_bandwidth`
- Augmentez le `tx_power`
- Vérifiez l'antenne

## Licence

Ce composant est basé sur la documentation AT du RYLR998 fournie par REYAX Technology.
