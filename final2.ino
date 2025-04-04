#include <EEPROM.h>
#include <Arduino.h>

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

// Coefficients et calibration
uint16_t anemometerCoeff = 10;
uint16_t anemometerOffset = 0;
uint16_t windVaneCoeff = 10;
uint16_t windVaneOffset = 0;
///modif
// Ajoutez ces variables près de gustCount et maxGustSpeed
float filteredSpeed = 0.0;       // Vitesse filtrée (lissée)
uint8_t gustDuration = 0;        // Durée de la rafale en cours (en mesures)
const float FILTER_COEFF = 0.2;  // Coefficient du filtre (20% de la nouvelle mesure)
// EEPROM
constexpr int ANEMOMETER_COEFF_EEPROM_ADDR = 0;
constexpr int ANEMOMETER_OFFSET_EEPROM_ADDR = sizeof(uint16_t);
constexpr int WIND_VANE_COEFF_EEPROM_ADDR = 2 * sizeof(uint16_t);
constexpr int WIND_VANE_OFFSET_EEPROM_ADDR = 3 * sizeof(uint16_t);
//constexpr int HOURLY_AVERAGES_EEPROM_ADDR = 4 * sizeof(uint16_t);

// Structure de données
struct Measurement {
    uint16_t value;
    char dateTime[20];
};

// Variables globales
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

constexpr unsigned long MEASURE_INTERVAL_1M = 4000;
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
int bufferIndex = 0;
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
//////////////////////////il manque quelquechose/////////////////////////////////////
bool sendFreeSMS(const String &number, const String &msg) {
    String cmd = "AT+CMGS=\"" + number + "\"";
    String resp = sendATCommand(cmd, 2000);
    
    if (resp.indexOf(">") != -1) {
        Serial2.print(msg);
        Serial2.write(26); // Ctrl+Z
        delay(500);
        
        unsigned long start = millis();
        while (millis() - start < 20000) {
            if (Serial2.available()) {
                String resp = Serial2.readString();
                if (usbConnected) {
                    Serial.println(resp);
                }
                if (resp.indexOf("+CMGS:") != -1) return true;
                if (resp.indexOf("CMS ERROR") != -1) return false;
            }
        }
    }
    return false;
}



void setup() {
    Serial.begin(115200);
    
    // Configuration Serial2 pour RP2040
    Serial2.setRX(SIM_RX_PIN);
    Serial2.setTX(SIM_TX_PIN);
    Serial2.begin(SIM_BAUDRATE);
    
    // Configuration RP2040
    analogReadResolution(12);
    EEPROM.begin(512);
    
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

    /* Gestion des SMS entrants
    if (simConnected) {
        handleSMS();
    }*/

    // Petit délai pour économie d'énergie
    //delay(50);
    receiveSMS();

}
////////////////////////////////////////////////////
void receiveSMS() {
    while (Serial2.available()) {
        char c = Serial2.read();
        
        // Gestion du buffer circulaire
        if (bufferIndex < BUFFER_SIZE - 1) {
            smsBuffer[bufferIndex++] = c;
        } else {
            bufferIndex = 0;  // Reset si buffer plein
        }
        
        lastCharTime = millis();
        
        // Vérifie si on a reçu un message complet
        checkCompleteMessage();
    }
    
    // Timeout si pas de caractères reçus depuis 500ms
    if (bufferIndex > 0 && (millis() - lastCharTime > 500)) {
        processBuffer();
        bufferIndex = 0;
    }
}
/////////////////////////////////////////////////////////////
void checkCompleteMessage() {
    // Vérifie la fin de message typique (OK\r\n ou ERROR\r\n)
    if (bufferIndex >= 5) {
        if (strstr(smsBuffer + bufferIndex - 5, "OK\r\n") || 
            strstr(smsBuffer + bufferIndex - 8, "ERROR\r\n")) {
            processBuffer();
            bufferIndex = 0;
        }
    }
}
void processBuffer() { 
    // Termine la chaîne
    smsBuffer[bufferIndex] = '\0';
    
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
                message += "Vitesse: " + String(windSpeed1Min[windSpeed1MinIndex] / 10.0f, 1) + " km/h\n";
                message += "Direction: " + directionStr + "Deg (" + getCardinalDirection(direction) + ")\n";
                message += "Rafale max: " + String(maxGustSpeed / 10.0f, 1) + " km/h\n";
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
    // Déclaration UNIQUE de avg1Min
    float avg1Min = 0.0;
    
    uint16_t sensorValue = analogRead(ANEMOMETER_PIN);
    if (sensorValue < 10) sensorValue = 0;

    float voltage = sensorValue * ADC_TO_VOLTAGE;
    ///////////Current output type Measuring range：0~32. 4 m/s Power supply voltage：12V~24V DC output signal：4~20 mA Load capacity：≤200Ωformule de la documentation Value of wind speed=（output voltage-0.4）/1.6*32.4
    float speedMs = ((voltage - 0.4f) / 1.6f) * 32.4f;
    ///////////////////////////////////////////////////////
    if (speedMs < 0) speedMs = 0;

    uint16_t speedKmh = static_cast<uint16_t>(speedMs * 3.6f * 10);

    // 1. Filtrage de la vitesse
    filteredSpeed = (1.0 - FILTER_COEFF) * filteredSpeed + FILTER_COEFF * speedKmh;
// Mise à jour des données d'abord
    windSpeedSum += speedKmh;
    windSpeedCount++;
    windSpeed1Min[windSpeed1MinIndex] = speedKmh;
    windSpeed1MinIndex = (windSpeed1MinIndex + 1) % 15;

    // 2. Calcul de la moyenne APRÈS mise à jour
    for (uint8_t i = 0; i < 15; i++) {
        avg1Min += windSpeed1Min[i];
    }
    avg1Min /= 15.0;



// 2. Calcul de la moyenne sur 1 minute (simplifiée)
for (uint8_t i = 0; i < 15; i++) avg1Min += windSpeed1Min[i];
avg1Min /= 15.0;

// 3. Détection de rafale avec seuil dynamique
float dynamicThreshold = max(5.0, avg1Min * 0.2); // Seuil = 5 km/h ou 20% de la moyenne
if (filteredSpeed - avg1Min > dynamicThreshold * 10) { // *10 car speedKmh est en x10
    gustDuration++;
    if (gustDuration >= 2) { // Rafale valide si > 4 secondes (2 mesures à 2s d'intervalle)
        gustCount++;
        if (speedKmh > maxGustSpeed) maxGustSpeed = speedKmh;
    }
} else {
    gustDuration = 0; // Reset si la condition n'est plus remplie
}
/////////////////////////////////////////////////////
    // Mise à jour des données
    windSpeedSum += speedKmh;
    windSpeedCount++;
    windSpeed1Min[windSpeed1MinIndex] = speedKmh;
    windSpeed1MinIndex = (windSpeed1MinIndex + 1) % 15;
    /////////////////////////////////////////////////
for (uint8_t i = 0; i < 15; i++) avg1Min += windSpeed1Min[i];
avg1Min /= 15.0;
////////////////////////////////////////////////////////
    if (windSpeed1HIndex < 30) {
        windSpeed1H[windSpeed1HIndex++] = speedKmh;
    }

    if (usbConnected) {
        Serial.print("Vitesse vent: ");
        Serial.print(speedKmh / 10.0f, 1);
        Serial.println(" km/h");
    }
    if (usbConnected) {
    Serial.print("Vitesse filtrée: ");
    Serial.print(filteredSpeed / 10.0f, 1);
    Serial.print(" km/h | Moyenne 1min: ");
    Serial.print(avg1Min / 10.0f, 1);
    Serial.print(" km/h | Rafale: ");
    Serial.println((filteredSpeed - avg1Min) / 10.0f, 1);
}
}

void measureWindDirection() {
    uint16_t sensorValue = analogRead(WIND_VANE_PIN);
    float voltage = sensorValue * ADC_TO_VOLTAGE;
    float direction = (voltage / 3.3f) * 360.0f;
    
    direction = (direction * (windVaneCoeff / 10.0f)) + (windVaneOffset / 10.0f);
    
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

    // Nouveau code à insérer :
if (gustCount > 0) {
    hourlyGusts[hourlyIndex] = maxGustSpeed / 10.0f;  // Sauvegarde
    if (usbConnected) {
        Serial.print("Rafale max cette heure: ");
        Serial.println(hourlyGusts[hourlyIndex], 1);
    }
} else {
    maxGustSpeed = 0;
}
gustCount = 0;
gustDuration = 0;
    cycleStartTime = millis();
if (usbConnected && gustCount > 0) {
    Serial.print("Rafale détectée! Max: ");
    Serial.print(maxGustSpeed / 10.0f, 1);
    Serial.print(" km/h | Durée: ");
    Serial.print(gustDuration * 2);  // 2 secondes par mesure
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
        
        static int saveCount = 0;
        if (++saveCount >= 3) {
            saveCount = 0;
            saveValuesToEEPROM();
        }
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

    if (sendSMS(senderNumber, message)) {
        if (usbConnected) {
            Serial.println("SMS envoyé avec les données météo");
        }
    } else {
        if (usbConnected) {
            Serial.println("Échec d'envoi du SMS");
        }
    }
}

/*void handleSMS() {
    if (!Serial2.available()) return;
    
    String response = Serial2.readString();
    if (usbConnected) {
        Serial.println("[DEBUG SMS] Réponse brute:");
        Serial.println(response);
    }

    if (response.indexOf("123") != -1) {
        if (usbConnected) {
            Serial.println("Commande '123' détectée");
        }
        
        int start = response.indexOf("+CMT: \"") + 7;
        int end = response.indexOf("\"", start);
        
        if (start >= 7 && end > start) {
            String num = response.substring(start, end);
            num.trim();
            num.toCharArray(senderNumber, sizeof(senderNumber));
            
            if (usbConnected) {
                Serial.print("Numéro expéditeur extrait: ");
                Serial.println(senderNumber);
            }
            
            sendDataViaSMS();
        } else if (usbConnected) {
            Serial.println("Erreur: Impossible d'extraire le numéro");
        }
    }
    else if (response.indexOf("meteo999") != -1) {
        if (usbConnected) {
            Serial.println("Commande de réinitialisation reçue");
        }
        resetArduino();
    }
}
*/
void resetArduino() {
    if (usbConnected) {
        Serial.println("Réinitialisation du système...");
    }
    delay(1000);
    rp2040.reboot();
}