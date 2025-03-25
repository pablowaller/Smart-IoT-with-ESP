#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>

// Configuración WiFi
#define WIFI_SSID "Luna 2.4"
#define WIFI_PASSWORD "Grecia2607"

// Configuración Firebase
#define FIREBASE_HOST "sense-bell-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "lZ5hOsyDNVMex6IibzuiLZEToIsFeOC70ths5los"

// Hardware
#define HAPTIC_MOTOR_PIN 14

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const String nodes[4] = {
  "/notifications/doorbell",      // Índice 0 - Timbre (formato JSON)
  "/notifications/priority_low",  // Índice 1 - Prioridad Baja (booleano)
  "/notifications/priority_medium",// Índice 2 - Prioridad Media (booleano)
  "/notifications/priority_high"  // Índice 3 - Prioridad Alta (booleano)
};

unsigned long lastValidEvent = 0;
const unsigned long EVENT_DEBOUNCE = 1000; // 1 segundo

void IRAM_ATTR handleActivation(uint8_t nodeIndex) {
  if(millis() - lastValidEvent < EVENT_DEBOUNCE) {
    Serial.println("⏳ Ignorando evento (debounce)");
    return;
  }
  
  lastValidEvent = millis();
  
  switch(nodeIndex) {
    case 0: // Doorbell - 5 segundos continuos
      Serial.println("\n🔔 TIMBRE ACTIVADO (5 segundos)");
      digitalWrite(HAPTIC_MOTOR_PIN, HIGH);
      for(int i=0; i<50; i++) { // 50 x 100ms = 5s
        delay(100);
        ESP.wdtFeed(); // Alimentar el watchdog
      }
      digitalWrite(HAPTIC_MOTOR_PIN, LOW);
      break;
      
    case 1: // Prioridad baja
      Serial.println("\n🔴 ACTIVANDO Prioridad BAJA");
      for(int i=0; i<5; i++) {
        digitalWrite(HAPTIC_MOTOR_PIN, HIGH);
        delay(200);
        digitalWrite(HAPTIC_MOTOR_PIN, LOW);
        if(i<4) {
          delay(100);
          ESP.wdtFeed();
        }
      }
      break;
      
    case 2: // Prioridad media
      Serial.println("\n🟡 ACTIVANDO Prioridad MEDIA");
      for(int i=0; i<5; i++) {
        digitalWrite(HAPTIC_MOTOR_PIN, HIGH);
        delay(350);
        digitalWrite(HAPTIC_MOTOR_PIN, LOW);
        if(i<4) {
          delay(200);
          ESP.wdtFeed();
        }
      }
      break;
      
    case 3: // Prioridad alta
      Serial.println("\n🟢 ACTIVANDO Prioridad ALTA");
      for(int i=0; i<5; i++) {
        digitalWrite(HAPTIC_MOTOR_PIN, HIGH);
        delay(700);
        digitalWrite(HAPTIC_MOTOR_PIN, LOW);
        if(i<4) {
          delay(250);
          ESP.wdtFeed();
        }
      }
      break;
  }
  
  // Resetear todos los nodos después de la activación
  resetNodes();
}

void connectWiFi() {
  Serial.println("\n📡 Conectando a WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startTime = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - startTime < 30000) {
    delay(500);
    Serial.print(".");
  }
  
  if(WiFi.status() != WL_CONNECTED) {
    Serial.println("\n❌ Fallo conexión WiFi");
    ESP.restart();
  }
  
  Serial.printf("\n📶 WiFi conectado | IP: %s\n", WiFi.localIP().toString().c_str());
}

void setupFirebase() {
  config.database_url = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  
  // Configuración para ESP8266
  config.timeout.sslHandshake = 30 * 1000; // 30 segundos para SSL
  config.timeout.serverResponse = 30 * 1000;
  config.timeout.wifiReconnect = 10 * 1000;
  
  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096, 1024); // Buffer para SSL
  fbdo.setResponseSize(2048); // Tamaño de respuesta
  
  Serial.println("🔥 Inicializando Firebase...");
  Firebase.begin(&config, &auth);
}

bool resetNodes() {
  Serial.println("\n🔄 Reseteando nodos...");
  bool success = true;
  
  // Resetear doorbell (JSON)
  FirebaseJson json;
  json.set("pressed", false);
  if(!Firebase.RTDB.setJSON(&fbdo, nodes[0], &json)) {
    Serial.printf("❌ Error doorbell: %s\n", fbdo.errorReason().c_str());
    success = false;
  }
  
  // Resetear prioridades
  for(uint8_t i = 1; i < 4; i++) {
    if(!Firebase.RTDB.setBool(&fbdo, nodes[i], false)) {
      Serial.printf("❌ Error %s: %s\n", nodes[i].c_str(), fbdo.errorReason().c_str());
      success = false;
    }
  }
  
  return success;
}

void setupStreams() {
  Serial.println("\n🔌 Configurando streams Firebase...");
  
  for(uint8_t i = 0; i < 4; i++) {
    Serial.printf("Iniciando stream en %s... ", nodes[i].c_str());
    if(Firebase.RTDB.beginStream(&fbdo, nodes[i].c_str())) {
      Firebase.RTDB.setStreamCallback(&fbdo, [](FirebaseStream data) {
        Serial.printf("\n🎯 Evento en %s. Tipo: %s\n", 
          data.streamPath().c_str(), data.dataType().c_str());
          
        bool trigger = false;
        FirebaseJsonData jsonData;
        
        if(data.dataType() == "json") {
          data.jsonObject().get(jsonData, "pressed");
          trigger = jsonData.boolValue;
        } else if(data.dataType() == "boolean") {
          trigger = data.boolData();
        }

        if(trigger) {
          uint8_t nodeIndex = 0;
          for(; nodeIndex < 4; nodeIndex++) {
            if(data.streamPath() == nodes[nodeIndex]) break;
          }
          
          if(nodeIndex < 4) {
            handleActivation(nodeIndex);
          }
        }
      }, [](bool timeout) {
        if(timeout) Serial.println("⚠️ Stream timeout, reconectando...");
      });
      Serial.println("OK");
    } else {
      Serial.println("FALLÓ: " + fbdo.errorReason());
      delay(2000);
      ESP.restart();
    }
    delay(100);
  }
  
  Serial.println("✅ Streams activos");
}

void setup() {
  Serial.begin(115200);
  pinMode(HAPTIC_MOTOR_PIN, OUTPUT);
  digitalWrite(HAPTIC_MOTOR_PIN, LOW);
  
  connectWiFi();
  setupFirebase();
  setupStreams();
  resetNodes();
  
  Serial.println("\n🚀 SISTEMA LISTO");
  Serial.println("----------------------------------");
  Serial.println("🔔 /notifications/doorbell");
  Serial.println("🔴 /notifications/priority_low");
  Serial.println("🟡 /notifications/priority_medium");
  Serial.println("🟢 /notifications/priority_high");
  Serial.println("----------------------------------");
}

void loop() {
  static unsigned long lastCheck = millis();
  
  if(millis() - lastCheck > 15000) { // Chequeo cada 15 segundos
    lastCheck = millis();
    
    if(!Firebase.ready()) {
      Serial.println("⚠️ Firebase no listo, reiniciando...");
      delay(1000);
      ESP.restart();
    }
    
    if(WiFi.status() != WL_CONNECTED) {
      Serial.println("⚠️ WiFi desconectado, reconectando...");
      WiFi.reconnect();
      delay(5000);
      if(WiFi.status() != WL_CONNECTED) {
        ESP.restart();
      }
    }
  }
  
  delay(100);
}
