#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>  

#define WIFI_SSID "Luna 2.4"
#define WIFI_PASSWORD "Grecia2607"

#define DOORBELL_URL "https://sense-bell-default-rtdb.firebaseio.com/doorbell.json"
#define ATTENDANCE_URL "https://sense-bell-default-rtdb.firebaseio.com/attendance.json"  

#define HAPTIC_MOTOR_PIN 14 

void setup() {
    Serial.begin(115200);

    pinMode(HAPTIC_MOTOR_PIN, OUTPUT);
    digitalWrite(HAPTIC_MOTOR_PIN, LOW);  
    WiFi.disconnect(true);
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
        hapticFeedbackPatterns();
    } else {
        Serial.println("❌ No hay conexión WiFi.");
    }
    delay(1000); 
}

void checkDoorbellStatus() {
    HTTPClient http; 
    WiFiClient client;
    http.begin(client, DOORBELL_URL);
    http.addHeader("Content-Type", "application/json");
    http.setUserAgent("ESP8266");

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
        Serial.printf("❌ Error al obtener datos del timbre: %d\n", httpResponseCode);
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

void hapticFeedbackPatterns() {
    HTTPClient http;
    WiFiClient client;
    http.begin(client, ATTENDANCE_URL);  // 🔄 URL cambiada
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
        String payload = http.getString();
        payload.trim();

        StaticJsonDocument<500> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            if (doc.containsKey("priority")) {  
                String priority = doc["priority"].as<String>();

                Serial.println("Prioridad: " + priority);

                if (priority == "high") {
                    maximumPriority();
                } else if (priority == "medium") {
                    mediumPriority();
                } else {
                    minimumPriority();
                }
            } else {
                Serial.println("⚠️ No se encontró el campo 'priority' en JSON.");
            }
        } else {
            Serial.println("❌ Error al parsear JSON: " + String(error.c_str()));
        }
    } else {
        Serial.printf("❌ Error al obtener los visitantes: %d\n", httpResponseCode);
    }

    http.end();
}

void maximumPriority() {
  Serial.println("💤 Vibración larga (700ms)");
  digitalWrite(HAPTIC_MOTOR_PIN, HIGH);
  delay(700);
  digitalWrite(HAPTIC_MOTOR_PIN, LOW);  
}

void mediumPriority() {
  Serial.println("📳 3 vibraciones medias (300ms ON/200ms OFF)");
  for (int i = 0; i < 3; i++) {
    digitalWrite(HAPTIC_MOTOR_PIN, HIGH);
    delay(300);
    digitalWrite(HAPTIC_MOTOR_PIN, LOW);
    delay(200);
  }
}

void minimumPriority() {
  Serial.println("⚡ 5 vibraciones rápidas (100ms ON/50ms OFF)");
  for (int i = 0; i < 5; i++) {
    digitalWrite(HAPTIC_MOTOR_PIN, HIGH);
    delay(100);
    digitalWrite(HAPTIC_MOTOR_PIN, LOW);
    delay(50);
  }
}
