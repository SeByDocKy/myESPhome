# Architecture RYLR998 - Comme SX127x

Ce document explique la nouvelle architecture du composant RYLR998, maintenant alignée avec le composant SX127x d'ESPHome.

## 🎯 Changements principaux

### 1. Pattern Listener

Comme dans SX127x, nous utilisons maintenant le pattern Listener :

```cpp
// Interface Listener
class RYLR998Listener {
 public:
  virtual void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) = 0;
};

// Le composant principal gère les listeners
class RYLR998Component {
  void register_listener(RYLR998Listener *listener);
  std::vector<RYLR998Listener *> listeners_;
};

// RYLR998Transport implémente RYLR998Listener
class RYLR998Transport : public PacketTransport, 
                         public PollingComponent,
                         public RYLR998Listener {
  void on_packet(const std::vector<uint8_t> &packet, float rssi, float snr) override;
};
```

### 2. Méthode transmit_packet()

Alignée avec SX127x :

```cpp
// Dans rylr998.h
bool transmit_packet(const std::vector<uint8_t> &data);
bool transmit_packet(uint16_t destination, const std::vector<uint8_t> &data);

// Dans rylr998_packet_transport.cpp
void RYLR998Transport::send_packet(const std::vector<uint8_t> &buf) const {
  this->parent_->transmit_packet(buf);
}
```

### 3. Trigger standardisé

```cpp
// Comme dans SX127x
Trigger<std::vector<uint8_t>, float, float> *get_packet_trigger();

// Au lieu de
Trigger<uint16_t, std::vector<uint8_t>, int, int>
```

### 4. Classe renommée

```
RYLR998PacketTransportComponent → RYLR998Transport
```

Pour être cohérent avec `SX127xTransport`.

## 📊 Comparaison SX127x vs RYLR998

| Fonctionnalité | SX127x | RYLR998 |
|----------------|--------|---------|
| **Listener interface** | `SX127xListener` | `RYLR998Listener` ✅ |
| **Transport class** | `SX127xTransport` | `RYLR998Transport` ✅ |
| **Register listener** | `register_listener()` | `register_listener()` ✅ |
| **Send method** | `transmit_packet()` | `transmit_packet()` ✅ |
| **Trigger** | `get_packet_trigger()` | `get_packet_trigger()` ✅ |
| **Trigger args** | `(data, rssi, snr)` | `(data, rssi, snr)` ✅ |

## 🔄 Migration du code

### Ancien format (ne fonctionne plus)

```yaml
rylr998:
  on_packet:
    then:
      - logger.log:
          format: "De %d: RSSI=%d"
          args: ['address', 'rssi']  # ❌ Plus d'address
```

### Nouveau format

```yaml
rylr998:
  on_packet:
    then:
      - logger.log:
          format: "RSSI=%f SNR=%f"  # Notez: float maintenant
          args: ['rssi', 'snr']
      - lambda: |-
          std::string msg(data.begin(), data.end());
          ESP_LOGI("main", "Données: %s", msg.c_str());
```

## 🏗️ Structure des classes

### rylr998.h

```cpp
class RYLR998Component : public Component, public uart::UARTDevice {
 public:
  // Enregistrer un listener (comme packet_transport)
  void register_listener(RYLR998Listener *listener);
  
  // Obtenir le trigger pour automation
  Trigger<std::vector<uint8_t>, float, float> *get_packet_trigger();
  
  // Transmettre un paquet (appelé par les listeners)
  bool transmit_packet(const std::vector<uint8_t> &data);
  bool transmit_packet(uint16_t destination, const std::vector<uint8_t> &data);
  
 protected:
  std::vector<RYLR998Listener *> listeners_;
  Trigger<std::vector<uint8_t>, float, float> packet_trigger_;
};

class RYLR998Listener {
 public:
  virtual void on_packet(const std::vector<uint8_t> &packet, 
                        float rssi, float snr) = 0;
};
```

### packet_transport/rylr998_packet_transport.h

```cpp
class RYLR998Transport : public packet_transport::PacketTransport, 
                         public PollingComponent,
                         public RYLR998Listener {
 public:
  void set_parent(RYLR998Component *parent) {
    this->parent_ = parent;
    parent->register_listener(this);  // S'enregistre comme listener
  }
  
  // PacketTransport interface
  void send_packet(const std::vector<uint8_t> &buf) const override;
  
  // RYLR998Listener interface
  void on_packet(const std::vector<uint8_t> &packet, 
                float rssi, float snr) override;
};
```

### packet_transport/rylr998_packet_transport.cpp

```cpp
void RYLR998Transport::send_packet(const std::vector<uint8_t> &buf) const {
  this->parent_->transmit_packet(buf);
}

void RYLR998Transport::on_packet(const std::vector<uint8_t> &packet, 
                                 float rssi, float snr) {
  this->process_(packet);  // Traite via PacketTransport
}
```

## 🔌 Flux de données

### Réception

```
Module RYLR998
    ↓
UART RX
    ↓
RYLR998Component::process_rx_line_()
    ↓
    ├─→ Notifie tous les listeners (for each listener)
    │   └─→ RYLR998Transport::on_packet()
    │       └─→ process_() [PacketTransport]
    │
    ├─→ Déclenche le trigger
    │   └─→ Automations YAML (on_packet)
    │
    └─→ Appelle les callbacks legacy
        └─→ Compatibilité arrière
```

### Transmission

```
YAML automation ou PacketTransport
    ↓
RYLR998Transport::send_packet()
    ↓
RYLR998Component::transmit_packet()
    ↓
RYLR998Component::send_data()
    ↓
Commande AT+SEND via UART
    ↓
Module RYLR998
```

## ✅ Avantages de cette architecture

1. **Cohérence** : Même pattern que SX127x
2. **Extensibilité** : Facile d'ajouter d'autres listeners
3. **Découplage** : PacketTransport ne dépend que de l'interface
4. **Compatibilité** : Support des callbacks legacy

## 🧪 Exemple complet

```yaml
# Composant de base
uart:
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora_radio
  address: 100
  frequency: 868MHz
  on_packet:
    then:
      - logger.log:
          format: "Paquet reçu: RSSI=%.1f dBm, SNR=%.1f dB"
          args: ['rssi', 'snr']

# Packet Transport (optionnel)
packet_transport:
  - platform: rylr998
    packet_transport_id: lora_radio
    update_interval: 60s
    sensors:
      - temperature

sensor:
  - platform: dht
    temperature:
      id: temperature
```

## 🎓 Pour les développeurs

### Créer votre propre listener

```cpp
class MyCustomListener : public RYLR998Listener {
 public:
  void setup(RYLR998Component *parent) {
    parent->register_listener(this);
  }
  
  void on_packet(const std::vector<uint8_t> &packet, 
                float rssi, float snr) override {
    // Votre logique personnalisée
    ESP_LOGI("custom", "Reçu %d octets", packet.size());
  }
};
```

### Envoyer depuis votre listener

```cpp
class MyCustomListener : public RYLR998Listener {
  void send_my_data() {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03};
    parent_->transmit_packet(data);
  }
  
 private:
  RYLR998Component *parent_;
};
```

## 📚 Références

- [SX127x Component](https://github.com/esphome/esphome/tree/dev/esphome/components/sx127x)
- [ESPHome Packet Transport](https://esphome.io/components/packet_transport/)
- Voir `STRUCTURE.md` pour l'organisation des fichiers

## 🔄 Historique des versions

### v2.0 (Actuelle)
- ✅ Architecture Listener comme SX127x
- ✅ Classe RYLR998Transport
- ✅ Trigger standardisé (data, rssi, snr)
- ✅ Méthode transmit_packet()

### v1.0 (Obsolète)
- ❌ Callbacks directs
- ❌ Trigger avec address
- ❌ RYLR998PacketTransportComponent
