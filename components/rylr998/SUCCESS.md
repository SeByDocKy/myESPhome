# ✅ Configurations testées et validées

Ce document liste les configurations qui ont été testées avec succès.

## 🎉 Configuration de base - ✅ FONCTIONNE

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
  frequency: 868MHz
  spreading_factor: 9
  signal_bandwidth: 125kHz
  coding_rate: 1
  preamble_length: 12
  network_id: 10
  tx_power: 14
  on_packet:
    then:
      - logger.log:
          format: "Paquet reçu: RSSI=%.1f SNR=%.1f"
          args: ['rssi', 'snr']
      - lambda: |-
          std::string msg(data.begin(), data.end());
          ESP_LOGI("main", "Données reçues: %s", msg.c_str());

button:
  - platform: template
    name: "Envoyer Test LoRa"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: [0x48, 0x45, 0x4C, 0x4C, 0x4F]
      - logger.log: "Message envoyé!"
```

**✅ Compilation** : OK  
**✅ Upload** : OK  
**✅ Fonctionnement** : OK

---

## 🎉 Configuration avec Packet Transport - ✅ FONCTIONNE

### Nœud Sensor (Provider)

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
  frequency: 868MHz
  spreading_factor: 9
  signal_bandwidth: 125kHz
  coding_rate: 1
  preamble_length: 12
  network_id: 10
  tx_power: 14
  on_packet:
    then:
      - logger.log:
          format: "Paquet reçu: RSSI=%.1f SNR=%.1f"
          args: ['rssi', 'snr']
      - lambda: |-
          std::string msg(data.begin(), data.end());
          ESP_LOGI("main", "Données reçues: %s", msg.c_str());

button:
  - platform: template
    name: "Envoyer Test LoRa"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: [0x48, 0x45, 0x4C, 0x4C, 0x4F]
      - logger.log: "Message envoyé!"

packet_transport:
  platform: rylr998
  rylr998_id: lora  # ⚠️ Nouveau nom (pas packet_transport_id)
  update_interval: 30s
  encryption: "MySecretKey123"
  sensors:
    - room_temperature
  providers:
    - name: lora-gateway
      encryption: "loralora"

sensor:
  - platform: dht
    pin: GPIO4
    temperature:
      name: "Température"
      id: room_temperature
    humidity:
      name: "Humidité"
  
  - platform: packet_transport
    provider: lora-gateway
    remote_id: active_power
    id: local_active_power
```

**✅ Compilation** : OK  
**✅ Upload** : OK  
**✅ Fonctionnement** : OK

---

## 📝 Notes importantes

### 1. L'include `automation.h` n'est pas nécessaire

Le fichier `rylr998.h` **ne doit PAS** inclure `automation.h` à la fin. L'include se fait automatiquement via Python.

### 2. Arguments du trigger `on_packet`

Les variables disponibles dans `on_packet` sont :
- ✅ `data` : `std::vector<uint8_t>`
- ✅ `rssi` : `float` (⚠️ pas `int`)
- ✅ `snr` : `float` (⚠️ pas `int`)
- ❌ `address` : **N'EXISTE PLUS**

### 3. Format des logs

Utilisez `%.1f` pour RSSI et SNR (float), pas `%d` (int) :

```yaml
# ✅ Correct
format: "RSSI=%.1f SNR=%.1f"

# ❌ Incorrect
format: "RSSI=%d SNR=%d"
```

### 4. Packet Transport - Nom du paramètre

```yaml
# ✅ Correct (v2.0)
packet_transport:
  platform: rylr998
  rylr998_id: lora

# ❌ Incorrect (v1.0 - obsolète)
packet_transport:
  platform: rylr998
  packet_transport_id: lora
```

### 5. Héritage de PacketTransport

`RYLR998Transport` hérite de `PacketTransport` qui hérite déjà de `PollingComponent`.

**Ne pas** hériter deux fois de `PollingComponent` !

---

## 🔧 Conseils de débogage

### Activer les logs détaillés

```yaml
logger:
  level: VERBOSE
  logs:
    rylr998: VERBOSE
    packet_transport: VERBOSE
```

### Vérifier la communication LoRa

1. **Envoi** : Logs `Sending to X: Y`
2. **Réception** : Logs `Received message from X`
3. **RSSI/SNR** : Qualité du signal

### Tester sans packet_transport d'abord

Avant d'activer packet_transport, testez d'abord :
1. L'envoi de paquets simples
2. La réception de paquets
3. Le trigger `on_packet`

Une fois que cela fonctionne, ajoutez packet_transport.

---

## 📊 Paramètres testés

| Paramètre | Valeur testée | Statut |
|-----------|---------------|--------|
| `frequency` | 868MHz | ✅ OK |
| `spreading_factor` | 9 | ✅ OK |
| `signal_bandwidth` | 125kHz | ✅ OK |
| `coding_rate` | 1 | ✅ OK |
| `preamble_length` | 12 | ✅ OK |
| `network_id` | 10 | ✅ OK |
| `tx_power` | 14 | ✅ OK |
| `update_interval` (PT) | 30s | ✅ OK |
| `encryption` (PT) | Activé | ✅ OK |

---

## 🚀 Prochaines étapes

### Tests recommandés

1. ✅ Test de portée (RSSI/SNR)
2. ✅ Test de débit (paquets/seconde)
3. ✅ Test de fiabilité (perte de paquets)
4. ✅ Test de batterie (deep sleep)

### Configurations avancées

Voir les exemples complets :
- `example_packet_transport_working.yaml` - Nœud sensor complet
- `example_gateway.yaml` - Gateway avec WiFi

---

## 🎓 Leçons apprises

1. **Architecture Listener** : Suivre exactement le pattern SX127x
2. **Héritage multiple** : Attention aux ambiguïtés
3. **Méthodes virtuelles pures** : Implémenter `update()` et `get_max_packet_size()`
4. **Imports Python** : Utiliser `new_packet_transport()` et `transport_schema()`
5. **Breaking changes** : Documenter clairement les changements

---

## 📚 Ressources

- [example_packet_transport_working.yaml](./example_packet_transport_working.yaml) - Configuration testée
- [example_gateway.yaml](./example_gateway.yaml) - Configuration gateway
- [BREAKING_CHANGES.md](./BREAKING_CHANGES.md) - Migration v1.0 → v2.0
- [ARCHITECTURE.md](./ARCHITECTURE.md) - Architecture du composant

---

## ✅ Statut final

| Composant | Compilation | Tests | Statut |
|-----------|-------------|-------|--------|
| **rylr998 basic** | ✅ | ✅ | **STABLE** |
| **rylr998 packet_transport** | ✅ | ✅ | **STABLE** |

**Version** : v2.0  
**Dernière mise à jour** : 2026-02-07  
**Testé sur** : ESPHome 2026.2.0-dev

---

Bon codage ! 🎉
