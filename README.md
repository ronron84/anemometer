Voici un **descriptif clé-en-main** pour votre dépôt GitHub, comprenant une **description du projet**, une **explication du code**, et des **instructions d'installation**. Vous pouvez copier-coller ce texte directement dans votre fichier `README.md`.

---

# 🌦 **Station Météo Anémomètre/Girouette avec SIM7670C**  
**Un système Arduino (RP2040) pour mesurer le vent (vitesse/direction), la température, et envoyer des données par SMS.**  

![Diagramme schématique du projet](https://via.placeholder.com/800x400.png?text=Station+Meteo+Diagram) *(Remplacez par un vrai schéma)*  

---

## 📌 **Fonctionnalités**  
- **Mesures en temps réel** :  
  - ✅ Vitesse du vent (anémomètre) avec détection de **rafales**.  
  - ✅ Direction du vent (girouette) en degrés et cardinale (N, NE, E, etc.).  
  - ✅ Température (capteur DS18B20).  
- **Communication GSM** :  
  - 📨 Envoi de données par SMS via module **SIM7670C**.  
  - 🔄 Réception de commandes (ex. : demande de relevé par SMS).  
- **Mémoire persistante** :  
  - 💾 Stockage des paramètres de calibration en EEPROM.  
- **Customisation facile** :  
  - 🛠 Coefficients et offsets ajustables via commandes série.  

---

## 📝 **Structure du Code**  
### **1. Capteurs**  
- **Anémomètre** (Broche `A0`) :  
  - Mesure analogique → conversion en km/h avec calibration (`anemometerCoeff`/`Offset`).  
  - Détection de rafales (**seuil = 1.5x la vitesse moyenne pendant 2 min**).  
- **Girouette** (Broche `A1`) :  
  - Conversion de la tension en angle (0°–360°) → cardinale (N, S, E, O).  
- **DS18B20** (Broche `2`) :  
  - Température avec moyenne glissante (10 mesures).  

### **2. Logique des Rafales**  
```cpp
// Paramètres :
#define GUST_FACTOR 1.5     // Seuil relatif (1.5x la moyenne)
#define MIN_GUST_DIFF 5.0   // Seuil absolu (5 km/h)
#define GUST_MIN_DURATION 120000  // Durée minimale (2 min)

// Détection :
if (currentSpeed > movingAvgSpeed * GUST_FACTOR && 
    (currentSpeed - movingAvgSpeed) >= MIN_GUST_DIFF) {
    // Rafale confirmée après 2 min
}
```

### **3. Communication GSM**  
- **SIM7670C** (Broches `RX=9`, `TX=8`) :  
  - Envoi de SMS avec les données météo via `AT+CMGS`.  
  - Réception de commandes (ex. : `"METEO0000"` pour demander un relevé).  

### **4. Calibration**  
- Coefficients stockés en **EEPROM** :  
  - `AC=1.5` (Coeff. anémomètre), `AO=0.0` (Offset).  
  - `GC=1.0` (Coeff. girouette), `GO=0.0` (Offset).  
- Commandes série pour ajustement :  
  ```bash
  LIST      # Affiche les paramètres actuels
  AC=1.5    # Modifie le coefficient anémomètre
  SAVE      # Enregistre en EEPROM
  ```

---

## ⚙️ **Installation**  
### **Matériel Requis**  
- Carte Arduino **RP2040** (ex. : Raspberry Pi Pico).  
- Capteurs :  
  - Anémomètre (**sortie analogique**).  
  - Girouette (**potentiomètre 360°**).  
  - DS18B20 (température).  
- Module GSM **SIM7670C** (avec antenne et carte SIM).  

### **Branchements**  
| Capteur       | Broche RP2040 |  
|---------------|---------------|  
| Anémomètre    | A0 (GP26)     |  
| Girouette     | A1 (GP27)     |  
| DS18B20       | 2 (GP2)       |  
| SIM7670C (RX) | 8 (GP8)       |  
| SIM7670C (TX) | 9 (GP9)       |  

### **Programmation**  
1. Installez le noyau **Arduino-Pico** (via Arduino IDE ou VS Code + PlatformIO).  
2. Téléversez le code.  
3. Ouvrez le **Moniteur Série** (`115200 baud`) pour calibrer.  

---

## 📲 **Exemple de SMS Envoyé**  
```plaintext
Station Meteo AMCPLV  
Vitesse (inst.): 12.5 km/h  
Vitesse (moy.): 10.2 km/h  
Dir: 225.0° (Sud-Ouest)  
Rafale max: 18.3 km/h  
Temp: 23.5°C  
```

---

## 🔧 **Personnalisation**  
- Pour adapter la **durée des rafales** (ex. : 10s au lieu de 2 min) :  
  ```cpp
  #define GUST_MIN_DURATION 10000  // 10 secondes
  ```
- Pour changer le **numéro de destination** :  
  ```cpp
  #define DEST_PHONE "+330000000"
  ```

---

## 📊 **Améliorations Possibles**  
- Ajouter un **écran LCD** pour un affichage local.  
- Intégrer **MQTT** pour envoi vers un serveur distant.  
- Logger les données sur **SD card**.  

---

🚀 **Contributions bienvenues !** *(License MIT)*  

--- 

Ce `README.md` est **prêt à l'emploi** – il ne vous reste qu’à :  
1. Ajouter des **photos/montages** du projet.  
2. Remplacer le lien vers le schéma.  
3. Personnaliser les détails spécifiques (ex. : numéro de téléphone par défaut).
![20250331_072737](https://github.com/user-attachments/assets/b5372ae1-a045-4c39-9f45-f9389016bd85)

Voici la section complète à ajouter dans votre `README.md` pour détailler **toutes les commandes disponibles** via le terminal série ou SMS :

---

## 📡 **Commandes Disponibles**

### **1. Commandes Série (Monitor Arduino)**
Connectez-vous au port série (`115200 baud`) et envoyez ces commandes :

| Commande          | Description                                                                 | Exemple                          |
|-------------------|-----------------------------------------------------------------------------|----------------------------------|
| `?`               | Affiche l'aide avec toutes les commandes                                    | `?`                              |
| `LIST`            | Liste les valeurs actuelles (vitesse, direction, calibration, etc.)        | `LIST`                           |
| `AC=<val>`        | Modifie le **coefficient anémomètre** (par défaut: `1.0`)                  | `AC=1.2` → 1.2 x la vitesse brute|
| `AO=<val>`        | Modifie l'**offset anémomètre** (en km/h, par défaut: `0.0`)               | `AO=0.5` → +0.5 km/h             |
| `GC=<val>`        | Modifie le **coefficient girouette** (par défaut: `1.0`)                   | `GC=1.1` → ajuste la direction   |
| `GO=<val>`        | Modifie l'**offset girouette** (en degrés, par défaut: `0.0`)              | `GO=90` → décale de 90°          |
| `SAVE`            | Sauvegarde la configuration actuelle en **EEPROM**                          | `SAVE`                           |
| `SIMNUM`          | Affiche le numéro de téléphone de la carte SIM                             | `SIMNUM`                         |
| `SENDSMS`         | Envoie un SMS de test au numéro par défaut (`DEST_PHONE`)                  | `SENDSMS`                        |
| `RESET`           | Redémarre la carte (soft reboot)                                           | `RESET`                          |

---

### **2. Commandes par SMS**
Envoyez un SMS au numéro de la carte SIM avec ces mots-clés :

| SMS Reçu          | Réponse du Système                                                                 |
|-------------------|-----------------------------------------------------------------------------------|
| `METEO0000`       | Renvoie un SMS avec **les données actuelles** (vitesse, direction, température).  |
| `meteo999`        | **Redémarre** la station (uniquement si envoyé par le numéro autorisé).          |

---

### **Exemple d'Utilisation des Commandes Série**
```bash
# Afficher la configuration actuelle
> LIST
Anémomètre - Coeff: 1.0, Offset: 0.0 km/h
Girouette - Coeff: 1.0, Offset: 0.0°
Température: 23.5°C

# Modifier le coefficient anémomètre
> AC=1.5
Nouveau coefficient anémomètre: 1.5

# Sauvegarder en EEPROM
> SAVE
Configuration sauvegardée.
```

---

### **3. Calibration Avancée**
Pour calibrer précisément les capteurs :
1. **Anémomètre** :  
   - Placez-le dans un vent **constant** (ex. : 10 km/h mesuré avec un anémomètre de référence).  
   - Ajustez `AC` et `AO` jusqu’à ce que la valeur affichée corresponde.  
   ```bash
   > AC=1.2
   > AO=0.3
   > SAVE
   ```

2. **Girouette** :  
   - Alignez-la vers le **Nord géographique**.  
   - Si la direction affichée est incorrecte (ex. : 350° au lieu de 0°), utilisez `GO` pour corriger :  
   ```bash
   > GO=10  # Décale de +10°
   > SAVE
   ```

---

### **Erreurs Courantes**
- `ERROR: SIM NOT CONNECTED` → Vérifiez l’antenne et la carte SIM.  
- `CAPTEUR DS18B20 NON DETECTE` → Vérifiez le branchement (broche `2`).  
- `CALIBRATION INVALIDE` → Les coefficients doivent être `> 0`.

---
### **"Fonctionnalités"**.  
📡 Commandes Disponibles
1. Commandes Série (Monitor Arduino)
Connectez-vous au port série (115200 baud) et envoyez ces commandes :

Commande	Description	Exemple
?	Affiche l'aide avec toutes les commandes	?
LIST	Liste les valeurs actuelles (vitesse, direction, calibration, etc.)	LIST
AC=<val>	Modifie le coefficient anémomètre (par défaut: 1.0)	AC=1.2 → 1.2 x la vitesse brute
AO=<val>	Modifie l'offset anémomètre (en km/h, par défaut: 0.0)	AO=0.5 → +0.5 km/h
GC=<val>	Modifie le coefficient girouette (par défaut: 1.0)	GC=1.1 → ajuste la direction
GO=<val>	Modifie l'offset girouette (en degrés, par défaut: 0.0)	GO=90 → décale de 90°
SAVE	Sauvegarde la configuration actuelle en EEPROM	SAVE
SIMNUM	Affiche le numéro de téléphone de la carte SIM	SIMNUM
SENDSMS	Envoie un SMS de test au numéro par défaut (DEST_PHONE)	SENDSMS
RESET	Redémarre la carte (soft reboot)	RESET
2. Commandes par SMS
Envoyez un SMS au numéro de la carte SIM avec ces mots-clés :

SMS Reçu	Réponse du Système
METEO0000	Renvoie un SMS avec les données actuelles (vitesse, direction, température).
meteo999	Redémarre la station (uniquement si envoyé par le numéro autorisé).
Exemple d'Utilisation des Commandes Série
bash
# Afficher la configuration actuelle
> LIST
Anémomètre - Coeff: 1.0, Offset: 0.0 km/h
Girouette - Coeff: 1.0, Offset: 0.0°
Température: 23.5°C

# Modifier le coefficient anémomètre
> AC=1.5
Nouveau coefficient anémomètre: 1.5

# Sauvegarder en EEPROM
> SAVE
Configuration sauvegardée.
3. Calibration Avancée
Pour calibrer précisément les capteurs :

Anémomètre :

Placez-le dans un vent constant (ex. : 10 km/h mesuré avec un anémomètre de référence).

Ajustez AC et AO jusqu’à ce que la valeur affichée corresponde.

bash
> AC=1.2
> AO=0.3
> SAVE
Girouette :

Alignez-la vers le Nord géographique.

Si la direction affichée est incorrecte (ex. : 350° au lieu de 0°), utilisez GO pour corriger :

bash
> GO=10  # Décale de +10°
> SAVE
Erreurs Courantes
ERROR: SIM NOT CONNECTED → Vérifiez l’antenne et la carte SIM.

CAPTEUR DS18B20 NON DETECTE → Vérifiez le branchement (broche 2).

CALIBRATION INVALIDE → Les coefficients doivent être > 0.

Cette section peut être intégrée directement dans votre README.md sous la partie "Fonctionnalités".
Je peux aussi vous fournir une version Markdown optimisée si besoin !

New chat
Message DeepSeek
