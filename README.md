# anemometer
 RP2040 Smart Wind Station with SMS Alerts & Remote Control

A sophisticated wind monitoring system based on Raspberry Pi Pico (RP2040) with GSM connectivity, designed for remote weather tracking and real-time alerts.

📊 Core Features
Anemometer (A0)

12-bit ADC precision (0-32.4 m/s range → 0-116.6 km/h)

Current-loop to voltage conversion (4-20mA → 0-3.3V)

Dynamic gust detection (GUST_THRESHOLD = 50 km/h)

Low-pass filtering (FILTER_COEFF = 0.2)

Wind Vane (A1)

360° direction measurement (0-3.3V analog)

Cardinal direction output (N, NE, E, etc.)

Configurable calibration (windVaneCoeff, windVaneOffset)

Data Processing

3 Time Intervals:

2s (raw), 1min (smoothed), 1h (trend analysis)

Trend Detection: "Stable", "Increasing", "Decreasing"

EEPROM Storage: Saves calibration (survives reboot)

SIM7670C GSM Module

SMS Commands:

METEO0000 → Sends full weather report (speed/direction/gusts/trend)

meteo999 → Remote system reboot

Auto-PIN Handling: Tries 0000/1234 if SIM is locked

Serial Debug: AT command logging (usbConnected flag)

# Installation
How to Flash the RP2040 with your Wind Station Code
Method 1: Using Pre-Compiled UF2 File (Quickest Method)
Enter Bootloader Mode:

Hold the BOOT button (BOOTSEL) while plugging in USB

Your computer will detect a drive named RPI-RP2

Copy the UF2 File:

Drag and drop final2.ino.uf2 to the RPI-RP2 drive

The RP2040 will automatically reboot and run your program

Method 2: Compiling from Arduino IDE (For Code Modifications)
Open final2.ino in Arduino IDE

Board Configuration:

Select: Tools → Board → Raspberry Pi Pico

(Ensure you've installed the RP2040 core)

Generate UF2:

Go to Sketch → Export Compiled Binary (Ctrl+Alt+S)

The UF2 file will be saved in:
.../final2/build/pico/final2.ino.uf2

Flash it (same as Method 1)
![20250331_072737](https://github.com/user-attachments/assets/b5372ae1-a045-4c39-9f45-f9389016bd85)

⚙️ Configuration Options
Command	Description	Example
AC=<val>	Set anemometer coefficient (x10)	AC=1.5 → 1.5
AO=<val>	Set anemometer offset (x10)	AO=0.2 → 0.2
GC=<val>	Set wind vane coefficient (x10)	GC=1.0 → 1.0
GO=<val>	Set wind vane offset (x10)	GO=5.0 → 5.0°
SAVE	Save config to EEPROM	-
SIMNUM	Show SIM card number	-
SENDSMS	Manual SMS to DEST_PHONE	-
TREND	Show 3-hour trend data	-
📡 SMS Report Example
Copy
Station Meteo AMCPLV  
----------------  
Vitesse: 12.5 km/h  
Direction: 45.0° (Nord-Est)  
Rafale max: 18.3 km/h  
Tendance: Augmentation  
----------------  
Description Française
🌬️ Station Anémométrique Intelligente RP2040 avec Alertes SMS

Un système complet de surveillance éolienne avec module GSM, conçu pour des mesures autonomes et des alertes à distance.

📊 Fonctionnalités Principales
Anémomètre (A0)

Mesure 12-bit (0-32.4 m/s → 0-116.6 km/h)

Conversion 4-20mA → 0-3.3V

Détection dynamique des rafales (GUST_THRESHOLD = 50 km/h)

Filtrage passe-bas (FILTER_COEFF = 0.2)

Girouette (A1)

Mesure analogique (0-360°)

Sortie en direction cardinale (N, NE, E...)

Calibration ajustable (windVaneCoeff, windVaneOffset)

Traitement des Données

3 Intervalles :

2s (brut), 1min (lissé), 1h (tendance)

Analyse de Tendance : "Stable", "Augmentation", "Diminution"

Stockage EEPROM : Sauvegarde la calibration

Module GSM SIM7670C

Commandes SMS :

METEO0000 → Envoie un rapport complet

meteo999 → Réinitialisation à distance

Gestion Auto du PIN : Teste 0000/1234 si bloqué

Debug Serie : Log des commandes AT
# Installation
📌 Méthode 1 : Flasher le fichier .uf2 (Recommandé pour une installation rapide)
Branchez le RP2040 en mode BOOTSEL :

Maintenez le bouton BOOT (BOOTSEL) enfoncé tout en branchant le câble USB.

Un lecteur appelé RPI-RP2 apparaîtra sur votre ordinateur.

Copiez le fichier .uf2 :

Glissez-déposez le fichier final2.ino.uf2 dans le lecteur RPI-RP2.

Le RP2040 redémarrera automatiquement et exécutera le programme.

⚙️ Méthode 2 : Compiler depuis l'IDE Arduino (Si vous modifiez le code)
Ouvrez final2.ino dans l'IDE Arduino.

Sélectionnez la carte :

Type de carte : Raspberry Pi Pico (Core RP2040 installé).

Port : Aucun (pour la compilation).

Compilez et exportez le .uf2 :

Allez dans Croquis → Exporter un binaire compilé (Ctrl+Alt+S sous Windows).

Le fichier final2.ino.uf2 sera généré dans le dossier du projet (.../build/pico/).

Transférez-le sur le RP2040 (comme en Méthode 1).
⚙️ Options de Configuration
Commande	Description	Exemple
AC=<val>	Coefficient anémomètre (x10)	AC=1.5 → 1.5
AO=<val>	Offset anémomètre (x10)	AO=0.2 → 0.2
GC=<val>	Coefficient girouette (x10)	GC=1.0 → 1.0
GO=<val>	Offset girouette (x10)	GO=5.0 → 5.0°
SAVE	Sauvegarde la config en EEPROM	-
SIMNUM	Affiche le numéro de la SIM	-
SENDSMS	Envoi manuel d’un SMS	-
TREND	Affiche les tendances sur 3h	-
📡 Exemple de Rapport SMS
Copy
Station Meteo AMCPLV  
----------------  
Vitesse: 12.5 km/h  
Direction: 45.0° (Nord-Est)  
Rafale max: 18.3 km/h  
Tendance: Augmentation  
----------------  
📌 Notes Techniques
Dépendances : EEPROM.h (pour le stockage), Arduino.h (RP2040 core)

Broches :

Anémomètre : A0 (GP26)

Girouette : A1 (GP27)

SIM7670C : RX=GP9, TX=GP8

Seuils :

Rafale : > 20% de la moyenne sur 1min

Tendance : ±2 km/h pour déclencher "Augmentation/Diminution"

Cette description met en valeur toutes les fonctionnalités tout en restant claire pour les utilisateurs. Vous pouvez ajouter une section "Installation" si nécessaire (ex: librairies à installer).

oui
Voici une section Installation/Usage complète à ajouter à votre description GitHub, en anglais et français :

🇬🇧 Installation & Setup
📦 Required Libraries
Arduino-Pico RP2040 Core (via Arduino Board Manager)

URL: https://github.com/earlephilhower/arduino-pico

EEPROM.h (Included with Arduino-Pico)

🔌 Hardware Connections
Component	RP2040 Pin	Notes
Anemometer	A0 (GP26)	4-20mA → 250Ω → 3.3V (0-32.4m/s)
Wind Vane	A1 (GP27)	0-3.3V analog (0-360°)
SIM7670C RX	GP9	3.3V logic level
SIM7670C TX	GP8	3.3V logic level
⚡ First-Time Setup
Calibration (via Serial Monitor at 115200 baud):

bash
Copy
# Set calibration values (example)
AC=1.5  # Anemometer coefficient (1.5 * raw value)
AO=0.0  # Anemometer offset
SAVE    # Save to EEPROM
SMS Keywords (send to SIM card number):

METEO0000 → Receive instant weather report

meteo999 → Emergency reboot

🇫🇷 Installation & Utilisation
📦 Bibliothèques Requises
Arduino-Pico RP2040 Core (via Arduino Board Manager)

URL: https://github.com/earlephilhower/arduino-pico

EEPROM.h (Inclus avec Arduino-Pico)

🔌 Branchements Matériels
Composant	Broche RP2040	Notes
Anémomètre	A0 (GP26)	4-20mA → 250Ω → 3.3V (0-32.4m/s)
Girouette	A1 (GP27)	0-3.3V analogique (0-360°)
SIM7670C RX	GP9	Niveau logique 3.3V
SIM7670C TX	GP8	Niveau logique 3.3V
⚡ Configuration Initiale
Calibration (via Moniteur Série à 115200 bauds) :

bash
Copy
# Exemple de calibration
AC=1.5  # Coefficient anémomètre (1.5 × valeur brute)
AO=0.0  # Offset anémomètre
SAVE    # Sauvegarde en EEPROM
Mots-Clés SMS (envoyés au numéro de la carte SIM) :

METEO0000 → Reçoit un rapport météo instantané

meteo999 → Redémarrage d'urgence

⚠️ Troubleshooting
Issue	Solution
SIM module not responding	Check SIM_BAUDRATE (default: 115200)
Wind speed = 0	Verify 4-20mA → 250Ω resistor circuit
EEPROM errors	Call initializeEEPROM() in setup()
🔧 Dépannage
Problème	Solution
Module SIM ne répond pas	Vérifier SIM_BAUDRATE (par défaut 115200)
Vitesse vent = 0	Contrôler le circuit 4-20mA → 250Ω
Erreurs EEPROM	Appeler initializeEEPROM() dans setup()
🌐 Sample Code Snippet
cpp
Copy
// Dynamic gust detection logic
if (filteredSpeed - avg1Min > dynamicThreshold * 10) {
  gustDuration++;
  if (gustDuration >= 2) { // Valid gust if >4s (2 measurements)
    gustCount++;
    if (speedKmh > maxGustSpeed) maxGustSpeed = speedKmh;
  }
}
anémometer and wind vane
// Dynamic gust detection logic
if (filteredSpeed - avg1Min > dynamicThreshold * 10) {
  gustDuration++;
  if (gustDuration >= 2) { // Valid gust if >4s (2 measurements)
    gustCount++;
    if (speedKmh > maxGustSpeed) maxGustSpeed = speedKmh;
  }
}
