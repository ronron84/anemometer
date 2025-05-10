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
#define DEST_PHONE "+0000000000000000000"
constexpr unsigned long SIM_BAUDRATE = 115200;

// Configuration des mesures
constexpr uint16_t WIND_SPEED_MIN = 0;
constexpr uint16_t WIND_SPEED_MAX = 500;
constexpr uint16_t WIND_DIR_MIN = 0;
constexpr uint16_t WIND_DIR_MAX = 360;

// Nouveaux paramètres rafales
constexpr float GUST_FACTOR = 1.5; // Rafale = 1.5x la vitesse moyenne
constexpr float MIN_GUST_DIFF = 5.0; // Différence minimale absolue en km/h
constexpr unsigned long GUST_MIN_DURATION = 120000; // 2 minutes en ms
constexpr unsigned long GUST_RESET_INTERVAL = 10800000; // 3 heures en ms
// Coefficients et calibration
uint16_t anemometerCoeff = 10;
uint16_t anemometerOffset = 0;
uint16_t windVaneCoeff = 10;
uint16_t windVaneOffset = 0;

///modif
// Ajoutez ces variables près de gustCount et maxGustSpeed
float filteredSpeed = 0.0;       // Vitesse filtrée (lissée)
const float FILTER_COEFF = 0.4;  // Coefficient du filtre (20% de la nouvelle mesure)

// EEPROM
constexpr int ANEMOMETER_COEFF_EEPROM_ADDR = 0;
constexpr int ANEMOMETER_OFFSET_EEPROM_ADDR = sizeof(uint16_t);
constexpr int WIND_VANE_COEFF_EEPROM_ADDR = 2 * sizeof(uint16_t);
constexpr int WIND_VANE_OFFSET_EEPROM_ADDR = 3 * sizeof(uint16_t);
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
uint8_t speedBufferIndex = 0;  // Index pour le buffer de vitesse du vent
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

// Variables pour gestion des rafales
float maxGustSpeed = 0.0;
float movingAvgSpeed = 0.0;
unsigned long lastGustReset = 0;
float hourlyMaxGust[3] = {0, 0, 0}; // Rafales max des 3 dernières heures
uint8_t currentHourSlot = 0;

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
void updateGustDetection(float currentSpeed);
void processCommand(String command);
void resetArduino();
String getCardinalDirection(float degrees);
String getSIMPhoneNumber();
bool initializeSIM();
bool checkSIMConnection();
String sendATCommand(const String &cmd, unsigned long timeout);
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
        return true;
    }
    // Si PIN requis
    else if (resp.indexOf("+CPIN: SIM PIN") != -1) {
        // Essai avec 0000
        resp = sendATCommand("AT+CPIN=\"0000\"", 5000);
        
        if (resp.indexOf("OK") != -1) {
            return true;
        } else {
            // Essai avec 1234 si 0000 échoue
            resp = sendATCommand("AT+CPIN=\"1234\"", 5000);
            
            if (resp.indexOf("OK") != -1) {
                return true;
            } else {
                return false;
            }
        }
    }
    // Si carte bloquée (PUK nécessaire)
    else if (resp.indexOf("+CPIN: SIM PUK") != -1) {
        return false;
    }
    // Autres cas
    else {
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
    }
}

void updateGustDetection(float currentSpeed) {
    unsigned long currentTime = millis();
    
    // Réinitialisation périodique des rafales
    if (currentTime - lastGustReset > GUST_RESET_INTERVAL) {
        maxGustSpeed = 0;
        lastGustReset = currentTime;
        currentHourSlot = (currentHourSlot + 1) % 3;
        hourlyMaxGust[currentHourSlot] = 0;
    }
    
    // Détection des rafales
    if (currentSpeed > movingAvgSpeed * GUST_FACTOR && 
        (currentSpeed - movingAvgSpeed) >= MIN_GUST_DIFF) {
        if (gustStartTime == 0) {
            gustStartTime = currentTime;
        } else if (currentTime - gustStartTime >= GUST_MIN_DURATION) {
            if (currentSpeed > maxGustSpeed) {
                maxGustSpeed = currentSpeed;
                if (currentSpeed > hourlyMaxGust[currentHourSlot]) {
                    hourlyMaxGust[currentHourSlot] = currentSpeed;
                }
            }
        }
    } else {
        gustStartTime = 0;
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
    memset(speedBuffer, 0, sizeof(speedBuffer));
    memset(smsBuffer, 0, sizeof(smsBuffer));
   
    for(int i=0; i<15; i++) windSpeed1Min[i] = 0;
    for(int i=0; i<30; i++) windSpeed1H[i] = 0;

    smsBufferIndex = 0;
    windSpeed1MinIndex = 0;
    windSpeed1HIndex = 0;
    
    // Initialisation
    initializeEEPROM();
    loadValuesFromEEPROM();
    lastGustReset = millis();
    cycleStartTime = millis();
    
    // Initialisation SIM
    if (initializeSIM()) {
        simConnected = checkSIMConnection();
    } else {
        simConnected = false;
    }

    // Configuration SIM7670C
    sendATCommand("ATE0", 1000);  // Désactive echo
    sendATCommand("AT+CMEE=2", 1000);  // Messages d'erreur détaillés
    sendATCommand("AT+CNMI=2,2,0,0", 1000);  // Notification immédiate des SMS
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

        // Rejette les valeurs "disconnected" et les valeurs extrêmes
        if (newTemp != DEVICE_DISCONNECTED_C && newTemp >= -40.0 && newTemp <= 100.0) {
            tempBuffer[tempIndex] = newTemp;
            tempIndex = (tempIndex + 1) % 10;
        } else {
            Serial.println("Erreur : Température invalide ou capteur déconnecté");
        }
        
        // Calcul moyenne glissante (sur 10 mesures = 20 secondes)
        float sumTemp = 0.0;
        for (int i = 0; i < 10; i++) sumTemp += tempBuffer[i];
        currentTemp = sumTemp / 10.0;

        // Affichage dans le terminal
        if (usbConnected) {
            Serial.print("Temp: ");
            Serial.print(currentTemp, 1);
            Serial.print("°C | ");
            
            Serial.print("Vitesse: ");
            Serial.print(filteredSpeed / 10.0f, 1);  // Vitesse instantanée filtrée
            Serial.print(" km/h (");
            
            // Calcul de la vitesse moyenne sur 1 minute
            float avg1Min = 0.0;
            uint8_t count = 0;
            for (uint8_t i = 0; i < 15; i++) {
                if (windSpeed1Min[i] > 0) {
                    avg1Min += windSpeed1Min[i];
                    count++;
                }
            }
            if (count > 0) avg1Min /= count;
            
            Serial.print(avg1Min / 10.0f, 1);  // Vitesse moyenne
            Serial.print(" km/h) | ");
            
            // Affichage direction vent
            float direction = (analogRead(WIND_VANE_PIN) * ADC_TO_VOLTAGE / 3.3f) * 360.0f;
            direction = (direction * (windVaneCoeff / 10.0f)) + (windVaneOffset / 10.0f);
            while (direction < 0) direction += 360;
            while (direction >= 360) direction -= 360;
            
            Serial.print("Dir: ");
            Serial.print(direction, 1);
            Serial.print("° (");
            Serial.print(getCardinalDirection(direction));
            Serial.println(")");
        }
    }
    
    // Filtrage des données
    if (currentMillis - last1mMeasure >= MEASURE_INTERVAL_1M) {
        last1mMeasure = currentMillis;
        filterWindSpeed();
    }
    
    if (currentMillis - last1hMeasure >= MEASURE_INTERVAL_1H) {
        last1hMeasure = currentMillis;
        // calculateTrend();  // <-- Commentez ou supprimez cette ligne
    }

    // Vérification du module SIM
    if (currentMillis - lastSIMCheck >= SIM_CHECK_INTERVAL) {
        lastSIMCheck = currentMillis;
        bool newStatus = checkSIMConnection();
        if (newStatus != simConnected) {
            simConnected = newStatus;
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

void receiveSMS() {
    while (Serial2.available()) {
        char c = Serial2.read();
        
        // Gestion du buffer circulaire
        if (smsBufferIndex < BUFFER_SIZE - 1) {
            smsBuffer[smsBufferIndex++] = c;
        } else {
            smsBufferIndex = 0;
        }
        
        lastCharTime = millis();
        
        // Vérifie si on a reçu un message complet
        checkCompleteMessage();
    }
    
    // Timeout si pas de caractères reçus depuis 500ms
    if (smsBufferIndex > 0 && (millis() - lastCharTime > 500)) {
        processBuffer();
        smsBufferIndex = 0;
    }
}

void checkCompleteMessage() {
    // Vérifie la fin de message typique (OK\r\n ou ERROR\r\n)
    if (smsBufferIndex >= 5) {
        if (strstr(smsBuffer + smsBufferIndex - 5, "OK\r\n") || 
            strstr(smsBuffer + smsBufferIndex - 8, "ERROR\r\n")) {
            processBuffer();
            smsBufferIndex = 0;
        }
    }
}

void processBuffer() { 
    // Termine la chaîne
    smsBuffer[smsBufferIndex] = '\0';
    
    // Vérifie si c'est un SMS
    char* smsStart = strstr(smsBuffer, "+CMT:");
    if (smsStart) {
        // Extraction du numéro
        char* numStart = strchr(smsStart, '"') + 1;
        char* numEnd = strchr(numStart, '"');
        
        // Extraction du contenu du SMS (après le dernier \n)
        char* contentStart = strchr(smsStart, '\n');
        if (contentStart) contentStart++;
        
        if (numStart && numEnd && contentStart) {
            *numEnd = '\0';  // Termine le numéro

            // Stocke le numéro de l'expéditeur
            strncpy(senderNumber, numStart, sizeof(senderNumber)-1);
            senderNumber[sizeof(senderNumber)-1] = '\0';
            
            // Convertit le contenu en majuscules avant comparaison
            char upperContent[BUFFER_SIZE];
            int i = 0;
            for (; contentStart[i] && i < BUFFER_SIZE-1; i++) {
                upperContent[i] = toupper(contentStart[i]);
            }
            upperContent[i] = '\0';

            if (strstr(upperContent, "METEO0000")) {
                // Préparation du message météo complet
                float direction = (analogRead(WIND_VANE_PIN) * ADC_TO_VOLTAGE / 3.3f) * 360.0f;
                direction = (direction * (windVaneCoeff / 10.0f)) + (windVaneOffset / 10.0f);
                while (direction < 0) direction += 360;
                while (direction >= 360) direction -= 360;

                String directionStr = String(direction, 1);

                // Création du message
                String message = "Station Meteo AMCPLV\n";
                message += "Vitesse (inst.): " + String(filteredSpeed / 10.0f, 1) + " km/h\n";
                message += "Vitesse (moy.): " + String(windSpeed1Min[windSpeed1MinIndex] / 10.0f, 1) + " km/h\n";
                message += "Dir: " + String(direction, 1) + "Deg (" + getCardinalDirection(direction) + ")\n";
                message += "Rafale max : " + String(maxGustSpeed, 1) + " km/h\n";
                message += "Temp: " + String(currentTemp, 1) + " Deg C\n";

                if (simConnected) {
                    sendSMS(senderNumber, message);
                }
            }
            
            // Traitement de la commande de réinitialisation
            if (strstr(contentStart, "meteo999")) {
                resetArduino();
            }
        }
    }
}

void initializeEEPROM() {
    // Vérifie si l'EEPROM est vierge (première utilisation)
    bool isInitialized = false;
    for (int i = 0; i < 8; i++) {
        if (EEPROM.read(i) != 0xFF) {
            isInitialized = true;
            break;
        }
    }
    
    if (!isInitialized) {
        EEPROM.put(ANEMOMETER_COEFF_EEPROM_ADDR, anemometerCoeff);
        EEPROM.put(ANEMOMETER_OFFSET_EEPROM_ADDR, anemometerOffset);
        EEPROM.put(WIND_VANE_COEFF_EEPROM_ADDR, windVaneCoeff);
        EEPROM.put(WIND_VANE_OFFSET_EEPROM_ADDR, windVaneOffset);
        EEPROM.commit();
    }
}

void loadValuesFromEEPROM() {
    EEPROM.get(ANEMOMETER_COEFF_EEPROM_ADDR, anemometerCoeff);
    EEPROM.get(ANEMOMETER_OFFSET_EEPROM_ADDR, anemometerOffset);
    EEPROM.get(WIND_VANE_COEFF_EEPROM_ADDR, windVaneCoeff);
    EEPROM.get(WIND_VANE_OFFSET_EEPROM_ADDR, windVaneOffset);
}

void saveValuesToEEPROM() {
    EEPROM.put(ANEMOMETER_COEFF_EEPROM_ADDR, anemometerCoeff);
    EEPROM.put(ANEMOMETER_OFFSET_EEPROM_ADDR, anemometerOffset);
    EEPROM.put(WIND_VANE_COEFF_EEPROM_ADDR, windVaneCoeff);
    EEPROM.put(WIND_VANE_OFFSET_EEPROM_ADDR, windVaneOffset);
    EEPROM.commit();
}

void measureWindSpeed() {
    uint16_t sensorValue = analogRead(ANEMOMETER_PIN);
    if (sensorValue < 10) sensorValue = 0;

    float voltage = sensorValue * ADC_TO_VOLTAGE;
    float speedMs = (voltage / 3.3f) * 32.5f;
    if (speedMs < 0) speedMs = 0;

    float adjustedSpeed = speedMs * (anemometerCoeff / 10.0f) + (anemometerOffset / 10.0f);
    uint16_t speedKmh = static_cast<uint16_t>(adjustedSpeed * 3.6f * 10);
    
    speedBuffer[speedBufferIndex] = speedKmh / 10.0f;
    speedBufferIndex = (speedBufferIndex + 1) % 30;
    
    // Détection des rafales
    updateGustDetection(speedKmh / 10.0f);

    filteredSpeed = (1.0 - FILTER_COEFF) * filteredSpeed + FILTER_COEFF * speedKmh;

    windSpeedSum += speedKmh;
    windSpeedCount++;
    windSpeed1Min[windSpeed1MinIndex] = speedKmh;
    windSpeed1MinIndex = (windSpeed1MinIndex + 1) % 15;
}

void measureWindDirection() {
    uint16_t sensorValue = analogRead(WIND_VANE_PIN);
    float voltage = sensorValue * ADC_TO_VOLTAGE;
    float direction = (voltage / 3.3f) * 360.0f;
    direction = 360.0f - direction;
    direction = (direction * (windVaneCoeff / 10.0f)) + (windVaneOffset / 10.0f);
    while (direction < 0) direction += 360;
    while (direction >= 360) direction -= 360;
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
    if (windSpeedCount > 0) {
        float average2s = windSpeedSum / static_cast<float>(windSpeedCount);
        windSpeedSum = 0;
        windSpeedCount = 0;

        windSpeed1Min[windSpeed1MinIndex] = static_cast<uint16_t>(average2s);
        windSpeed1MinIndex = (windSpeed1MinIndex + 1) % 15;

        // Calcul de la moyenne mobile pour la détection des rafales
        float sum = 0;
        uint8_t count = 0;
        for (uint8_t i = 0; i < 15; i++) {
            if (windSpeed1Min[i] > 0) {
                sum += windSpeed1Min[i];
                count++;
            }
        }
        if (count > 0) {
            movingAvgSpeed = sum / count;
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
        Serial.println("Configuration sauvegardée en EEPROM");
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

            // Création du message
            String message = "Station Meteo AMCPLV\n";
            message += "Vitesse (inst.): " + String(filteredSpeed / 10.0f, 1) + " km/h\n";
            message += "Vitesse (moy.): " + String(windSpeed1Min[windSpeed1MinIndex] / 10.0f, 1) + " km/h\n";
            message += "Dir: " + String(direction, 1) + "Deg (" + getCardinalDirection(direction) + ")\n";
            message += "Rafale max: " + String(maxGustSpeed, 1) + " km/h\n";
            message += "Temp: " + String(currentTemp, 1) + " Deg C\n";

            if (sendSMS(DEST_PHONE, message)) {
                Serial.println("SMS envoyé avec succès");
            } else {
                Serial.println("Erreur lors de l'envoi du SMS");
            }
        } else {
            Serial.println("Module SIM non connecté, impossible d'envoyer SMS");
        }
    }
    else {
        Serial.println("Commande non reconnue. Tapez '?' pour l'aide.");
    }
}

void resetArduino() {
    delay(1000);
    rp2040.reboot();
}