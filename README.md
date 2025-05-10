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

