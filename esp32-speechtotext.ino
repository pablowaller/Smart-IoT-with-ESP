#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <URLEncoder.h> 
#include "driver/i2s.h"

#define SAMPLE_RATE 16000
#define PIN_I2S_BCLK 26
#define PIN_I2S_LRC 22
#define PIN_I2S_DIN 35
#define AUDIO_BUFFER_SIZE 110000

const char* ssid = "Luna 2.4";
const char* password = "Grecia2607";
const char* FIREBASE_STORAGE_BUCKET = "sense-bell.firebasestorage.app";

WiFiClientSecure client;
HTTPClient http;

size_t recordAudio(uint8_t* audioBuffer, size_t bufferSize) {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 512,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = PIN_I2S_BCLK,
        .ws_io_num = PIN_I2S_LRC,
        .data_out_num = I2S_PIN_NO_CHANGE,  // 📌 Orden corregido
        .data_in_num = PIN_I2S_DIN
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);

    Serial.println("🎤 Iniciando grabación...");
    size_t bytesRead = 0;
    size_t totalBytes = 0;

    while (totalBytes < bufferSize) {
        i2s_read(I2S_NUM_0, audioBuffer + totalBytes, bufferSize - totalBytes, &bytesRead, portMAX_DELAY);
        totalBytes += bytesRead;
    }

    i2s_driver_uninstall(I2S_NUM_0);
    Serial.println("🎤 Grabación completada.");

    return totalBytes;
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\n✅ WiFi conectado!");

    client.setInsecure();  // 🔹 Deshabilita verificación SSL

    uint8_t* audioBuffer = (uint8_t*)malloc(AUDIO_BUFFER_SIZE);
    if (!audioBuffer) {
        Serial.println("❌ Error: No se pudo asignar memoria para el buffer de audio.");
        return;
    }

    Serial.println("🎤 Grabando audio...");
    size_t recordedSize = recordAudio(audioBuffer, AUDIO_BUFFER_SIZE);
    Serial.printf("📏 Tamaño del audio grabado: %d bytes\n", recordedSize);

    if (recordedSize > 0) {
        Serial.println("📤 Subiendo audio a Firebase...");
        String fileUrl = uploadAudioToFirebase(audioBuffer, recordedSize);
        
        if (fileUrl != "") {
            Serial.println("✅ Audio subido a Firebase!");
            Serial.println("📡 URL: " + fileUrl);
            sendToGoogleSpeech(fileUrl);
        } else {
            Serial.println("❌ Error al subir audio.");
        }
    } else {
        Serial.println("⚠️ No se grabó audio válido.");
    }

    free(audioBuffer);
}

String uploadAudioToFirebase(uint8_t *audioData, size_t dataSize) {
    String fileName = "voice-recognition/audio_" + String(millis()) + ".wav";
    String encodedFileName = URLEncoder.encode(fileName);  // 🔹 Codifica el nombre del archivo
    String url = "https://firebasestorage.googleapis.com/v0/b/" + 
                 String(FIREBASE_STORAGE_BUCKET) + 
                 "/o?uploadType=media&name=" + encodedFileName;

    Serial.println("📤 URL de subida: " + url);

    http.begin(client, url);
    http.addHeader("Content-Type", "audio/wav");
    
    int httpResponseCode = http.POST(audioData, dataSize);
    String response = http.getString();
    http.end();

    Serial.printf("📡 Código de respuesta Firebase: %d\n", httpResponseCode);
    Serial.println("📡 Respuesta Firebase:");
    Serial.println(response);  // 📌 Imprime la respuesta completa para depuración

    if (httpResponseCode == 200) {
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);
        String name = doc["name"].as<String>();
        return "https://firebasestorage.googleapis.com/v0/b/" + 
               String(FIREBASE_STORAGE_BUCKET) + "/o/" + 
               URLEncoder.encode(name) + "?alt=media";
    }
    
    return "";
}

void sendToGoogleSpeech(String fileUrl) {
    String apiKey = "AIzaSyBXDJudi3ejfHkBNKxSqpAYfZADj8Iq8Zw";  // 🔹 API Key de Google Speech
    String url = "https://speech.googleapis.com/v1/speech:recognize?key=" + apiKey;
    
    String jsonPayload = "{" 
                         "\"config\": {" 
                         "\"encoding\":\"LINEAR16\"," 
                         "\"sampleRateHertz\":16000," 
                         "\"languageCode\":\"es-ES\"" 
                         "}," 
                         "\"audio\": {\"uri\":\"" + fileUrl + "\"}" 
                         "}";

    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(jsonPayload);

    Serial.printf("📡 Código de respuesta Google Speech: %d\n", httpResponseCode);
    if (httpResponseCode > 0) {
        Serial.println("📡 Respuesta de Google:");
        Serial.println(http.getString());
    } else {
        Serial.println("❌ Error en la solicitud a Google.");
    }

    http.end();
}


void loop() {
}
