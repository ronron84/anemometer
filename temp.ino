#include <EEPROM.h>
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#define DS18B20_PIN 2          // Broche du capteur (modifiable)
OneWire oneWire(DS18B20_PIN);
DallasTemperature tempSensor(&oneWire);
float tempBuffer[10] = {0};    // Buffer pour moyenne glissante (10 mesures)
uint8_t tempIndex = 0;
float currentTemp = 0.0;
// Configuration des broches
constexpr int ANEMOMETER_PIN = A0;  // GP26
constexpr int WIND_VANE_PIN = A1;   // GP27

// Communication SIM - Utilisation de Serial2 avec pins 8 et 9
#define SIM_RX_PIN 9  // GP9 (RX du module SIM)
#define SIM_TX_PIN 8  // GP8 (TX du module SIM)
#define DEST_PHONE "+33652054971"
constexpr unsigned long SIM_BAUDRATE = 115200;
// Configuration des mesures
constexpr uint16_t WIND_SPEED_MIN = 0;
constexpr uint16_t WIND_SPEED_MAX = 500;
constexpr uint16_t WIND_DIR_MIN = 0;
constexpr uint16_t WIND_DIR_MAX = 360;
//Configuration des rafales
constexpr float GUST_THRESHOLD_KNOTS = 10.0; // 10 noeuds
constexpr float GUST_THRESHOLD_KMH = 18.5;   // 18.5 km/h
constexpr unsigned long GUST_MIN_DURATION = 120000; // 2 minutes en ms
constexpr unsigned long GUST_WINDOW = 600000; // Fenêtre de 10 minutes
// Coefficients et calibration
uint16_t anemometerCoeff = 10;
uint16_t anemometerOffset = 0;
uint16_t windVaneCoeff = 10;
uint16_t windVaneOffset = 0;
///modif
// Ajoutez ces variables près de gustCount et maxGustSpeed
float filteredSpeed = 0.0;       // Vitesse filtrée (lissée)
uint8_t gustDuration = 0;        // Durée de la rafale en cours (en mesures)
const float FILTER_COEFF = 0.4;  // Coefficient du filtre (20% de la nouvelle mesure)
// EEPROM
constexpr int ANEMOMETER_COEFF_EEPROM_ADDR = 0;
constexpr int ANEMOMETER_OFFSET_EEPROM_ADDR = sizeof(uint16_t);
constexpr int WIND_VANE_COEFF_EEPROM_ADDR = 2 * sizeof(uint16_t);
constexpr int WIND_VANE_OFFSET_EEPROM_ADDR = 3 * sizeof(uint16_t);
//constexpr int HOURLY_AVERAGES_EEPROM_ADDR = 4 * sizeof(uint16_t);
// Variables pour le suivi des rafales
struct GustData {
    float maxSpeed;
    float avgSpeed;
    bool isGust;
};
GustData currentGust;
unsigned long gustStartTime = 0;
// Structure de données
struct Measurement {
    uint16_t value;
    char dateTime[20];
};

// Variables globales
static float speedBuffer[30];      // Stocke 30 mesures (10 min si 20s/mesure)
Measurement windSpeedMemory[6];
Measurement windDirectionMemory[3];
uint8_t windSpeedIndex = 0;
uint8_t windDirectionIndex = 0;
float hourlyGusts[3]; // Stocke les rafales max des 3 dernières heures
// Timing et mesures
constexpr int MEASURE_INTERVAL_2S = 2000;
uint32_t windSpeedSum = 0;
uint16_t windSpeedCount = 0;
unsigned long last2sMeasure = 0;

constexpr unsigned long MEASURE_INTERVAL_1M = 60000;
uint16_t windSpeed1Min[15];
uint8_t windSpeed1MinIndex = 0;
unsigned long last1mMeasure = 0;

constexpr int MEASURE_INTERVAL_1H = 3600000;
float windSpeed1H[30];
uint8_t windSpeed1HIndex = 0;
unsigned long last1hMeasure = 0;
unsigned long cycleStartTime = 0;
bool firstHourCycle = true;

// Gestion des rafales
uint16_t gustCount = 0;
uint16_t maxGustSpeed = 0;
constexpr uint16_t GUST_THRESHOLD = 50;
uint16_t previousSpeed = 0;

// Tendance horaire
float hourlyAverages[3]; // Stocke les 3 dernières moyennes horaires
uint8_t hourlyIndex = 0;
bool trendInitialized = false;
String currentTrend = "DONNEE INSUFFISANTE";

constexpr float ADC_TO_VOLTAGE = 3.3f / 4095.0f; // RP2040 ADC 12-bit

// Gestion SIM
char senderNumber[20] = "";
bool simConnected = false;
unsigned long lastSIMCheck = 0;
constexpr unsigned long SIM_CHECK_INTERVAL = 10000;
//////////////////////////////////////////////////////////////////////////
// Buffer pour la réception SMS
constexpr int BUFFER_SIZE = 512;
char smsBuffer[BUFFER_SIZE];
int speedBufferIndex = 0;
int smsBufferIndex = 0; 
unsigned long lastCharTime = 0;
//////////////////////////////////////////////////////////////////////////
// Variable pour détecter si USB est connecté
bool usbConnected = false;

// Prototypes de fonctions
void initializeEEPROM();
void loadValuesFromEEPROM();
void saveValuesToEEPROM();
void measureWindSpeed();
void measureWindDirection();
void filterWindSpeed();
void calculateTrend();
void updateTrend(float currentAverage);
void processCommand(String command);
void sendDataViaSMS();
void handleSMS();
void resetArduino();
String getCardinalDirection(float degrees);
String getSIMPhoneNumber();
bool initializeSIM();
bool checkSIMConnection();
String sendATCommand(const String &cmd, unsigned long timeout);
//bool sendFreeSMS(const String &number, const String &msg);
void checkUSBConnection();
void receiveSMS();
void checkCompleteMessage();
void processBuffer();

String sendATCommand(const String& cmd, unsigned long timeout) {
    Serial2.println(cmd);
    String response;
    unsigned long start = millis();
    
    while (millis() - start < timeout) {
        while (Serial2.available()) {
            char c = Serial2.read();
            response += c;
        }
    }
    
    Serial.print(">> ");
    Serial.println(cmd);
    Serial.print("<< ");
    response.replace("\r", "\\r");
    response.replace("\n", "\\n");
    Serial.println(response);
    
    return response;

}
bool sendSMS(const char* number, const String& message) {
    Serial2.print("AT+CMGS=\"");
    Serial2.print(number);
    Serial2.println("\"");
    delay(100);
    
    Serial2.print(message);
    Serial2.write(0x1A);  // Ctrl+Z
    
    unsigned long start = millis();
    while (millis() - start < 10000) {
        if (Serial2.available()) {
            String resp = Serial2.readString();
            if (resp.indexOf("+CMGS:") != -1) return true;
            if (resp.indexOf("ERROR") != -1) break;
        }
    }
    return false;
  }

String getSIMPhoneNumber() {
    String response = sendATCommand("AT+CNUM", 2000);
    
    // Analyse de la réponse
    int startIndex = response.indexOf("+CNUM:");
    if (startIndex != -1) {
        int endIndex = response.indexOf("\n", startIndex);
        String numLine = response.substring(startIndex, endIndex);
        int numStart = numLine.indexOf("\"") + 1;
        int numEnd = numLine.indexOf("\"", numStart);
        if (numStart != -1 && numEnd != -1) {
            return numLine.substring(numStart, numEnd);
        }
    }
    
    return "Numéro non trouvé";
}

bool initializeSIM() {
    // Vérification de l'état du PIN
    String resp = sendATCommand("AT+CPIN?", 2000);
    
    // Si pas de PIN requis
    if (resp.indexOf("+CPIN: READY") != -1) {
        if (usbConnected) {
            Serial.println("[SIM] Pas de code PIN nécessaire");
        }
        return true;
    }
    // Si PIN requis
    else if (resp.indexOf("+CPIN: SIM PIN") != -1) {
        if (usbConnected) {
            Serial.println("[SIM] Code PIN requis, essai avec 0000");
        }
        
        // Essai avec 0000
        resp = sendATCommand("AT+CPIN=\"0000\"", 5000);
        
        if (resp.indexOf("OK") != -1) {
            if (usbConnected) {
                Serial.println("[SIM] Code PIN 0000 accepté");
            }
            return true;
        } else {
            // Essai avec 1234 si 0000 échoue
            if (usbConnected) {
                Serial.println("[SIM] Code PIN 0000 refusé, essai avec 1234");
            }
            resp = sendATCommand("AT+CPIN=\"1234\"", 5000);
            
            if (resp.indexOf("OK") != -1) {
                if (usbConnected) {
                    Serial.println("[SIM] Code PIN 1234 accepté");
                }
                return true;
            } else {
                if (usbConnected) {
                    Serial.println("[SIM] Échec de l'initialisation avec les codes PIN par défaut");
                }
                return false;
            }
        }
    }
    // Si carte bloquée (PUK nécessaire)
    else if (resp.indexOf("+CPIN: SIM PUK") != -1) {
        if (usbConnected) {
            Serial.println("[SIM] Carte bloquée (PUK nécessaire)");
        }
        return false;
    }
    // Autres cas
    else {
        if (usbConnected) {
            Serial.println("[SIM] État PIN inconnu");
        }
        return false;
    }
}

bool checkSIMConnection() {
   String resp = sendATCommand("AT", 1000);
    return resp.indexOf("OK") != -1;
}
void checkUSBConnection() {
    bool newStatus = (Serial);
    if (newStatus != usbConnected) {
        usbConnected = newStatus;
        if (usbConnected) {
            delay(1000);
            Serial.println("\nStation météo RP2040 initialisée");
            Serial.println("-----------------------------");
            Serial.print("Anémomètre - Coeff: ");
            Serial.print(anemometerCoeff / 10.0f, 1);
            Serial.print(", Offset: ");
            Serial.println(anemometerOffset / 10.0f, 1);
            Serial.print("Girouette - Coeff: ");
            Serial.print(windVaneCoeff / 10.0f, 1);
            Serial.print(", Offset: ");
            Serial.println(windVaneOffset / 10.0f, 1);
            Serial.println("-----------------------------");
            Serial.println("Tapez '?' pour l'aide");
        }
    }
}

void setup() {
    Serial.begin(115200);
    
    // Configuration Serial2 pour RP2040
    Serial2.setRX(SIM_RX_PIN);
    Serial2.setTX(SIM_TX_PIN);
    Serial2.begin(SIM_BAUDRATE);
    tempSensor.begin();  // Initialisation du DS18B20

    // Configuration RP2040
    analogReadResolution(12);
    EEPROM.begin(512);
    
    // Initialisation des buffers et variables
     // Tous les buffers sont mis à zéro pour éviter les valeurs aléatoires
    memset(speedBuffer, 0, sizeof(speedBuffer));
    memset(smsBuffer, 0, sizeof(smsBuffer));
    memset(&currentGust, 0, sizeof(currentGust));
    // Initialisation explicite des tableaux
    for(int i=0; i<15; i++) windSpeed1Min[i] = 0;
    for(int i=0; i<30; i++) windSpeed1H[i] = 0;
    for(int i=0; i<3; i++) hourlyGusts[i] = 0;
    // Réinitialisation des index
    speedBufferIndex = 0;
    smsBufferIndex = 0;
    windSpeed1MinIndex = 0;
    windSpeed1HIndex = 0;
    
    // Initialisation
    initializeEEPROM();
    loadValuesFromEEPROM();
    
    cycleStartTime = millis();
    
     // Initialisation SIM
    if (initializeSIM()) {
        simConnected = checkSIMConnection();
        if (simConnected && usbConnected) {
            Serial.println("[SIM] Module connecté");
            Serial.print("[SIM] Numéro de la carte : ");
            Serial.println(getSIMPhoneNumber());
        }
    } else {
        simConnected = false;
        if (usbConnected) {
            Serial.println("[SIM] Échec de la connexion au module SIM");
        }
    }
// Configuration SIM7670C
    sendATCommand("ATE0", 1000);  // Désactive echo
    sendATCommand("AT+CMEE=2", 1000);  // Messages d'erreur détaillés
    sendATCommand("AT+CNMI=2,2,0,0", 1000);  // Notification immédiate des SMS
    
    Serial.println("Système prêt");
}

void loop() {
    unsigned long currentMillis = millis();
    checkUSBConnection();
    // Acquisition des données
    if (currentMillis - last2sMeasure >= MEASURE_INTERVAL_2S) {
        last2sMeasure = currentMillis;
        measureWindSpeed();
        measureWindDirection();
        // Mesure température DS18B20 (toutes les 2 secondes)
    tempSensor.requestTemperatures();
    float newTemp = tempSensor.getTempCByIndex(0);
    tempBuffer[tempIndex] = newTemp;
    tempIndex = (tempIndex + 1) % 10;
    
    // Calcul moyenne glissante (sur 10 mesures = 20 secondes)
    float sumTemp = 0.0;
    for (int i = 0; i < 10; i++) sumTemp += tempBuffer[i];
    currentTemp = sumTemp / 10.0;
    }

    // Filtrage des données
    if (currentMillis - last1mMeasure >= MEASURE_INTERVAL_1M) {
        last1mMeasure = currentMillis;
        filterWindSpeed();
    }

    // Calcul des tendances
    if (currentMillis - last1hMeasure >= MEASURE_INTERVAL_1H) {
        last1hMeasure = currentMillis;
        calculateTrend();
    }

    // Vérification du module SIM
    if (currentMillis - lastSIMCheck >= SIM_CHECK_INTERVAL) {
        lastSIMCheck = currentMillis;
        bool newStatus = checkSIMConnection();
        if (newStatus != simConnected) {
            simConnected = newStatus;
            if (usbConnected) {
                Serial.println(simConnected ? "[SIM] Module connecté" : "[SIM] Module non détecté");
                if (simConnected) {
                    Serial.print("[SIM] Numéro de la carte : ");
                    Serial.println(getSIMPhoneNumber());
                }
            }
        }
    }

    // Gestion des commandes série
    if (usbConnected && Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        processCommand(command);
    }

    
    receiveSMS();

}
////////////////////////////////////////////////////
void receiveSMS() {
    while (Serial2.available()) {
        char c = Serial2.read();
        
        // Gestion du buffer circulaire
        if (speedBufferIndex < BUFFER_SIZE - 1) {
            smsBuffer[speedBufferIndex++] = c;
        } else {
            speedBufferIndex = 0;  // Reset si buffer plein
        }
        
        lastCharTime = millis();
        
        // Vérifie si on a reçu un message complet
        checkCompleteMessage();
    }
    
    // Timeout si pas de caractères reçus depuis 500ms
    if (speedBufferIndex > 0 && (millis() - lastCharTime > 500)) {
        processBuffer();
        speedBufferIndex = 0;
    }
}
/////////////////////////////////////////////////////////////
void checkCompleteMessage() {
    // Vérifie la fin de message typique (OK\r\n ou ERROR\r\n)
    if (speedBufferIndex >= 5) {
        if (strstr(smsBuffer + speedBufferIndex - 5, "OK\r\n") || 
            strstr(smsBuffer + speedBufferIndex - 8, "ERROR\r\n")) {
            processBuffer();
            speedBufferIndex = 0;
        }
    }
}
void processBuffer() { 
    // Termine la chaîne
    smsBuffer[speedBufferIndex] = '\0';
    
    // Vérifie si c'est un SMS
    char* smsStart = strstr(smsBuffer, "+CMT:");
    if (smsStart) {
        // Extraction du numéro
        char* numStart = strchr(smsStart, '"') + 1;
        char* numEnd = strchr(numStart, '"');
        
        // Extraction du contenu du SMS (après le dernier \n)
        char* contentStart = strchr(smsStart, '\n'); // Premier '\n' après "+CMT:"
        if (contentStart) contentStart++;  // Skip the newline character
        
        if (numStart && numEnd && contentStart) {
            *numEnd = '\0';  // Termine le numéro

            // Log pour voir le contenu du message
            Serial.println("\nNouveau SMS reçu:");
            Serial.print("De: ");
            Serial.println(numStart);
            Serial.print("Contenu: ");
            Serial.println(contentStart);
            
            // Stocke le numéro de l'expéditeur
            strncpy(senderNumber, numStart, sizeof(senderNumber)-1);
            senderNumber[sizeof(senderNumber)-1] = '\0';
            
            // Vérification du mot-clé "Aaa"
            // Convertit le contenu en majuscules avant comparaison
char upperContent[BUFFER_SIZE];
for (int i = 0; contentStart[i] && i < BUFFER_SIZE-1; i++) {
    upperContent[i] = toupper(contentStart[i]);
}
upperContent[strlen(contentStart)] = '\0';

if (strstr(upperContent, "METEO0000")) {
                Serial.println("DEBUG: Mot clé 'Aaa' détecté ! Envoi de la météo...");

                // Préparation du message météo complet
                float direction = (analogRead(WIND_VANE_PIN) * ADC_TO_VOLTAGE / 3.3f) * 360.0f;
                direction = (direction * (windVaneCoeff / 10.0f)) + (windVaneOffset / 10.0f);
                while (direction < 0) direction += 360;
                while (direction >= 360) direction -= 360;

                // Conversion de la direction avec String(direction, 1) pour un seul chiffre après la virgule
                String directionStr = String(direction, 1); // Utilisation de String avec un seul chiffre après la virgule

                // Création du message
                String message = "Station Meteo AMCPLV\n";
                message += "----------------\n";
                message += "Vitesse (inst.): " + String(filteredSpeed / 10.0f, 1) + " km/h\n";  // Dernière valeur brute
                message += "Vitesse (moy.): " + String(windSpeed1Min[windSpeed1MinIndex] / 10.0f, 1) + " km/h\n";
                message += "Direction: " + String(direction, 1) + "° (" + getCardinalDirection(direction) + ")\n";
                message += "Rafale max: " + String(maxGustSpeed / 10.0f, 1) + " km/h\n";
                message += "Température: " + String(currentTemp, 1) + " °C\n";  // Moyenne glissante
                message += "Tendance: " + currentTrend + "\n";
                message += "----------------";

                if (simConnected) {
                    if (sendSMS(senderNumber, message)) {
                        Serial.println("SMS météo envoyé avec succès !");
                    } else {
                        Serial.println("Échec d'envoi du SMS météo !");
                    }
                } else {
                    Serial.println("[SIM] Module non connecté - Impossible d'envoyer SMS");
                }
            } else {
                Serial.println("DEBUG: Mot clé 'Aaa' non détecté.");
            }
            
            // Traitement de la commande de réinitialisation
            if (strstr(contentStart, "meteo999")) {
                Serial.println("Commande de réinitialisation reçue par SMS");
                resetArduino();
            }
        }
    }
}


/////////////////////////////////////////////


void initializeEEPROM() {
    if (EEPROM.read(ANEMOMETER_COEFF_EEPROM_ADDR) == 255) {
        EEPROM.put(ANEMOMETER_COEFF_EEPROM_ADDR, anemometerCoeff);
        EEPROM.put(ANEMOMETER_OFFSET_EEPROM_ADDR, anemometerOffset);
        EEPROM.put(WIND_VANE_COEFF_EEPROM_ADDR, windVaneCoeff);
        EEPROM.put(WIND_VANE_OFFSET_EEPROM_ADDR, windVaneOffset);
        
        // NE PAS sauvegarder hourlyAverages dans l'EEPROM
        if (!EEPROM.commit() && usbConnected) {
            Serial.println("Erreur écriture EEPROM");
        } else if (usbConnected) {
            Serial.println("EEPROM initialisée avec valeurs par défaut");
        }
    }
}
void loadValuesFromEEPROM() {
    EEPROM.get(ANEMOMETER_COEFF_EEPROM_ADDR, anemometerCoeff);
    EEPROM.get(ANEMOMETER_OFFSET_EEPROM_ADDR, anemometerOffset);
    EEPROM.get(WIND_VANE_COEFF_EEPROM_ADDR, windVaneCoeff);
    EEPROM.get(WIND_VANE_OFFSET_EEPROM_ADDR, windVaneOffset);
    // NE PAS charger hourlyAverages depuis l'EEPROM
}
void saveValuesToEEPROM() {
    EEPROM.put(ANEMOMETER_COEFF_EEPROM_ADDR, anemometerCoeff);
    EEPROM.put(ANEMOMETER_OFFSET_EEPROM_ADDR, anemometerOffset);
    EEPROM.put(WIND_VANE_COEFF_EEPROM_ADDR, windVaneCoeff);
    EEPROM.put(WIND_VANE_OFFSET_EEPROM_ADDR, windVaneOffset);
    // NE PAS sauvegarder hourlyAverages dans l'EEPROM
    if (!EEPROM.commit() && usbConnected) {
        Serial.println("Erreur écriture EEPROM");
    } else if (usbConnected) {
        Serial.println("Configuration sauvegardée en EEPROM");
    }
}
void measureWindSpeed() {
    
    // --- Nouveaux buffers pour le calcul des rafales ---
   
    static unsigned long gustStartTime = 0;  // Timestamp du début de rafale
    static bool isGustActive = false;
    static float currentMaxGust = 0;
    // Lecture de la valeur brute du capteur
    uint16_t sensorValue = analogRead(ANEMOMETER_PIN);
    if (sensorValue < 10) sensorValue = 0;  // Seuil minimal pour éliminer le bruit

    // Conversion en tension
    float voltage = sensorValue * ADC_TO_VOLTAGE;
    // Nouvelle formule de conversion 4-20mA → 0-3.3V → Vitesse
    float speedMs = (voltage / 3.3f) * 32.5f; // Version simple
    // Conversion tension -> vitesse (m/s)
    //float speedMs = ((voltage - 0.4f) / 1.6f) * 32.4f;
    if (speedMs < 0) speedMs = 0;

    // Application du coefficient et offset (modification importante ici)
    float adjustedSpeed = speedMs * (anemometerCoeff / 10.0f) + (anemometerOffset / 10.0f);
    uint16_t speedKmh = static_cast<uint16_t>(adjustedSpeed * 3.6f * 10); // x10 pour précision décimale
    // Mise à jour du buffer (10 minutes de données)
    speedBuffer[speedBufferIndex] = speedKmh / 10.0f;  // Convertit en float (km/h)
    speedBufferIndex = (speedBufferIndex + 1) % 30;         // Buffer circulaire
    // Calcul de la moyenne sur 10 minutes
    float avg10min = 0;
    for (int i = 0; i < 30; i++) avg10min += speedBuffer[i];
    avg10min /= 30;
    // Détection du dépassement du seuil (10 nœuds = 18.5 km/h)
    if ((speedKmh / 10.0f) > (avg10min + 18.5f)) {
        if (!isGustActive) {
            gustStartTime = millis();  // Début de la rafale
            currentMaxGust = speedKmh / 10.0f;
            isGustActive = true;
        } else {
            // Mise à jour du max si la rafale continue
            currentMaxGust = max(currentMaxGust, speedKmh / 10.0f);
        }
    } else {
        // Réinitialisation si le seuil n'est plus atteint
        isGustActive = false;
    }

    // Validation de la rafale après 2 minutes
    if (isGustActive && (millis() - gustStartTime >= 120000)) {
        maxGustSpeed = currentMaxGust * 10;  // Stocke en x10 (compatible avec votre code)
        gustCount++;
    }
    // Debug optionnel (à activer/désactiver selon besoin)
    if (usbConnected && false) {  // Mettre "true" pour activer le debug
        Serial.print("[DEBUG] Raw=");
        Serial.print(sensorValue);
        Serial.print(" Voltage=");
        Serial.print(voltage, 3);
        Serial.print("V SpeedMs=");
        Serial.print(speedMs, 2);
        Serial.print("m/s Coeff=");
        Serial.print(anemometerCoeff / 10.0f, 2);
        Serial.print(" Offset=");
        Serial.print(anemometerOffset / 10.0f, 2);
        Serial.print(" AdjSpeed=");
        Serial.print(adjustedSpeed, 2);
        Serial.print("m/s Final=");
        Serial.print(speedKmh / 10.0f, 1);
        Serial.println("km/h");
    }

    // 1. Filtrage de la vitesse
    filteredSpeed = (1.0 - FILTER_COEFF) * filteredSpeed + FILTER_COEFF * speedKmh;

    // 2. Mise à jour des données pour les moyennes
    windSpeedSum += speedKmh;
    windSpeedCount++;
    windSpeed1Min[windSpeed1MinIndex] = speedKmh;
    
    // 3. Calcul de la moyenne sur 1 minute
    float avg1Min = 0.0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < 15; i++) {
        if (windSpeed1Min[i] > 0) {
            avg1Min += windSpeed1Min[i];
            count++;
        }
    }
    if (count > 0) avg1Min /= count;

    // 4. Détection de rafale avec seuil dynamique modifié
    float dynamicThreshold = max(2.0 * 10, avg1Min * 0.20); // Seuil à 2 km/h ou 20% de la moyenne
    
    if (filteredSpeed - avg1Min > dynamicThreshold) {
        gustDuration++;
        // Enregistrer immédiatement la vitesse maximale
        if (speedKmh > maxGustSpeed) {
            maxGustSpeed = speedKmh;
        }
        // Valider comme rafale après 1 mesure (2 secondes)
        if (gustDuration >= 1) {
            gustCount++;
        }
    } else {
        gustDuration = 0;
    }

    // 5. Mise à jour de l'index pour le buffer circulaire
    windSpeed1MinIndex = (windSpeed1MinIndex + 1) % 15;

    // Affichage normal (si USB connecté)
    if (usbConnected) {
        Serial.print("Vitesse: ");
        Serial.print(speedKmh / 10.0f, 1);
        Serial.print(" km/h | Filtrée: ");
        Serial.print(filteredSpeed / 10.0f, 1);
        Serial.print(" km/h | Moyenne: ");
        Serial.print(avg1Min / 10.0f, 1);
        Serial.print(" km/h | Rafale max: ");
        Serial.print(maxGustSpeed / 10.0f, 1);
        Serial.println(" km/h");
    }
}
void measureWindDirection() {
    uint16_t sensorValue = analogRead(WIND_VANE_PIN);
    float voltage = sensorValue * ADC_TO_VOLTAGE;
    float direction = (voltage / 3.3f) * 360.0f;
    // Inversion pour corriger Est/Ouest
    direction = 360.0f - direction;
    direction = (direction * (windVaneCoeff / 10.0f)) + (windVaneOffset / 10.0f);
    
    // Application calibration
    direction = (direction * (windVaneCoeff / 10.0f)) + (windVaneOffset / 10.0f);
    
    // Normalisation entre 0° et 360°
    while (direction < 0) direction += 360;
    while (direction >= 360) direction -= 360;
    if (usbConnected) {
        Serial.print("Direction vent: ");
        Serial.print(direction, 1);
        Serial.print("° (");
        Serial.print(getCardinalDirection(direction));
        Serial.println(")");
    }
}

String getCardinalDirection(float degrees) {
    if (degrees >= 337.5 || degrees < 22.5) return "Nord";
    if (degrees >= 22.5 && degrees < 67.5) return "Nord-Est";
    if (degrees >= 67.5 && degrees < 112.5) return "Est";
    if (degrees >= 112.5 && degrees < 157.5) return "Sud-Est";
    if (degrees >= 157.5 && degrees < 202.5) return "Sud";
    if (degrees >= 202.5 && degrees < 247.5) return "Sud-Ouest";
    if (degrees >= 247.5 && degrees < 292.5) return "Ouest";
    if (degrees >= 292.5 && degrees < 337.5) return "Nord-Ouest";
    return "Inconnu";
}

void filterWindSpeed() {
    float average2s = windSpeedSum / static_cast<float>(windSpeedCount);
    windSpeedSum = 0;
    windSpeedCount = 0;

    windSpeed1Min[windSpeed1MinIndex] = static_cast<uint16_t>(average2s);
    windSpeed1MinIndex = (windSpeed1MinIndex + 1) % 15;

    static bool isTableFull = false;
    if (!isTableFull && windSpeed1MinIndex == 0) isTableFull = true;

    if (isTableFull && usbConnected) {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < 15; i++) sum += windSpeed1Min[i];
        float average1Min = sum / 15.0f;

        Serial.print("Moyenne 1 minute: ");
        Serial.print(average1Min / 10.0f, 1);
        Serial.println(" km/h");
    }
}

void calculateTrend() {
    unsigned long elapsedTime = millis() - cycleStartTime;

    if (firstHourCycle && elapsedTime < MEASURE_INTERVAL_1H) {
        float prorataFactor = (float)elapsedTime / MEASURE_INTERVAL_1H;
        float sum = 0;
        uint8_t valid = 0;
        
        for (uint8_t i = 0; i < windSpeed1HIndex; i++) {
            if (windSpeed1H[i] > 0) { sum += windSpeed1H[i]; valid++; }
        }
        
        if (valid > 0 && usbConnected) {
            float currentAverage = (sum / valid) * prorataFactor;
            Serial.print("Moyenne horaire (prorata): ");
            Serial.print(currentAverage / 10.0f, 1);
            Serial.println(" km/h");
        }
        return;
    }

    float sum = 0;
    uint8_t valid = 0;
    for (uint8_t i = 0; i < 30; i++) {
        if (windSpeed1H[i] > 0) { sum += windSpeed1H[i]; valid++; }
    }

    if (valid > 0) {
        float currentAverage = sum / valid;
        
        if (usbConnected) {
            Serial.print("Moyenne horaire complète: ");
            Serial.print(currentAverage / 10.0f, 1);
            Serial.println(" km/h");
        }

        updateTrend(currentAverage);
        
        for (uint8_t i = 0; i < 29; i++) windSpeed1H[i] = windSpeed1H[i+1];
        windSpeed1H[29] = 0;
        windSpeed1HIndex = 29;
        firstHourCycle = false;
    }

    // NOUVEAU CODE - GESTION DES RAFALES
    if (gustCount > 0) {
        hourlyGusts[hourlyIndex] = maxGustSpeed / 10.0f;
        if (usbConnected) {
            Serial.print("Rafale max cette heure: ");
            Serial.println(hourlyGusts[hourlyIndex], 1);
        }
    }
    // Sauvegarde de la rafale max avant réinitialisation
    if (maxGustSpeed > 0) {
        hourlyGusts[hourlyIndex] = maxGustSpeed / 10.0f;
        if (usbConnected) {
            Serial.print("Rafale max cette heure: ");
            Serial.println(hourlyGusts[hourlyIndex], 1);
        }
    }
    maxGustSpeed = 0;
    gustCount = 0;
    gustDuration = 0;

    cycleStartTime = millis();
    
    if (usbConnected && gustCount > 0) {
        Serial.print("Rafale détectée! Max: ");
        Serial.print(maxGustSpeed / 10.0f, 1);
        Serial.print(" km/h | Durée: ");
        Serial.print(gustDuration * 2);
        Serial.println("s");
    }    
}

void updateTrend(float currentAverage) {
    hourlyAverages[hourlyIndex] = currentAverage;
    hourlyIndex = (hourlyIndex + 1) % 3;
    
    if (!trendInitialized && hourlyIndex == 0) {
        trendInitialized = true;
    }
    
    if (trendInitialized) {
        float delta1 = hourlyAverages[(hourlyIndex + 1) % 3] - hourlyAverages[(hourlyIndex + 2) % 3];
        float delta2 = hourlyAverages[hourlyIndex] - hourlyAverages[(hourlyIndex + 1) % 3];
        float totalDelta = delta1 + delta2;
        
        if (abs(delta1) < 1.0 && abs(delta2) < 1.0) {
            currentTrend = "Stable";
        } else if ((delta1 > 0 && delta2 > 0) || (totalDelta > 2.0)) {
            currentTrend = "Augmentation";
        } else if ((delta1 < 0 && delta2 < 0) || (totalDelta < -2.0)) {
            currentTrend = "Diminution";
        } else {
            currentTrend = "Variable";
        }
        
        if (usbConnected) {
            Serial.print("Tendance horaire: ");
            Serial.println(currentTrend);
            Serial.print("Détails: ");
            for (int i = 0; i < 3; i++) {
                Serial.print(hourlyAverages[i] / 10.0f, 1);
                Serial.print(" ");
            }
            Serial.println();
        }
        
        /*static int saveCount = 0;
        if (++saveCount >= 3) {
            saveCount = 0;
            saveValuesToEEPROM();
        }*/
    }
}

void processCommand(String command) {
    if (!usbConnected) return;
    
    if (command == "?") {
        Serial.println("Commandes disponibles:");
        Serial.println("?       - Affiche cette aide");
        Serial.println("LIST    - Affiche les valeurs actuelles");
        Serial.println("AC=<val> - Coefficient anémomètre (ex: AC=1.5)");
        Serial.println("AO=<val> - Offset anémomètre (ex: AO=0.2)");
        Serial.println("GC=<val> - Coefficient girouette (ex: GC=1.0)");
        Serial.println("GO=<val> - Offset girouette (ex: GO=5.0)");
        Serial.println("SAVE    - Sauvegarde la configuration");
        Serial.println("SIMNUM  - Affiche le numéro de la carte SIM");
        Serial.println("SENDSMS - Envoie un SMS de test");
        Serial.println("TREND   - Affiche la tendance actuelle");
    }
    else if (command == "LIST") {
        Serial.println("Configuration actuelle:");
        Serial.print("Anémomètre - Coeff: ");
        Serial.print(anemometerCoeff / 10.0f, 1);
        Serial.print(", Offset: ");
        Serial.println(anemometerOffset / 10.0f, 1);
        Serial.print("Girouette - Coeff: ");
        Serial.print(windVaneCoeff / 10.0f, 1);
        Serial.print(", Offset: ");
        Serial.println(windVaneOffset / 10.0f, 1);
        Serial.print("Tendance actuelle: ");
        Serial.println(currentTrend);
    }
    else if (command.startsWith("AC=")) {
        anemometerCoeff = command.substring(3).toFloat() * 10;
        Serial.print("Nouveau coefficient anémomètre: ");
        Serial.println(anemometerCoeff / 10.0f, 1);
    }
    else if (command.startsWith("AO=")) {
        anemometerOffset = command.substring(3).toFloat() * 10;
        Serial.print("Nouvel offset anémomètre: ");
        Serial.println(anemometerOffset / 10.0f, 1);
    }
    else if (command.startsWith("GC=")) {
        windVaneCoeff = command.substring(3).toFloat() * 10;
        Serial.print("Nouveau coefficient girouette: ");
        Serial.println(windVaneCoeff / 10.0f, 1);
    }
    else if (command.startsWith("GO=")) {
        windVaneOffset = command.substring(3).toFloat() * 10;
        Serial.print("Nouvel offset girouette: ");
        Serial.println(windVaneOffset / 10.0f, 1);
    }
    else if (command == "SAVE") {
        saveValuesToEEPROM();
    }
    else if (command == "SIMNUM") {
        if (simConnected) {
            Serial.print("[SIM] Numéro de la carte : ");
            Serial.println(getSIMPhoneNumber());
        } else {
            Serial.println("[SIM] Module non connecté");
        }
    }
    else if (command == "SENDSMS") {
        if (simConnected) {
            float direction = (analogRead(WIND_VANE_PIN) * ADC_TO_VOLTAGE / 3.3f) * 360.0f;
            direction = (direction * (windVaneCoeff / 10.0f)) + (windVaneOffset / 10.0f);
            while (direction < 0) direction += 360;
            while (direction >= 360) direction -= 360;

            String message = "Station Meteo RP2040\n";
            message += "----------------\n";
            message += "Vitesse: " + String(windSpeed1Min[windSpeed1MinIndex] / 10.0f, 1) + " km/h\n";
            message += "Direction: " + String(direction, 1) + "° (" + getCardinalDirection(direction) + ")\n";
            message += "Rafale max: " + String(maxGustSpeed / 10.0f, 1) + " km/h\n";
            message += "Tendance: " + currentTrend + "\n";
            message += "----------------";

            if (sendSMS(DEST_PHONE, message)) {
                Serial.println("SMS envoyé avec succès");
            } else {
                Serial.println("Échec d'envoi du SMS");
            }
        } else {
            Serial.println("[SIM] Module non connecté");
        }
    }
    else if (command == "TREND") {
        Serial.print("Tendance horaire actuelle: ");
        Serial.println(currentTrend);
        if (trendInitialized) {
            Serial.println("Dernières moyennes horaires:");
            for (int i = 0; i < 3; i++) {
                Serial.print("Heure -");
                Serial.print(3 - i);
                Serial.print(": ");
                Serial.print(hourlyAverages[(hourlyIndex + i) % 3] / 10.0f, 1);
                Serial.println(" km/h");
            }
        } else {
            Serial.println("Données insuffisantes (attendre 3 heures)");
        }
    }
    else {
        Serial.println("Commande non reconnue. Tapez '?' pour l'aide.");
    }
}

void sendDataViaSMS() {
    // 1. Calcul de la direction (code existant)
    float direction = (analogRead(WIND_VANE_PIN) * ADC_TO_VOLTAGE / 3.3f) * 360.0f;
    direction = (direction * (windVaneCoeff / 10.0f)) + (windVaneOffset / 10.0f);
    while (direction < 0) direction += 360;
    while (direction >= 360) direction -= 360;

    // 2. Calcul de la moyenne sur 10 minutes (pour les rafales)
    float avg10min = 0;
    for (int i = 0; i < 30; i++) avg10min += speedBuffer[i]; // speedBuffer défini dans measureWindSpeed()
    avg10min /= 30;

    // 3. Construction du message SMS
    String message = "Station Meteo AMCPLV\n";
    message += "----------------\n";
    message += "Vitesse actuelle: " + String(windSpeed1Min[windSpeed1MinIndex] / 10.0f, 1) + " km/h\n";
    message += "Direction: " + String(direction, 1) + "° (" + getCardinalDirection(direction) + ")\n";
    
    // 4. Section Rafale (seulement si détectée)
    if (maxGustSpeed > 0) {
        message += "----------------\n";
        message += "[RAFALE DETECTEE]\n";
        message += "Vitesse max: " + String(maxGustSpeed / 10.0f, 1) + " km/h\n";
        message += "Duree: 2 min\n"; 
        message += "Seuil depasse: " + String(avg10min + 18.5f, 1) + " km/h\n";
        message += "----------------\n";
    } else {
        message += "----------------\n";
        message += "Pas de rafale significative\n";
    }

    // 5. Envoi du SMS
    if (simConnected) {
        if (sendSMS(senderNumber, message)) {
            if (usbConnected) Serial.println("SMS envoyé :\n" + message);
        } else {
            if (usbConnected) Serial.println("Échec d'envoi du SMS");
        }
    }
}

void resetArduino() {
    if (usbConnected) {
        Serial.println("Réinitialisation du système...");
    }
    delay(1000);
    rp2040.reboot();
}