# Breaking Changes - RYLR998 v2.0

Ce document liste les changements incompatibles avec la version précédente du composant RYLR998.

## ⚠️ Changements importants

### 1. Trigger `on_packet` - Arguments modifiés

**Avant (v1.0)** :
```yaml
rylr998:
  on_packet:
    then:
      - logger.log:
          format: "Reçu de %d: RSSI=%d SNR=%d"
          args: ['address', 'rssi', 'snr']  # ❌ address n'existe plus
      - lambda: |-
          ESP_LOGD("main", "De %d", address);  # ❌ ERREUR !
```

**Maintenant (v2.0)** :
```yaml
rylr998:
  on_packet:
    then:
      - logger.log:
          format: "RSSI=%.1f SNR=%.1f"  # Notez: float maintenant
          args: ['rssi', 'snr']  # ✅ Plus d'address
      - lambda: |-
          std::string msg(data.begin(), data.end());
          ESP_LOGI("main", "Données: %s", msg.c_str());
```

### 2. Types des variables dans `on_packet`

| Variable | Avant (v1.0) | Maintenant (v2.0) |
|----------|--------------|-------------------|
| `address` | `uint16_t` | ❌ **Supprimé** |
| `data` | `std::vector<uint8_t>` | `std::vector<uint8_t>` ✅ |
| `rssi` | `int` | `float` ⚠️ |
| `snr` | `int` | `float` ⚠️ |

### 3. Format des logs

**Avant** :
```cpp
ESP_LOGD("main", "RSSI=%d SNR=%d", rssi, snr);  // int
```

**Maintenant** :
```cpp
ESP_LOGD("main", "RSSI=%.1f SNR=%.1f", rssi, snr);  // float
```

### 4. Classe PacketTransport renommée

**Avant** :
```python
RYLR998PacketTransportComponent
```

**Maintenant** :
```python
RYLR998Transport  # Aligné avec SX127x
```

### 5. Configuration packet_transport

**Avant (v1.0)** :
```yaml
packet_transport:
  platform: rylr998
  packet_transport_id: lora
```

**Maintenant (v2.0)** :
```yaml
packet_transport:
  - platform: rylr998
    rylr998_id: lora  # Nom changé
```

## 🔄 Migration automatique

### Script de migration pour vos fichiers YAML

```python
#!/usr/bin/env python3
import re
import sys

def migrate_yaml(content):
    # Migration 1: Supprimer 'address' des args
    content = re.sub(
        r"args:\s*\['address',\s*'rssi',\s*'snr'\]",
        "args: ['rssi', 'snr']",
        content
    )
    
    # Migration 2: Changer %d en %.1f pour RSSI/SNR
    content = re.sub(
        r'format:\s*"([^"]*?)RSSI=%d([^"]*?)SNR=%d([^"]*?)"',
        r'format: "\1RSSI=%.1f\2SNR=%.1f\3"',
        content
    )
    
    # Migration 3: packet_transport_id -> rylr998_id
    content = re.sub(
        r'packet_transport_id:',
        'rylr998_id:',
        content
    )
    
    return content

if __name__ == "__main__":
    with open(sys.argv[1], 'r') as f:
        content = f.read()
    
    migrated = migrate_yaml(content)
    
    with open(sys.argv[1], 'w') as f:
        f.write(migrated)
    
    print(f"✅ Fichier {sys.argv[1]} migré vers v2.0")
```

**Usage** :
```bash
python migrate_rylr998.py votre_config.yaml
```

## 📝 Exemples de migration

### Exemple 1 : Logger simple

**Avant** :
```yaml
on_packet:
  then:
    - logger.log:
        format: "De %d: RSSI=%d dBm"
        args: ['address', 'rssi']
```

**Après** :
```yaml
on_packet:
  then:
    - logger.log:
        format: "RSSI=%.1f dBm"
        args: ['rssi']
```

### Exemple 2 : Lambda avec address

**Avant** :
```yaml
on_packet:
  then:
    - lambda: |-
        ESP_LOGI("lora", "De %d: %s", address, data);
        if (address == 200) {
          // Traiter
        }
```

**Après** :
```yaml
on_packet:
  then:
    - lambda: |-
        std::string msg(data.begin(), data.end());
        ESP_LOGI("lora", "Données: %s", msg.c_str());
        // Note: Plus d'address disponible
        // L'adresse source n'est plus exposée dans le trigger
```

### Exemple 3 : RSSI/SNR dans conditions

**Avant** :
```yaml
on_packet:
  then:
    - if:
        condition:
          lambda: 'return rssi > -80;'  # int
        then:
          - logger.log: "Bon signal"
```

**Après** :
```yaml
on_packet:
  then:
    - if:
        condition:
          lambda: 'return rssi > -80.0f;'  # float
        then:
          - logger.log: "Bon signal"
```

## 🤔 Pourquoi ces changements ?

### Architecture alignée avec SX127x

Ces changements permettent :
1. **Cohérence** avec le composant SX127x d'ESPHome
2. **Pattern Listener** pour l'extensibilité
3. **Précision** : RSSI/SNR en float pour plus de précision
4. **Simplicité** : Le trigger se concentre sur les données reçues

### Pourquoi supprimer `address` ?

Le RYLR998 fonctionne en **broadcast par défaut** dans notre implémentation actuelle. L'adresse source n'est pas toujours fiable dans le protocole LoRa sans couche applicative supplémentaire.

Si vous avez vraiment besoin de l'adresse source, vous pouvez :
- L'inclure dans les **données** du paquet
- Utiliser **packet_transport** qui gère cela automatiquement

### Exemple avec adresse dans les données

```yaml
# Émetteur
button:
  - platform: template
    name: "Envoyer avec ID"
    on_press:
      - rylr998.send_packet:
          id: lora
          destination: 200
          data: !lambda |-
            // Format: "ID:100,DATA:HELLO"
            std::string msg = "ID:100,DATA:HELLO";
            return std::vector<uint8_t>(msg.begin(), msg.end());

# Récepteur
rylr998:
  on_packet:
    then:
      - lambda: |-
          std::string msg(data.begin(), data.end());
          
          // Parser "ID:100,DATA:HELLO"
          size_t id_pos = msg.find("ID:");
          size_t data_pos = msg.find(",DATA:");
          
          if (id_pos != std::string::npos && data_pos != std::string::npos) {
            std::string id_str = msg.substr(3, data_pos - 3);
            int sender_id = std::stoi(id_str);
            std::string payload = msg.substr(data_pos + 6);
            
            ESP_LOGI("main", "De %d: %s", sender_id, payload.c_str());
          }
```

## ✅ Checklist de migration

- [ ] Supprimer `address` de tous les `args:` dans `on_packet`
- [ ] Remplacer `%d` par `%.1f` pour RSSI/SNR dans les formats
- [ ] Mettre à jour les lambdas qui utilisent `address`
- [ ] Changer `packet_transport_id:` en `rylr998_id:` (si packet_transport utilisé)
- [ ] Vérifier que les conditions sur RSSI/SNR utilisent des float
- [ ] Tester la compilation
- [ ] Tester l'envoi/réception de paquets

## 🆘 Aide

### Erreur: `'address' was not declared in this scope`

**Cause** : Votre YAML utilise encore `address` dans un lambda.

**Solution** :
1. Cherchez tous les `address` dans votre YAML
2. Supprimez-les ou remplacez-les par une solution alternative (voir ci-dessus)

### Erreur: Format specifier mismatch

**Cause** : Utilisation de `%d` au lieu de `%.1f` pour RSSI/SNR.

**Solution** :
```yaml
# ❌ Avant
format: "RSSI=%d"

# ✅ Après
format: "RSSI=%.1f"
```

## 📚 Ressources

- [ARCHITECTURE.md](./ARCHITECTURE.md) - Nouvelle architecture Listener
- [EXAMPLES.md](./EXAMPLES.md) - Exemples mis à jour
- [README.md](./README.md) - Documentation complète

## 🔄 Historique

- **v2.0** (Actuelle) : Architecture Listener, trigger simplifié
- **v1.0** (Obsolète) : Callbacks directs, trigger avec address
