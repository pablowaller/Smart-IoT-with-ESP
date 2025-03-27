#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>

#define WIFI_SSID "iPhone"
#define WIFI_PASSWORD "tarantula"
#define FIREBASE_HOST "sense-bell-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "lZ5hOsyDNVMex6IibzuiLZEToIsFeOC70ths5los"
#define HAPTIC_MOTOR_PIN 14

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
const char doorbellPath[] = "/doorbell";
unsigned long lastVibrationTime = 0;
const unsigned long VIBRATION_COOLDOWN = 10000;

void connectWiFi();
void setupFirebase();
void checkConnection();
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
void activateVibration();
void simpleVibration();
void maximumPriority();
void mediumPriority();
void minimumPriority();
void resetDoorbellStatus();

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nIniciando sistema...");

  pinMode(HAPTIC_MOTOR_PIN, OUTPUT);
  digitalWrite(HAPTIC_MOTOR_PIN, LOW);

  connectWiFi();
  setupFirebase();

  while (!Firebase.ready()) {
    Serial.println("Esperando conexión Firebase...");
    delay(1000);
  }

  if (!Firebase.RTDB.beginStream(&fbdo, doorbellPath)) {
    Serial.println("❌ Error al configurar stream: " + fbdo.errorReason());
    ESP.restart();
  }

  Firebase.RTDB.setStreamCallback(&fbdo, streamCallback, streamTimeoutCallback);
  Serial.println("✅ Stream conectado a /doorbell");
}

void loop() {
  static unsigned long lastCheck = millis();
  static unsigned long lastMemoryCheck = millis();

  if (millis() - lastMemoryCheck > 5000) {
    lastMemoryCheck = millis();
    Serial.printf("Memoria libre: %d bytes\n", ESP.getFreeHeap());
  }

  if (ESP.getFreeHeap() < 5000) {  
    Serial.println("⚠️ Memoria crítica, reiniciando...");
    ESP.restart();
  }

  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    checkConnection();
  }

  delay(500);
}

void connectWiFi() {
  Serial.println("\n📡 Conectando a WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n❌ Fallo conexión WiFi");
    ESP.restart();
  }

  Serial.printf("\n📶 WiFi conectado | IP: %s\n", WiFi.localIP().toString().c_str());
}

void setupFirebase() {
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
}

void streamCallback(FirebaseStream data) {
  if (data.dataType() == "json") {
    FirebaseJson json = data.jsonObject();
    FirebaseJsonData jsonData;

    json.get(jsonData, "pressed");
    if (jsonData.success && jsonData.boolValue) {
      Serial.println("🔔 Timbre presionado detectado");
      activateVibration();  // Cambiado de simpleVibration a activateVibration
    }

    bool priorityLow = false, priorityMedium = false, priorityHigh = false;

    json.get(jsonData, "priority_low");
    if (jsonData.success) priorityLow = jsonData.boolValue;

    json.get(jsonData, "priority_medium");
    if (jsonData.success) priorityMedium = jsonData.boolValue;

    json.get(jsonData, "priority_high");
    if (jsonData.success) priorityHigh = jsonData.boolValue;

    if (priorityHigh) {
      Serial.println("⚠️ Alta prioridad detectada");
      maximumPriority();
    } else if (priorityMedium) {
      Serial.println("🟡 Prioridad media detectada");
      mediumPriority();
    } else if (priorityLow) {
      Serial.println("🔴 Prioridad baja detectada");
      minimumPriority();
    }

    lastVibrationTime = millis();
    resetDoorbellStatus();
  }
}

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("⚠️ Stream timeout, reconectando...");
    Firebase.RTDB.beginStream(&fbdo, doorbellPath);
  }
}

void activateVibration() {
  Serial.println("🔔 Alguien presionó el timbre! Activando vibración");
  digitalWrite(HAPTIC_MOTOR_PIN, HIGH);
  delay(6000);
  digitalWrite(HAPTIC_MOTOR_PIN, LOW);
}

void simpleVibration() {
  Serial.println("🔔 Activando vibración simple");
  digitalWrite(HAPTIC_MOTOR_PIN, HIGH);
  delay(500);
  digitalWrite(HAPTIC_MOTOR_PIN, LOW);
}

void maximumPriority() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(HAPTIC_MOTOR_PIN, HIGH);
    delay(700);
    digitalWrite(HAPTIC_MOTOR_PIN, LOW);
    if (i < 2) {
      delay(250);
    }
  }
}

void mediumPriority() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(HAPTIC_MOTOR_PIN, HIGH);
    delay(350);
    digitalWrite(HAPTIC_MOTOR_PIN, LOW);
    delay(200);
  }
}

void minimumPriority() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(HAPTIC_MOTOR_PIN, HIGH);
    delay(200);
    digitalWrite(HAPTIC_MOTOR_PIN, LOW);
    delay(100);
  }
}

void resetDoorbellStatus() {
  FirebaseJson json;
  json.set("pressed", false);
  json.set("priority_low", false);
  json.set("priority_medium", false);
  json.set("priority_high", false);

  if (Firebase.RTDB.setJSON(&fbdo, doorbellPath, &json)) {
    Serial.println("Estados reiniciados correctamente");
  } else {
    Serial.println("Error al reiniciar estados: " + fbdo.errorReason());
  }
}

void checkConnection() {
  if (!Firebase.ready() || WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Problema de conexión, reiniciando...");
    delay(1000);
    ESP.restart();
  }
}
