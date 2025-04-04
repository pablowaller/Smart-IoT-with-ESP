#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Firebase_ESP_Client.h>
#include <time.h>
#include <I2S.h>
#include <ArduinoJson.h>

const char* ssid = "Luna 2.4";
const char* password = "Grecia2607";

#define FIREBASE_HOST "sense-bell-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "AIzaSyDQJ-amic1aPwLp1B-XyctBgcMRd6ogYwM"

const String API_KEY = "AIzaSyDQJ-amic1aPwLp1B-XyctBgcMRd6ogYwM";
const String SPEECH_API_URL = "https://speech.googleapis.com/v1/speech:recognize?key=" + API_KEY;

const int sampleRate = 16000;
const int bufferSize = 1024;
int16_t audioBuffer[bufferSize];

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String base64Encode(const uint8_t* data, size_t length) {
  String base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String encodedString = "";
  int i = 0;
  int j = 0;
  uint8_t byteArray3[3];
  uint8_t byteArray4[4];

  while (length--) {
    byteArray3[i++] = *(data++);
    if (i == 3) {
      byteArray4[0] = (byteArray3[0] & 0xfc) >> 2;
      byteArray4[1] = ((byteArray3[0] & 0x03) << 4) + ((byteArray3[1] & 0xf0)) >> 4;
      byteArray4[2] = ((byteArray3[1] & 0x0f) << 2) + ((byteArray3[2] & 0xc0)) >> 6;
      byteArray4[3] = byteArray3[2] & 0x3f;

      for (i = 0; i < 4; i++) {
        encodedString += base64Chars[byteArray4[i]];
      }
      i = 0;
    }
  }

  if (i) {
    for (j = i; j < 3; j++) {
      byteArray3[j] = '\0';
    }

    byteArray4[0] = (byteArray3[0] & 0xfc) >> 2;
    byteArray4[1] = ((byteArray3[0] & 0x03) << 4) + ((byteArray3[1] & 0xf0) >> 4);
    byteArray4[2] = ((byteArray3[1] & 0x0f) << 2) + ((byteArray3[2] & 0xc0) >> 6);

    for (j = 0; j < i + 1; j++) {
      encodedString += base64Chars[byteArray4[j]];
    }

    while (i++ < 3) {
      encodedString += '=';
    }
  }

  return encodedString;
}

void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
  
  configTime(0, 0, "pool.ntp.org");
  while (!time(nullptr)) {
    delay(500);
    Serial.print(".");
  }
  
  config.host = FIREBASE_HOST;
  config.api_key = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  if (!I2S.begin(I2S_PHILIPS_MODE, sampleRate, 16)) {
    Serial.println("Error al iniciar I2S!");
    while (1);
  }
}

void loop() {
  static unsigned long lastRecording = 0;
  const unsigned long recordingInterval = 10000;
  
  if (millis() - lastRecording > recordingInterval) {
    lastRecording = millis();
    
    String audioBase64 = recordAudio(5000);
    String transcription = transcribeAudio(audioBase64);
    
    if (transcription != "") {
      String formattedTime = getFormattedDateTime();
      sendToFirebase(transcription, formattedTime);
      Serial.println("Transcripción: " + transcription);
      Serial.println("Timestamp: " + formattedTime);
    }
  }
}

String recordAudio(int durationMs) {
  size_t bytesRead = 0;
  size_t totalBytes = sampleRate * durationMs / 1000 * sizeof(int16_t);
  String audioBase64 = "";
  
  uint8_t* rawAudio = (uint8_t*)malloc(totalBytes);
  size_t offset = 0;
  
  unsigned long startTime = millis();
  while (millis() - startTime < durationMs) {
    bytesRead = I2S.readBytes((char*)audioBuffer, bufferSize * sizeof(int16_t));
    memcpy(rawAudio + offset, audioBuffer, bytesRead);
    offset += bytesRead;
  }
  
  audioBase64 = base64Encode(rawAudio, offset);
  free(rawAudio);
  
  return audioBase64;
}

String transcribeAudio(String audioBase64) {
  WiFiClientSecure client;
  HTTPClient http;
  String transcription = "";
  
  String payload = String("{\"config\":{\"encoding\":\"LINEAR16\",") + 
                   "\"sampleRateHertz\":" + String(sampleRate) + "," +
                   "\"languageCode\":\"es-AR\"," +
                   "\"enableAutomaticPunctuation\":true}," +
                   "\"audio\":{\"content\":\"" + audioBase64 + "\"}}";
  
  if (http.begin(client, SPEECH_API_URL)) {
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(payload);
    
    if (httpCode == HTTP_CODE_OK) {
      String response = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      
      if (doc["results"][0]["alternatives"][0]["transcript"]) {
        transcription = doc["results"][0]["alternatives"][0]["transcript"].as<String>();
      }
    } else {
      Serial.printf("Error en API: %d\n", httpCode);
    }
    http.end();
  }
  
  return transcription;
}

String getFormattedDateTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  
  return String(buffer);
}

void sendToFirebase(String text, String timestamp) {
  FirebaseJson json;
  json.set("text", text);
  json.set("timestamp", timestamp);
  
  if (Firebase.RTDB.pushJSON(&fbdo, "/transcriptions", &json)) {
    Serial.println("Datos enviados a Firebase");
  } else {
    Serial.print("Error Firebase: ");
    Serial.println(fbdo.errorReason());
  }
}
