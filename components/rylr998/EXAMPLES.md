# Exemples d'envoi de données - RYLR998

Ce guide présente différentes façons d'envoyer des données avec le composant RYLR998.

## 📝 Note importante

Le composant RYLR998 supporte **deux syntaxes** pour le paramètre `data` :

1. **Liste statique** : `data: [0x48, 0x45, 0x4C, 0x4C, 0x4F]` ✅ Simple et directe
2. **Lambda dynamique** : `data: !lambda |- return std::vector<uint8_t>{...};` ✅ Pour données calculées

## Méthode 1 : Liste d'octets statique (Plus simple)

```yaml
button:
  - platform: template
    name: "Envoyer HELLO"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: [0x48, 0x45, 0x4C, 0x4C, 0x4F]  # "HELLO"
```

## Méthode 2 : Caractères ASCII (Plus lisible)

```yaml
button:
  - platform: template
    name: "Envoyer TEST"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: [0x54, 0x45, 0x53, 0x54]  # "TEST"
```

Astuce : Convertir ASCII vers hexadécimal :
- 'A' = 0x41, 'B' = 0x42, 'C' = 0x43...
- 'a' = 0x61, 'b' = 0x62, 'c' = 0x63...
- '0' = 0x30, '1' = 0x31, '2' = 0x32...

## Méthode 3 : Lambda avec caractères (Dynamique)

```yaml
button:
  - platform: template
    name: "Envoyer message"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: !lambda |-
            return std::vector<uint8_t>{'B', 'O', 'N', 'J', 'O', 'U', 'R'};
```

## Méthode 4 : Depuis une string (Lambda)

```yaml
button:
  - platform: template
    name: "Envoyer message"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: !lambda |-
            std::string msg = "BONJOUR";
            return std::vector<uint8_t>(msg.begin(), msg.end());
```

## Méthode 5 : Données binaires hexadécimales

```yaml
button:
  - platform: template
    name: "Envoyer données binaires"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: [0x01, 0x02, 0x03, 0xAA, 0xBB, 0xFF]
```

## Méthode 5 : Envoyer des valeurs de capteurs

```yaml
sensor:
  - platform: dht
    pin: GPIO4
    temperature:
      name: "Température"
      id: temp_sensor

button:
  - platform: template
    name: "Envoyer température"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: !lambda |-
            char buffer[32];
            float temp = id(temp_sensor).state;
            snprintf(buffer, sizeof(buffer), "TEMP:%.1f", temp);
            std::string msg(buffer);
            return std::vector<uint8_t>(msg.begin(), msg.end());
```

## Méthode 6 : Format JSON

```yaml
button:
  - platform: template
    name: "Envoyer JSON"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: !lambda |-
            float temp = id(temp_sensor).state;
            float hum = id(humidity_sensor).state;
            char buffer[64];
            snprintf(buffer, sizeof(buffer), 
                     "{\"temp\":%.1f,\"hum\":%.1f}", temp, hum);
            std::string msg(buffer);
            return std::vector<uint8_t>(msg.begin(), msg.end());
```

## Méthode 7 : Broadcast à tous les nœuds

```yaml
button:
  - platform: template
    name: "Broadcast ALERT"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 0  # 0 = broadcast
          data: !lambda |-
            return std::vector<uint8_t>{'A', 'L', 'E', 'R', 'T'};
```

## Méthode 8 : Lambda complet dans un script

```yaml
sensor:
  - platform: dht
    pin: GPIO4
    temperature:
      id: temp
    humidity:
      id: hum
    update_interval: 60s
    on_value:
      then:
        - lambda: |-
            // Construire le message
            char buffer[50];
            snprintf(buffer, sizeof(buffer), "T:%.1f,H:%.1f", 
                     id(temp).state, id(hum).state);
            std::string msg(buffer);
            std::vector<uint8_t> data(msg.begin(), msg.end());
            
            // Envoyer via LoRa
            id(lora).send_data(200, data);
```

## Méthode 9 : Données binaires structurées

```yaml
button:
  - platform: template
    name: "Envoyer structure"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: !lambda |-
            std::vector<uint8_t> data;
            
            // Header
            data.push_back(0xAA);  // Start marker
            data.push_back(0x01);  // Type: sensor data
            
            // Temperature (2 bytes, little endian)
            int16_t temp = (int16_t)(id(temp_sensor).state * 10);
            data.push_back(temp & 0xFF);
            data.push_back((temp >> 8) & 0xFF);
            
            // Checksum
            uint8_t checksum = 0;
            for (auto byte : data) checksum ^= byte;
            data.push_back(checksum);
            
            return data;
```

## Méthode 10 : Avec variable templatable (avancé)

Si vous voulez une destination variable :

```yaml
globals:
  - id: target_address
    type: int
    initial_value: '200'

button:
  - platform: template
    name: "Envoyer au target"
    on_press:
      - lambda: |-
          std::string msg = "HELLO";
          std::vector<uint8_t> data(msg.begin(), msg.end());
          id(lora).send_data(id(target_address), data);
```

## Erreurs courantes à éviter

### ❌ INCORRECT - Ancienne version qui ne compilait pas

```yaml
# Ceci ne fonctionnait PAS dans les versions précédentes
button:
  - platform: template
    name: "Test"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: [0x48, 0x45, 0x4C, 0x4C, 0x4F]  # ❌ Erreur dans les anciennes versions
```

**Erreur ancienne** : `couldn't deduce template parameter 'V'`

### ✅ CORRECT - Version actuelle

```yaml
# Maintenant ceci fonctionne parfaitement !
button:
  - platform: template
    name: "Test"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: [0x48, 0x45, 0x4C, 0x4C, 0x4F]  # ✅ Fonctionne !
```

### 💡 Les deux syntaxes sont supportées

```yaml
# Syntaxe 1 : Liste statique (simple)
- rylr998.send_packet:
    id: lora
    destination: 200
    data: [0x48, 0x45, 0x4C, 0x4C, 0x4F]

# Syntaxe 2 : Lambda (dynamique)
- rylr998.send_packet:
    id: lora
    destination: 200
    data: !lambda |-
      return std::vector<uint8_t>{'H', 'E', 'L', 'L', 'O'};
```

## Réception et décodage

Pour recevoir et décoder les données :

```yaml
rylr998:
  id: lora
  on_packet:
    then:
      - lambda: |-
          // Convertir en string
          std::string msg(data.begin(), data.end());
          ESP_LOGI("lora", "Reçu de %d: %s (RSSI: %d, SNR: %d)", 
                   address, msg.c_str(), rssi, snr);
          
          // Parser un format "KEY:VALUE"
          size_t pos = msg.find(':');
          if (pos != std::string::npos) {
            std::string key = msg.substr(0, pos);
            std::string value = msg.substr(pos + 1);
            ESP_LOGI("lora", "Key: %s, Value: %s", 
                     key.c_str(), value.c_str());
          }
```

## Limites importantes

1. **Taille maximale** : 240 octets par paquet
2. **Pas de caractères nuls** : Les `\0` peuvent causer des problèmes
3. **ASCII recommandé** : Plus facile à débugger
4. **Temps d'émission** : Varie selon les paramètres LoRa

## Calcul du temps d'émission

Le temps d'émission dépend de :
- `spreading_factor` (plus élevé = plus lent)
- `signal_bandwidth` (plus faible = plus lent)
- Taille des données

Exemple avec SF9, BW125kHz :
- 10 octets : ~250 ms
- 50 octets : ~500 ms
- 100 octets : ~1000 ms
- 240 octets : ~2500 ms

⚠️ Ne pas envoyer trop de paquets trop rapidement !

## Conseils

1. **Commencez simple** : Testez d'abord avec "HELLO"
2. **Ajoutez des logs** : Utilisez `logger.log` pour débugger
3. **Vérifiez RSSI/SNR** : Qualité du signal dans `on_packet`
4. **Format structuré** : Utilisez un format clair (JSON, CSV, etc.)
5. **Gestion d'erreurs** : Vérifiez que `send_data()` retourne `true`

## Exemple complet : Station météo

```yaml
esphome:
  name: weather-station

uart:
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

rylr998:
  id: lora
  address: 100
  frequency: 868MHz

sensor:
  - platform: bme280
    temperature:
      id: temperature
    humidity:
      id: humidity
    pressure:
      id: pressure
    update_interval: 60s
    on_value:
      then:
        - lambda: |-
            // Format: "T:25.5,H:60.2,P:1013.2"
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "T:%.1f,H:%.1f,P:%.1f",
                     id(temperature).state,
                     id(humidity).state,
                     id(pressure).state);
            
            std::string msg(buffer);
            std::vector<uint8_t> data(msg.begin(), msg.end());
            
            if (id(lora).send_data(200, data)) {
              ESP_LOGI("main", "Données envoyées: %s", buffer);
            } else {
              ESP_LOGE("main", "Échec de l'envoi");
            }
```

Bon codage ! 🚀
