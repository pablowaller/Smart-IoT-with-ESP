#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "driver/i2s.h"
#include "mbedtls/base64.h"

#define WIFI_SSID "Luna 2.4"
#define WIFI_PASSWORD "Grecia2607"
#define STORAGE_BUCKET "sense-bell.appspot.com"
#define SAMPLE_RATE 16000
#define RECORD_TIME 10  // Segundos
#define BITS_PER_SAMPLE 16
#define NUM_CHANNELS 1
#define PIN_I2S_BCLK 26
#define PIN_I2S_LRC 22
#define PIN_I2S_DIN 35
#define AUDIO_BUFFER_SIZE (SAMPLE_RATE * (BITS_PER_SAMPLE / 8) * RECORD_TIME)  // Tamaño en bytes

WiFiClientSecure client;
HTTPClient http;

void setupI2S() {
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
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_I2S_DIN
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);
}

void createWavHeader(uint8_t *header, uint32_t audioSize) {
    uint32_t fileSize = audioSize + 36;
    
    // Cabecera WAV estándar
    memcpy(header, "RIFF", 4);
    memcpy(header + 4, &fileSize, 4);
    memcpy(header + 8, "WAVE", 4);
    memcpy(header + 12, "fmt ", 4);
    
    uint32_t subChunk1Size = 16;
    uint16_t audioFormat = 1;
    uint16_t numChannels = NUM_CHANNELS;
    uint32_t sampleRate = SAMPLE_RATE;
    uint32_t byteRate = SAMPLE_RATE * numChannels * (BITS_PER_SAMPLE / 8);
    uint16_t blockAlign = numChannels * (BITS_PER_SAMPLE / 8);
    uint16_t bitsPerSample = BITS_PER_SAMPLE;
    
    memcpy(header + 16, &subChunk1Size, 4);
    memcpy(header + 20, &audioFormat, 2);
    memcpy(header + 22, &numChannels, 2);
    memcpy(header + 24, &sampleRate, 4);
    memcpy(header + 28, &byteRate, 4);
    memcpy(header + 32, &blockAlign, 2);
    memcpy(header + 34, &bitsPerSample, 2);
    
    memcpy(header + 36, "data", 4);
    memcpy(header + 40, &audioSize, 4);
}

size_t recordAudio(uint8_t *audioBuffer, size_t bufferSize) {
    Serial.println("🎤 Grabando audio...");
    size_t bytesRead = 0, totalBytes = 0;
    while (totalBytes < bufferSize) {
        i2s_read(I2S_NUM_0, audioBuffer + totalBytes, bufferSize - totalBytes, &bytesRead, portMAX_DELAY);
        totalBytes += bytesRead;
    }
    Serial.println("✅ Grabación completada.");
    return totalBytes;
}

bool uploadToFirebase(uint8_t *wavData, size_t wavSize) {
    Serial.println("📤 Subiendo audio a Firebase...");

    // Convertir a Base64
    size_t encodedLength = 4 * ((wavSize + 2) / 3);
    uint8_t *base64Data = (uint8_t *)malloc(encodedLength);
    if (!base64Data) {
        Serial.println("❌ Error: No se pudo asignar memoria para Base64.");
        return false;
    }

    size_t actualEncodedSize;
    mbedtls_base64_encode(base64Data, encodedLength, &actualEncodedSize, wavData, wavSize);

    String base64String = String((char *)base64Data);
    free(base64Data);

    // Enviar a Firebase
    String url = "https://firebasestorage.googleapis.com/v0/b/" + String(STORAGE_BUCKET) + "/o/voice-recognition%2Frecord.wav?uploadType=media";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"file\":\"" + base64String + "\"}";
    int httpResponseCode = http.POST(jsonPayload);
    String response = http.getString();
    http.end();

    Serial.printf("📡 Respuesta Firebase: %d\n", httpResponseCode);
    Serial.println(response);

    return httpResponseCode == 200;
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }
    Serial.println("\n✅ WiFi conectado!");

    client.setInsecure();  // 🔹 Permite conexiones sin verificación SSL
    setupI2S();

    uint8_t *audioBuffer = (uint8_t *)malloc(AUDIO_BUFFER_SIZE);
    if (!audioBuffer) {
        Serial.println("❌ Error: No se pudo asignar memoria.");
        return;
    }

    size_t recordedSize = recordAudio(audioBuffer, AUDIO_BUFFER_SIZE);
    if (recordedSize > 0) {
        size_t wavSize = recordedSize + 44;  // Tamaño total incluyendo el encabezado WAV
        uint8_t *wavBuffer = (uint8_t *)malloc(wavSize);
        if (!wavBuffer) {
            Serial.println("❌ Error: No se pudo asignar memoria para WAV.");
            free(audioBuffer);
            return;
        }

        createWavHeader(wavBuffer, recordedSize);
        memcpy(wavBuffer + 44, audioBuffer, recordedSize);

        if (uploadToFirebase(wavBuffer, wavSize)) {
            Serial.println("✅ Audio subido exitosamente!");
        } else {
            Serial.println("❌ Error al subir audio.");
        }

        free(wavBuffer);
    } else {
        Serial.println("⚠️ No se grabó audio válido.");
    }

    free(audioBuffer);
}

void loop() {
}
