#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>

const char* WIFI_SSID = "iPhone";
const char* WIFI_PASSWORD = "tarantula";

#define FIREBASE_HOST "sense-bell-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "lZ5hOsyDNVMex6IibzuiLZEToIsFeOC70ths5los"
#define HAPTIC_MOTOR_PIN D8

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const char doorbellPath[] = "/doorbell";
const unsigned long VIBRATION_COOLDOWN = 10000;
const unsigned long CONNECTION_CHECK_INTERVAL = 5000;
const unsigned long MEMORY_CHECK_INTERVAL = 5000;
const int CRITICAL_MEMORY = 6000;

unsigned long lastVibrationTime = 0;
unsigned long lastConnectionCheck = 0;
unsigned long lastMemoryCheck = 0;

enum VibrationState { IDLE, VIBRATING };
VibrationState vibrationState = IDLE;
unsigned long vibrationStartTime = 0;
unsigned long vibrationDuration = 0;

bool connectWiFi();
void setupFirebase();
void checkConnection();
void streamCallback(StreamData data);
void activateVibration(unsigned long duration);
void handleVibration();
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

  if (!connectWiFi()) {
    Serial.println("❌ No se pudo conectar a la red, reiniciando...");
    delay(2000);
    ESP.restart();
  }
  
  setupFirebase();
  
  if (!Firebase.beginStream(fbdo, doorbellPath)) {
    Serial.println("❌ Error al configurar stream: " + fbdo.errorReason());
    delay(2000);
    ESP.restart();
  }
  
  Serial.println("✅ Stream conectado a /doorbell");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastMemoryCheck > MEMORY_CHECK_INTERVAL) {
    lastMemoryCheck = currentMillis;
    Serial.printf("Memoria libre: %d bytes\n", ESP.getFreeHeap());
    
    if (ESP.getFreeHeap() < CRITICAL_MEMORY) {
      Serial.println("⚠️ Memoria crítica, reiniciando...");
      delay(1000);
      ESP.restart();
    }
  }

  if (currentMillis - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
    lastConnectionCheck = currentMillis;
    checkConnection();
  }

  handleVibration();
  delay(50);
}

bool connectWiFi() {
  Serial.println("\n📡 Intentando conectar a red...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000) { 
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n✅ Conectado a WiFi | RSSI: %d dBm\n", WiFi.RSSI());
    return true;
  }
  
  return false; 
}

void setupFirebase() {
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void streamCallback(StreamData data) {
  if (data.dataType() == "json") {
    FirebaseJson json = data.jsonObject();
    FirebaseJsonData jsonData;

    if (json.get(jsonData, "pressed") && jsonData.boolValue) {
      Serial.println("🔔 Timbre presionado detectado");
      activateVibration(6000);
    }

    bool priorityHigh = false;
    if (json.get(jsonData, "priority_high") && jsonData.boolValue) {
      maximumPriority();
      priorityHigh = true;
    }
    if (!priorityHigh && json.get(jsonData, "priority_medium") && jsonData.boolValue) {
      mediumPriority();
    } else if (json.get(jsonData, "priority_low") && jsonData.boolValue) {
      minimumPriority();
    }

    lastVibrationTime = millis();
    resetDoorbellStatus();
  }
}

void activateVibration(unsigned long duration) {
  if (vibrationState == IDLE) {
    digitalWrite(HAPTIC_MOTOR_PIN, HIGH);
    vibrationStartTime = millis();
    vibrationDuration = duration;
    vibrationState = VIBRATING;
  }
}

void handleVibration() {
  if (vibrationState == VIBRATING && millis() - vibrationStartTime >= vibrationDuration) {
    digitalWrite(HAPTIC_MOTOR_PIN, LOW);
    vibrationState = IDLE;
  }
}

void maximumPriority() {
  Serial.println("🔴 Alta prioridad detectada");
  for (int i = 0; i < 5; i++) {
    activateVibration(700);
    while (vibrationState == VIBRATING) {
      handleVibration();
      delay(10);
    }
    if (i < 2) {
      delay(250);
    }
  }
}

void mediumPriority() {
  Serial.println("🟡 Prioridad media detectada");
  for (int i = 0; i < 5; i++) {
    activateVibration(350);
    while (vibrationState == VIBRATING) {
      handleVibration();
      delay(10);
    }
    delay(200);
  }
}

void minimumPriority() {
  Serial.println("🟢 Prioridad baja detectada");
  for (int i = 0; i < 5; i++) {
    activateVibration(200);
    while (vibrationState == VIBRATING) {
      handleVibration();
      delay(10);
    }
    delay(100);
  }
}

void resetDoorbellStatus() {
  FirebaseJson json;
  json.set("pressed", false);
  json.set("priority_low", false);
  json.set("priority_medium", false);
  json.set("priority_high", false);
  Firebase.updateNode(fbdo, doorbellPath, json);
}

void checkConnection() {
  if (!Firebase.ready() || WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ Problema de conexión, intentando reconectar...");
    if (!connectWiFi()) {
      Serial.println("❌ No se pudo reconectar, reiniciando...");
      delay(1000);
      ESP.restart();
    }
    setupFirebase();
  }
}
