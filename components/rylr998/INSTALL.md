# Guide d'installation - Composant RYLR998 pour ESPHome

## Prérequis

- ESPHome installé et configuré
- Module RYLR998 ou RYLR498
- ESP32 ou ESP8266
- Connexion UART disponible

## Installation Étape par Étape

### Option 1 : Installation de Base (Recommandé pour débuter)

Cette installation permet d'utiliser le RYLR998 pour envoyer et recevoir des paquets LoRa.

#### 1. Créer la structure des dossiers

Dans votre répertoire de configuration ESPHome, créez :
```
config/
├── custom_components/
│   └── rylr998/
│       ├── __init__.py
│       ├── rylr998.h
│       └── rylr998.cpp
└── test_rylr998.yaml
```

#### 2. Copier les fichiers

Copiez **uniquement** ces 3 fichiers dans `custom_components/rylr998/` :
- `__init__.py`
- `rylr998.h`
- `rylr998.cpp`

#### 3. Créer votre configuration YAML

Utilisez `example_basic.yaml` comme point de départ ou créez votre propre fichier :

```yaml
esphome:
  name: mon-node-lora
  platform: ESP32
  board: esp32dev

logger:

uart:
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora
  address: 100
  frequency: 868MHz
  on_packet:
    then:
      - logger.log:
          format: "Reçu de %d: %s"
          args: ['address', 'data']

button:
  - platform: template
    name: "Test"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: [0x48, 0x45, 0x4C, 0x4C, 0x4F]
```

#### 4. Compiler et uploader

```bash
esphome run test_rylr998.yaml
```

### Option 2 : Installation avec Packet Transport (Avancé)

Cette option ajoute le support pour la communication inter-nœuds ESPHome automatique.

⚠️ **IMPORTANT** : Le packet_transport nécessite que ce composant soit disponible dans votre version d'ESPHome. Si vous obtenez une erreur de compilation concernant `packet_transport.h`, utilisez l'Option 1 à la place.

#### 1. Créer la structure des dossiers

```
config/
├── custom_components/
│   └── rylr998/
│       ├── __init__.py
│       ├── rylr998.h
│       ├── rylr998.cpp
│       ├── packet_transport.py
│       ├── rylr998_packet_transport.h
│       └── rylr998_packet_transport.cpp
└── test_rylr998.yaml
```

#### 2. Copier TOUS les fichiers

Copiez les 6 fichiers dans `custom_components/rylr998/` :
- `__init__.py`
- `rylr998.h`
- `rylr998.cpp`
- `packet_transport.py` ← Nouveau
- `rylr998_packet_transport.h` ← Nouveau
- `rylr998_packet_transport.cpp` ← Nouveau

#### 3. Créer votre configuration YAML

Utilisez `example_packet_transport.yaml` comme point de départ.

#### 4. Compiler et uploader

```bash
esphome run test_rylr998.yaml
```

## Vérification de l'installation

### Test de communication

1. Ouvrez les logs :
   ```bash
   esphome logs test_rylr998.yaml
   ```

2. Au démarrage, vous devriez voir :
   ```
   [D][rylr998:XXX]: Setting up RYLR998...
   [D][rylr998:XXX]: Resetting module...
   [C][rylr998:XXX]: RYLR998:
   [C][rylr998:XXX]:   Address: 100
   [C][rylr998:XXX]:   Frequency: 868000000 Hz
   [C][rylr998:XXX]:   Spreading Factor: 9
   ```

3. Appuyez sur le bouton "Test" dans Home Assistant ou ESPHome

4. Vous devriez voir dans les logs :
   ```
   [D][rylr998:XXX]: Sending to 200: HELLO
   ```

## Résolution des problèmes

### Erreur : `packet_transport.h: No such file or directory`

**Solution** : Votre version d'ESPHome ne supporte pas packet_transport.
- Utilisez l'Installation de Base (Option 1)
- Supprimez les fichiers `packet_transport.*` et `rylr998_packet_transport.*`

### Le module ne répond pas

**Vérifications** :
1. Connexions UART correctes (TX ↔ RX inversés)
2. Alimentation 3.3V stable
3. Baud rate à 115200
4. Logs debug activés : `logger: level: DEBUG`

### Paquets non reçus entre modules

**Vérifications** :
1. Même `network_id` sur tous les modules
2. Même `frequency`
3. Mêmes paramètres RF (spreading_factor, signal_bandwidth, etc.)
4. Portée et obstacles

### Erreur de compilation générique

1. Vérifiez que tous les fichiers sont dans `custom_components/rylr998/`
2. Nettoyez le cache : `esphome clean test_rylr998.yaml`
3. Recompilez

## Connexions matérielles

### RYLR998 ↔ ESP32

| RYLR998 | ESP32 | Notes |
|---------|-------|-------|
| VCC | 3.3V | ⚠️ Pas 5V ! |
| GND | GND | |
| TXD | GPIO16 | RX de l'ESP32 |
| RXD | GPIO17 | TX de l'ESP32 |

### RYLR998 ↔ ESP8266

| RYLR998 | ESP8266 | Notes |
|---------|---------|-------|
| VCC | 3.3V | |
| GND | GND | |
| TXD | GPIO13 | Software Serial RX |
| RXD | GPIO15 | Software Serial TX |

**Note ESP8266** : Utilisez SoftwareSerial car l'UART matériel est utilisé pour les logs.

## Exemples de configuration

### Configuration Europe (868 MHz)

```yaml
rylr998:
  frequency: 868MHz
  tx_power: 14  # Max pour CE
```

### Configuration US (915 MHz)

```yaml
rylr998:
  frequency: 915MHz
  tx_power: 22
```

### Configuration RYLR498 (490 MHz)

```yaml
rylr998:
  frequency: 490MHz
```

### Portée maximale

```yaml
rylr998:
  spreading_factor: 11
  signal_bandwidth: 125kHz
  coding_rate: 1
```

### Débit maximal

```yaml
rylr998:
  spreading_factor: 7
  signal_bandwidth: 500kHz
  coding_rate: 1
```

## Documentation complémentaire

- [README.md](./README.md) - Documentation complète du composant de base
- [README_PACKET_TRANSPORT.md](./README_PACKET_TRANSPORT.md) - Documentation packet_transport
- [Documentation REYAX RYLR998](https://reyax.com/products/RYLR998)

## Support

Si vous rencontrez des problèmes :

1. Vérifiez les logs avec `esphome logs`
2. Activez le niveau DEBUG : `logger: level: DEBUG`
3. Consultez la section "Dépannage" dans README.md
4. Vérifiez que vous utilisez la bonne option d'installation (1 ou 2)

## Notes importantes

- ⚠️ Le RYLR998 fonctionne en **3.3V uniquement**
- 📏 Maximum **240 octets** par paquet
- 🔒 Pour l'Europe : `tx_power: 14` max (conformité CE)
- 🔄 Tous les modules doivent avoir le même `network_id` pour communiquer
- 📡 Antenne obligatoire ! Ne pas émettre sans antenne.
