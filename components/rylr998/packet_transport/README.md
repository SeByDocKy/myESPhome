# RYLR998 Packet Transport

⚠️ **ATTENTION : Cette fonctionnalité nécessite que le composant `packet_transport` soit disponible dans votre version d'ESPHome.**

## Vérification de compatibilité

Le composant `packet_transport` n'est pas disponible dans toutes les versions d'ESPHome. Pour vérifier si votre version le supporte :

```bash
# Chercher packet_transport dans votre installation ESPHome
find /path/to/esphome -name "packet_transport" -type d
```

Si vous obtenez l'erreur :
```
AttributeError: module 'esphome.components.packet_transport' has no attribute 'PacketTransportComponent'
```

**Cela signifie que votre version d'ESPHome ne supporte pas packet_transport.**

## Solution 1 : Utiliser uniquement le composant de base

Le composant RYLR998 de base fonctionne parfaitement **sans** packet_transport. Vous pouvez :
- Envoyer et recevoir des paquets LoRa
- Utiliser `on_packet` pour recevoir
- Utiliser `send_packet` pour envoyer
- Construire votre propre protocole de communication

**Supprimez simplement le dossier `packet_transport`** et utilisez le composant de base.

## Solution 2 : Mettre à jour ESPHome (si possible)

Si le composant `packet_transport` a été ajouté dans une version plus récente d'ESPHome :

```bash
pip install --upgrade esphome
```

## Solution 3 : Utiliser le composant sx127x comme référence

Le packet_transport était initialement conçu pour le composant SX127x. Si vous avez besoin de cette fonctionnalité, vous pouvez :

1. Regarder comment le composant SX127x implémente packet_transport
2. Adapter le code pour votre version d'ESPHome

## Que fait packet_transport ?

Packet_transport permet de :
- Synchroniser automatiquement des capteurs entre nœuds ESPHome
- Utiliser une couche de protocole standardisée
- Gérer le chiffrement et l'authentification
- Établir des relations Provider/Consumer entre nœuds

**Cependant, tout cela peut être fait manuellement avec le composant de base !**

## Alternative : Protocole manuel

Vous pouvez créer votre propre protocole simple sans packet_transport :

```yaml
# Nœud émetteur (Provider)
rylr998:
  id: lora
  address: 100

sensor:
  - platform: dht
    temperature:
      id: temp
    update_interval: 60s
    on_value:
      then:
        - lambda: |-
            // Format: "T:25.5"
            char buffer[32];
            snprintf(buffer, sizeof(buffer), "T:%.1f", id(temp).state);
            std::string msg(buffer);
            std::vector<uint8_t> data(msg.begin(), msg.end());
            id(lora).send_data(200, data);

# Nœud récepteur (Consumer)
rylr998:
  id: lora
  address: 200
  on_packet:
    then:
      - lambda: |-
          std::string msg(data.begin(), data.end());
          // Parser "T:25.5"
          if (msg.substr(0, 2) == "T:") {
            float temp = std::stof(msg.substr(2));
            id(remote_temp).publish_state(temp);
          }

sensor:
  - platform: template
    name: "Température distante"
    id: remote_temp
```

## Conclusion

**Si packet_transport ne fonctionne pas, utilisez simplement le composant de base RYLR998.**

Il est tout à fait capable de :
✅ Communication LoRa bidirectionnelle
✅ Envoi/réception de données
✅ Broadcast
✅ Protocole personnalisé

Consultez `EXAMPLES.md` pour voir comment construire votre propre protocole de communication.
