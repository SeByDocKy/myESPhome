# Structure des dossiers - Composant RYLR998

Ce document explique comment organiser les fichiers du composant RYLR998.

## 📁 Structure recommandée

### Version Basic (sans packet_transport)

```
config/
└── custom_components/
    └── rylr998/
        ├── __init__.py
        ├── rylr998.h
        ├── rylr998.cpp
        └── automation.h
```

### Version Complete (avec packet_transport)

```
config/
└── custom_components/
    └── rylr998/
        ├── __init__.py
        ├── rylr998.h
        ├── rylr998.cpp
        ├── automation.h
        └── packet_transport/
            ├── __init__.py
            ├── rylr998_packet_transport.h
            ├── rylr998_packet_transport.cpp
            └── README.md
```

## 🔧 Installation étape par étape

### Option 1 : Installation Basic (Recommandé)

1. **Créer le dossier** :
   ```bash
   mkdir -p config/custom_components/rylr998
   ```

2. **Copier les fichiers** :
   ```
   rylr998/
   ├── __init__.py
   ├── rylr998.h
   ├── rylr998.cpp
   └── automation.h
   ```

3. **Compiler** :
   ```bash
   esphome run votre_config.yaml
   ```

### Option 2 : Installation Complete (Avancé)

**⚠️ ATTENTION** : Cette option nécessite que votre version d'ESPHome supporte `packet_transport`.

1. **Créer les dossiers** :
   ```bash
   mkdir -p config/custom_components/rylr998/packet_transport
   ```

2. **Copier les fichiers de base** dans `rylr998/` :
   ```
   rylr998/
   ├── __init__.py
   ├── rylr998.h
   ├── rylr998.cpp
   └── automation.h
   ```

3. **Copier les fichiers packet_transport** dans `rylr998/packet_transport/` :
   ```
   rylr998/packet_transport/
   ├── __init__.py
   ├── rylr998_packet_transport.h
   ├── rylr998_packet_transport.cpp
   └── README.md
   ```

4. **Compiler** :
   ```bash
   esphome run votre_config.yaml
   ```

## ❌ Que faire si ça ne compile pas ?

### Erreur : `packet_transport has no attribute 'PacketTransportComponent'`

**Cause** : Votre version d'ESPHome ne supporte pas packet_transport.

**Solution** : 
1. Supprimez le dossier `packet_transport/` :
   ```bash
   rm -rf config/custom_components/rylr998/packet_transport
   ```

2. Utilisez uniquement le composant de base

3. Consultez `EXAMPLES.md` pour créer votre propre protocole

### Erreur : Module not found

**Vérifiez** que vous avez la bonne structure :

```bash
# Depuis votre dossier config
tree custom_components/rylr998
```

Devrait afficher :
```
custom_components/rylr998
├── __init__.py
├── rylr998.h
├── rylr998.cpp
└── automation.h
```

## 📋 Checklist d'installation

### Version Basic
- [ ] Dossier `custom_components/rylr998/` créé
- [ ] Fichier `__init__.py` copié
- [ ] Fichier `rylr998.h` copié
- [ ] Fichier `rylr998.cpp` copié
- [ ] Fichier `automation.h` copié
- [ ] Configuration YAML créée
- [ ] Compilation réussie

### Version Complete
- [ ] Tous les points de la version Basic ✓
- [ ] Dossier `packet_transport/` créé
- [ ] Fichier `packet_transport/__init__.py` copié
- [ ] Fichier `packet_transport/rylr998_packet_transport.h` copié
- [ ] Fichier `packet_transport/rylr998_packet_transport.cpp` copié
- [ ] Configuration YAML avec packet_transport
- [ ] Compilation réussie (si échec → revenir à Basic)

## 🔄 Migration de l'ancienne structure

Si vous aviez les fichiers à la racine de `rylr998/` :

### Ancienne structure (ne fonctionne plus) :
```
rylr998/
├── __init__.py
├── rylr998.h
├── rylr998.cpp
├── automation.h
├── packet_transport.py              ❌ À DÉPLACER
├── rylr998_packet_transport.h       ❌ À DÉPLACER
└── rylr998_packet_transport.cpp     ❌ À DÉPLACER
```

### Nouvelle structure :
```
rylr998/
├── __init__.py
├── rylr998.h
├── rylr998.cpp
├── automation.h
└── packet_transport/
    ├── __init__.py                  ← packet_transport.py renommé
    ├── rylr998_packet_transport.h   ← déplacé ici
    └── rylr998_packet_transport.cpp ← déplacé ici
```

### Commandes de migration :
```bash
cd config/custom_components/rylr998
mkdir packet_transport
mv packet_transport.py packet_transport/__init__.py
mv rylr998_packet_transport.h packet_transport/
mv rylr998_packet_transport.cpp packet_transport/
```

## 🎯 Vérification finale

### Test de la structure :

```bash
# Depuis votre dossier config
ls -la custom_components/rylr998/
# Devrait montrer : __init__.py, rylr998.h, rylr998.cpp, automation.h
# Et éventuellement : packet_transport/ (si version complete)

# Si version complete :
ls -la custom_components/rylr998/packet_transport/
# Devrait montrer : __init__.py, rylr998_packet_transport.h, rylr998_packet_transport.cpp
```

### Test de compilation :

```bash
# Nettoyer le cache
esphome clean votre_config.yaml

# Compiler
esphome compile votre_config.yaml
```

Si la compilation réussit → ✅ Structure correcte !

Si erreur sur packet_transport → Supprimez le dossier `packet_transport/`

## 📚 Fichiers de documentation

Ces fichiers ne sont **pas nécessaires** pour la compilation, mais utiles pour référence :

```
rylr998/
├── README.md                 # Documentation principale (optionnel)
├── INSTALL.md               # Guide d'installation (optionnel)
├── EXAMPLES.md              # Exemples de code (optionnel)
└── packet_transport/
    └── README.md            # Infos sur packet_transport (optionnel)
```

Vous pouvez les conserver ailleurs ou les supprimer après lecture.

## ⚡ Démarrage rapide

**Méthode la plus simple** :

1. Téléchargez `rylr998_basic.tar.gz`
2. Extrayez dans `custom_components/rylr998/`
3. Utilisez `example_basic.yaml` comme modèle
4. Compilez !

**Si vous voulez packet_transport** (et que vous êtes sûr de votre version ESPHome) :

1. Téléchargez `rylr998_complete.tar.gz`
2. Extrayez dans `custom_components/rylr998/`
3. Compilez
4. Si erreur → Supprimez `packet_transport/` et recompilez

## 🆘 Besoin d'aide ?

- Structure incorrecte → Consultez ce document
- Erreur packet_transport → Supprimez le dossier `packet_transport/`
- Autres erreurs → Consultez `INSTALL.md`
