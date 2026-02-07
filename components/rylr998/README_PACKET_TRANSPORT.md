# RYLR998 Packet Transport Platform

Plateforme de transport de paquets pour le composant RYLR998, permettant la communication directe entre nœuds ESPHome via LoRa.

## Vue d'ensemble

Le composant `packet_transport` permet aux nœuds ESPHome de communiquer directement entre eux sur un canal de communication, sans nécessiter de serveur ou broker central. L'implémentation RYLR998 utilise LoRa comme moyen de communication.

## Installation

1. Installez d'abord le composant RYLR998 de base (voir README.md principal)
2. Copiez également le fichier `packet_transport.py` dans le dossier `custom_components/rylr998/`

## Configuration

### Configuration de base

```yaml
# Configuration du module LoRa
rylr998:
  id: lora
  uart_id: uart_bus
  address: 100
  frequency: 868MHz
  spreading_factor: 9
  signal_bandwidth: 125kHz
  network_id: 10

# Configuration du packet transport
packet_transport:
  platform: rylr998
  id: lora_transport
  update_interval: 60s
  sensors:
    - temperature
  binary_sensors:
    - motion

# Capteur à transmettre
sensor:
  - platform: dht
    pin: GPIO4
    temperature:
      name: "Température"
      id: temperature
    humidity:
      name: "Humidité"

binary_sensor:
  - platform: gpio
    pin: GPIO5
    name: "Détecteur de mouvement"
    id: motion
```

### Avec chiffrement

```yaml
packet_transport:
  platform: rylr998
  id: lora_transport
  update_interval: 60s
  encryption: "MySecretKey123"
  rolling_code_enable: true
  sensors:
    - temperature
```

### Configuration Provider/Consumer

#### Nœud Provider (émetteur)

```yaml
esphome:
  name: lora-node-sensor

uart:
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora
  uart_id: uart_bus
  address: 100
  frequency: 868MHz
  network_id: 10

packet_transport:
  platform: rylr998
  update_interval: 30s
  encryption: "SharedSecret"
  sensors:
    - dht_temp
    - dht_humidity

sensor:
  - platform: dht
    pin: GPIO4
    temperature:
      name: "Température extérieure"
      id: dht_temp
    humidity:
      name: "Humidité extérieure"
      id: dht_humidity
```

#### Nœud Consumer (récepteur)

```yaml
esphome:
  name: lora-gateway

uart:
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora
  uart_id: uart_bus
  address: 200
  frequency: 868MHz
  network_id: 10

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:

packet_transport:
  platform: rylr998
  encryption: "SharedSecret"
  providers:
    - name: lora-node-sensor
      encryption: "SharedSecret"

# Capteurs reçus via LoRa
sensor:
  - platform: packet_transport
    provider: lora-node-sensor
    remote_id: dht_temp
    name: "Température extérieure"
    
  - platform: packet_transport
    provider: lora-node-sensor
    remote_id: dht_humidity
    name: "Humidité extérieure"
```

## Paramètres de configuration

| Paramètre | Type | Défaut | Description |
|-----------|------|--------|-------------|
| `platform` | string | **Requis** | Doit être `rylr998` |
| `id` | ID | Optionnel | ID du composant packet transport |
| `update_interval` | Time | 60s | Intervalle de mise à jour |
| `encryption` | string | Optionnel | Clé de chiffrement partagée |
| `rolling_code_enable` | bool | false | Active la protection contre les attaques par rejeu |
| `ping_pong_enable` | bool | false | Active le mode ping-pong pour vérifier la connectivité |
| `sensors` | list | Optionnel | Liste des IDs de capteurs à transmettre |
| `binary_sensors` | list | Optionnel | Liste des IDs de capteurs binaires à transmettre |
| `providers` | list | Optionnel | Liste des fournisseurs de données distants |

### Configuration des Providers

```yaml
providers:
  - name: remote-node-1
    encryption: "KeyForNode1"  # Optionnel, clé spécifique
  - name: remote-node-2
    # Pas de chiffrement pour ce nœud
```

## Exemple complet : Réseau multi-capteurs

### Station météo distante (Provider)

```yaml
esphome:
  name: weather-station
  platform: ESP32
  board: esp32dev

uart:
  id: uart_bus
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora
  uart_id: uart_bus
  address: 101
  frequency: 868.5MHz
  spreading_factor: 9
  signal_bandwidth: 125kHz
  network_id: 10
  tx_power: 14

# Deep sleep pour économiser la batterie
deep_sleep:
  run_duration: 30s
  sleep_duration: 5min

packet_transport:
  platform: rylr998
  update_interval: 20s
  encryption: "WeatherNet2024"
  rolling_code_enable: true
  sensors:
    - temperature
    - humidity
    - pressure

sensor:
  - platform: bme280
    temperature:
      name: "Température"
      id: temperature
    humidity:
      name: "Humidité"
      id: humidity
    pressure:
      name: "Pression"
      id: pressure
    address: 0x76
    update_interval: 15s
```

### Passerelle centrale (Consumer)

```yaml
esphome:
  name: lora-gateway
  platform: ESP32
  board: esp32dev

uart:
  id: uart_bus
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora
  uart_id: uart_bus
  address: 1
  frequency: 868.5MHz
  spreading_factor: 9
  signal_bandwidth: 125kHz
  network_id: 10
  tx_power: 14

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:
  encryption:
    key: !secret api_encryption_key

ota:
  password: !secret ota_password

packet_transport:
  platform: rylr998
  encryption: "WeatherNet2024"
  rolling_code_enable: true
  ping_pong_enable: true
  providers:
    - name: weather-station
      encryption: "WeatherNet2024"

# Capteurs reçus de la station météo
sensor:
  - platform: packet_transport
    provider: weather-station
    remote_id: temperature
    name: "Température extérieure"
    device_class: temperature
    unit_of_measurement: "°C"
    
  - platform: packet_transport
    provider: weather-station
    remote_id: humidity
    name: "Humidité extérieure"
    device_class: humidity
    unit_of_measurement: "%"
    
  - platform: packet_transport
    provider: weather-station
    remote_id: pressure
    name: "Pression atmosphérique"
    device_class: pressure
    unit_of_measurement: "hPa"

# Capteur de timeout pour surveiller la connectivité
  - platform: packet_transport_ping_timeout
    name: "Station météo - Timeout"
    provider: weather-station
```

## Fonctionnalités de sécurité

### Chiffrement

Le chiffrement protège les données en transit :

```yaml
packet_transport:
  platform: rylr998
  encryption: "MyVerySecretKey"  # Partagée entre tous les nœuds
```

### Rolling Code (Protection anti-rejeu)

Empêche les attaques par rejeu en utilisant un compteur monotone :

```yaml
packet_transport:
  platform: rylr998
  encryption: "MySecretKey"
  rolling_code_enable: true
```

### Chiffrement par Provider

Clés différentes pour chaque nœud distant :

```yaml
packet_transport:
  platform: rylr998
  encryption: "DefaultKey"  # Clé par défaut
  providers:
    - name: secure-node
      encryption: "SpecialKey"  # Clé spécifique pour ce nœud
    - name: public-node
      # Pas de chiffrement pour ce nœud
```

## Mode Ping-Pong

Vérifie la connectivité entre les nœuds :

```yaml
packet_transport:
  platform: rylr998
  ping_pong_enable: true

sensor:
  - platform: packet_transport_ping_timeout
    name: "Node Connection Timeout"
    provider: remote-node
```

## Limitations

1. **Taille des paquets** : Maximum 240 octets par paquet RYLR998
2. **Débit** : Limité par les paramètres LoRa (spreading factor, bandwidth)
3. **Broadcast uniquement** : Actuellement, le RYLR998 envoie en broadcast (adresse 0)
4. **Network ID** : Tous les modules doivent partager le même `network_id`

## Dépannage

### Les données ne sont pas reçues

1. Vérifiez que tous les nœuds ont le même `network_id`
2. Vérifiez que la `frequency` est identique
3. Vérifiez que les paramètres RF sont identiques
4. Vérifiez que la clé de chiffrement est la même

### Données corrompues

1. Réduisez le `spreading_factor` si les paquets sont trop longs
2. Activez `rolling_code_enable` pour détecter les rejeux
3. Vérifiez la qualité du signal (RSSI/SNR)

### Performance

Pour améliorer la portée au détriment du débit :

```yaml
rylr998:
  spreading_factor: 11
  signal_bandwidth: 125kHz
  coding_rate: 1
```

Pour améliorer le débit au détriment de la portée :

```yaml
rylr998:
  spreading_factor: 7
  signal_bandwidth: 500kHz
  coding_rate: 1
```

## Comparaison avec autres transports

| Transport | Portée | Débit | Consommation | Fiabilité |
|-----------|--------|-------|--------------|-----------|
| RYLR998 (LoRa) | +++++ | + | ++ | ++++ |
| ESP-NOW | ++ | ++++ | +++ | +++ |
| UDP (WiFi) | +++ | +++++ | ++ | +++++ |
| UART | + | ++++ | +++++ | +++++ |

## Ressources

- [Documentation Packet Transport](https://esphome.io/components/packet_transport/)
- [Composant RYLR998 de base](./README.md)
- [Documentation REYAX RYLR998](https://reyax.com/products/RYLR998)

## Licence

Ce composant est basé sur la documentation AT du RYLR998 fournie par REYAX Technology.
