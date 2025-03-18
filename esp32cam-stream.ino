#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

const char* ssid = "Luna 2.4";
const char* password = "Grecia2607";

WebServer server(80);

#define FLASH_LED_PIN 4
#define DOORBELL 13  

const char* FIREBASE_URL = "https://sense-bell-default-rtdb.firebaseio.com/flash.json";
const char* DOORBELL_URL = "https://sense-bell-default-rtdb.firebaseio.com/doorbell.json";

bool previousFlashState = false;
bool previousDoorbellState = false;  
unsigned long lastFirebaseCheck = 0;
unsigned long lastReconnectAttempt = 0;
const unsigned long firebaseCheckInterval = 1000;
const unsigned long reconnectInterval = 10000;

// Configuración del NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -3 * 3600, 60000); // Ajusta el offset para Argentina (-3 horas)

void configCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Error al inicializar la cámara");
    while (true);
  }
}

void handleStream() {
  WiFiClient client = server.client();
  camera_fb_t* fb = NULL;
  String boundary = "frame";
  String response = "HTTP/1.1 200 OK\r\n" + String("Content-Type: multipart/x-mixed-replace; boundary=") + boundary + "\r\n\r\n";
  client.write(response.c_str(), response.length());

  while (client.connected()) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Error al capturar imagen");
      continue;
    }

    String part = "--" + boundary + "\r\n" + "Content-Type: image/jpeg\r\n" + "Content-Length: " + String(fb->len) + "\r\n\r\n";
    client.write(part.c_str(), part.length());
    client.write(fb->buf, fb->len);
    client.write("\r\n");

    esp_camera_fb_return(fb);
  }
}

void checkFirebaseStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ No hay conexión WiFi.");
    return;
  }

  HTTPClient http;
  http.begin(FIREBASE_URL);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.GET();
  if (httpResponseCode == 200) {
    String payload = http.getString();
    payload.trim();

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      bool currentFlashState = doc["pressed"];  
      if (currentFlashState != previousFlashState) {
        digitalWrite(FLASH_LED_PIN, currentFlashState ? HIGH : LOW);
        Serial.println(currentFlashState ? "🔔 LED ENCENDIDO" : "🔕 LED APAGADO");
        previousFlashState = currentFlashState;
      }
    } else {
      Serial.println("❌ Error al parsear JSON: " + String(error.c_str()));
    }
  } else {
    Serial.printf("❌ Error al obtener datos: %d\n", httpResponseCode);
  }

  http.end();
}

String getFormattedDateTime() {
  timeClient.update(); 
  unsigned long epochTime = timeClient.getEpochTime();

  if (epochTime < 1609459200) { 
    Serial.println("❌ Tiempo no válido. Verifica la conexión a Internet.");
    return "Fecha no disponible";
  }

  time_t rawTime = (time_t)epochTime;
  struct tm *ptm = gmtime(&rawTime);

  char buffer[30];
  sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", ptm->tm_mday, ptm->tm_mon + 1, ptm->tm_year + 1900, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  return String(buffer);
}

void sendDoorbellStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ No hay conexión WiFi.");
    return;
  }

  HTTPClient http;
  http.begin(DOORBELL_URL); // Usar la ruta base "doorbell"
  http.addHeader("Content-Type", "application/json");

  String formattedDateTime = getFormattedDateTime();

  String jsonPayload = "{\"pressed\": true, \"timestamp\": \"" + formattedDateTime + "\"}";

  int httpResponseCode = http.POST(jsonPayload);

  if (httpResponseCode == 200) {
    Serial.println("📡 Estado del timbre enviado a Firebase.");
  } else {
    Serial.printf("❌ Error al enviar datos: %d\n", httpResponseCode);
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  setCpuFrequencyMhz(240);

  pinMode(FLASH_LED_PIN, OUTPUT);
  pinMode(DOORBELL, INPUT_PULLUP);  

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a WiFi");

  timeClient.begin();
  timeClient.update(); 

  configCamera();

  server.on("/stream", HTTP_GET, handleStream);

  server.begin();
  Serial.println("Servidor iniciado en:");
  Serial.print("🔗 http://");
  Serial.print(WiFi.localIP());
  Serial.println("/stream");
}

void loop() {
  server.handleClient();

  bool currentDoorbellState = (digitalRead(DOORBELL) == LOW);

  if (currentDoorbellState != previousDoorbellState) {
    if (currentDoorbellState) {
      Serial.println("🔔 Botón presionado!");
      digitalWrite(FLASH_LED_PIN, HIGH);
      sendDoorbellStatus(); 
      delay(9000);  
    }
    else {
      digitalWrite(FLASH_LED_PIN, LOW);
    }
    previousDoorbellState = currentDoorbellState;
    delay(1000);
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastFirebaseCheck >= firebaseCheckInterval) {
    checkFirebaseStatus();
    lastFirebaseCheck = currentMillis;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (currentMillis - lastReconnectAttempt >= reconnectInterval) {
      Serial.println("WiFi perdido, reconectando...");
      WiFi.disconnect();
      WiFi.reconnect();
      lastReconnectAttempt = currentMillis;
    }
  }
}
