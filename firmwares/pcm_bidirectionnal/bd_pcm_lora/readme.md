# Documentation des fonctionnalités & Mises à jour

Ce document résume les dernières améliorations apportées au firmware ESPHome.

---

## 📋 Nouveautés de la version

### 1. Contrôle de la charge et de la décharge (`switch`)
Deux interrupteurs ont été ajoutés pour vous permettre d'activer ou de désactiver manuellement le flux d'énergie de la batterie :

* **`allow_charging`** : Autorise ou bloque la charge de la batterie.
* **`allow_discharging`** : Autorise ou bloque la décharge de la batterie.

---

### 2. Réglage des deltas de veille (`number`)
Deux paramètres numériques configurables permettent d'ajuster les seuils de tolérance (*deltas*) pour le passage en mode veille (Idle) :

* **`delta_idle_charging`** : Définit la marge de puissance/courant sous laquelle la charge passe en état passif.
* **`delta_idle_discharging`** : Définit la marge de puissance/courant sous laquelle la décharge passe en état passif.

---

### 3. Mise à jour du firmware à chaud (`update: http_request`)
Le système intègre désormais la mise à jour Over-The-Air (OTA) autonome via un manifeste JSON distant sur GitHub.

#### Fonctionnement
1. L'ESP32 interroge le fichier `manifest.json` présent sur GitHub.
2. Il compare la version courante avec la version distante et vérifie l'empreinte **MD5**.
3. Vous pouvez déclencher la mise à jour directement depuis l'interface ESPHome / Home Assistant ou via le bouton dédié.

> **Remarque :** Les identifiants Wi-Fi et les configurations conservés en mémoire **NVS** ne sont jamais effacés lors d'une mise à jour de firmware.

---

## 🛠️ Structure du manifeste distant (`manifest.json`)

Le fichier `manifest.json` hébergé sur GitHub doit respecter la structure suivante :

```json
{
  "name": "bd_pcm",
  "version": "1.0.4",
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "ota": {
        "md5": "VOTRE_MD5_ICI",
        "path": "[https://raw.githubusercontent.com/SeByDocKy/myESPhome/main/firmwares/pcm_bidirectionnal/firmware.bin](https://raw.githubusercontent.com/SeByDocKy/myESPhome/main/firmwares/pcm_bidirectionnal/firmware.bin)"
      }
    }
  ]
}