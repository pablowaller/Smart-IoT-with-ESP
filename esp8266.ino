#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>  

#define WIFI_SSID "Luna 2.4"
#define WIFI_PASSWORD "Grecia2607"

#define FIREBASE_URL "https://sense-bell-default-rtdb.firebaseio.com/doorbell.json"

#define HAPTIC_MOTOR_PIN 14 

void setup() {
    Serial.begin(115200);

    pinMode(HAPTIC_MOTOR_PIN, OUTPUT);
    digitalWrite(HAPTIC_MOTOR_PIN, LOW);  

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Conectando a WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n✅ Conectado a WiFi");
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        checkDoorbellStatus();
    } else {
        Serial.println("❌ No hay conexión WiFi.");
    }
    delay(1000); 
}

void checkDoorbellStatus() {
    HTTPClient http; 
    WiFiClient client;
    http.begin(client, FIREBASE_URL);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
        String payload = http.getString();
        payload.trim();

        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            bool doorbellPressed = doc["pressed"];
            String timestamp = doc["timestamp"];
            if (doorbellPressed) {
                Serial.println("🔔 Timbre presionado!");
                Serial.println("Fecha y hora: " + timestamp);

                activateHapticMotor();
            }
        } else {
            Serial.println("❌ Error al parsear JSON: " + String(error.c_str()));
        }
    } else {
        Serial.printf("❌ Error al obtener datos: %d\n", httpResponseCode);
    }

    http.end();
}

void activateHapticMotor() {
    Serial.println("🎚️ Activando motor háptico...");
    digitalWrite(HAPTIC_MOTOR_PIN, HIGH); 
    delay(500); 
    digitalWrite(HAPTIC_MOTOR_PIN, LOW);  
    Serial.println("🎚️ Motor háptico desactivado.");
}
