#include <WiFi.h>
#include <Firebase_ESP_Client.h>

// Configuración de red y Firebase
#define WIFI_SSID "iPhone"
#define WIFI_PASSWORD "tarantula"
#define FIREBASE_HOST "sense-bell-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "lZ5hOsyDNVMex6IibzuiLZEToIsFeOC70ths5los"
#define HAPTIC_MOTOR_PIN 14

// Objetos Firebase
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Rutas y constantes
const char doorbellPath[] = "/doorbell";
const unsigned long VIBRATION_COOLDOWN = 10000;
const unsigned long CONNECTION_CHECK_INTERVAL = 5000;
const unsigned long MEMORY_CHECK_INTERVAL = 5000;
const int CRITICAL_MEMORY = 6000; // Aumentado a 6KB

// Variables de estado
unsigned long lastVibrationTime = 0;
unsigned long lastConnectionCheck = 0;
unsigned long lastMemoryCheck = 0;

// Estados para vibración
enum VibrationState { IDLE, VIBRATING };
VibrationState vibrationState = IDLE;
unsigned long vibrationStartTime = 0;
unsigned long vibrationDuration = 0;

// Prototipos de funciones
void connectWiFi();
void setupFirebase();
void checkConnection();
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);
void activateVibration(unsigned long duration);
void handleVibration();
void maximumPriority();
void mediumPriority();
void minimumPriority();
void resetDoorbellStatus();

void setup() {
  Serial.begin(115200);
  delay(1000); // Pequeño delay inicial para estabilizar
  
  Serial.println("\nIniciando sistema...");

  // Configuración del pin del motor
  pinMode(HAPTIC_MOTOR_PIN, OUTPUT);
  digitalWrite(HAPTIC_MOTOR_PIN, LOW);

  // Conexión a WiFi y Firebase
  connectWiFi();
  setupFirebase();

  // Configuración del stream de Firebase
  while (!Firebase.ready()) {
    Serial.println("Esperando conexión Firebase...");
    delay(1000);
  }

  if (!Firebase.RTDB.beginStream(&fbdo, doorbellPath)) {
    Serial.println("❌ Error al configurar stream: " + fbdo.errorReason());
    delay(2000);
    ESP.restart();
  }

  Firebase.RTDB.setStreamCallback(&fbdo, streamCallback, streamTimeoutCallback);
  Serial.println("✅ Stream conectado a /doorbell");
}

void loop() {
  unsigned long currentMillis = millis();

  // Verificación periódica de memoria
  if (currentMillis - lastMemoryCheck > MEMORY_CHECK_INTERVAL) {
    lastMemoryCheck = currentMillis;
    Serial.printf("Memoria libre: %d bytes\n", ESP.getFreeHeap());
    
    if (ESP.getFreeHeap() < CRITICAL_MEMORY) {
      Serial.println("⚠️ Memoria crítica, reiniciando...");
      delay(1000);
      ESP.restart();
    }
  }

  // Verificación periódica de conexión
  if (currentMillis - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
    lastConnectionCheck = currentMillis;
    checkConnection();
  }

  // Manejo de la vibración no bloqueante
  handleVibration();

  // Pequeño delay para evitar saturación
  delay(50);
}

void connectWiFi() {
  Serial.println("\n📡 Conectando a WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n❌ Fallo conexión WiFi");
    delay(2000);
    ESP.restart();
  }

  Serial.printf("\n📶 WiFi conectado | IP: %s | RSSI: %d dBm\n", 
               WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

void setupFirebase() {
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  config.timeout.serverResponse = 30;
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);
  
  // Configuración adicional para reducir uso de memoria
  fbdo.setBSSLBufferSize(1024, 1024); // Buffer más pequeño
  fbdo.setResponseSize(1024);
}

void streamCallback(FirebaseStream data) {
  if (data.dataType() == "json") {
    FirebaseJson json = data.jsonObject();
    FirebaseJsonData jsonData;

    // Verificar si el timbre fue presionado
    if (json.get(jsonData, "pressed") && jsonData.boolValue) {
      Serial.println("🔔 Timbre presionado detectado");
      activateVibration(6000); // Vibración de 6 segundos
    }

    // Verificar prioridades
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

void streamTimeoutCallback(bool timeout) {
  if (timeout) {
    Serial.println("⚠️ Stream timeout, reconectando en 2 segundos...");
    delay(2000); // Espera antes de reconectar
    
    if (!Firebase.RTDB.beginStream(&fbdo, doorbellPath)) {
      Serial.println("❌ Error al reconectar stream: " + fbdo.errorReason());
    } else {
      Serial.println("✅ Stream reconectado");
    }
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
}

void checkConnection() {
  if (!Firebase.ready() || WiFi.status() != WL_CONNECTED) {
    Serial.printf("⚠️ Problema de conexión | WiFi: %d | Firebase: %d\n", 
                 WiFi.status(), Firebase.ready());
    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
    
    // Intentar reconectar antes de reiniciar
    WiFi.reconnect();
    delay(2000);
    
    if (WiFi.status() != WL_CONNECTED || !Firebase.ready()) {
      Serial.println("❌ No se pudo reconectar, reiniciando...");
      delay(1000);
      ESP.restart();
    }
  }
}
