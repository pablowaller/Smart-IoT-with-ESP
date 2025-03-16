# 🚀 ESP32 + ESP8266 Management  

This repository contains code for an IoT system that combines:  
- 📷 **ESP32-CAM** for real-time video streaming and notice when someone touch the doorbell button.
- 📟 **Nodemcu ESP32** for voice recognition with Google Cloud Speech To Text API.
- 📡 **NodeMCU ESP8266** and **Wemos ESP8266**  to receive notifications from Firebase over WiFi and to energize a haptic motor.  

## 📌 Features  
✅ **Real-time video streaming** with ESP32-CAM.  
✅ **Built-in web server** to display video in a browser and then stream it on a mobile app.  
✅ **Flash LED control** to improve low-light streaming.  
✅ **Audio recording with Mems INMP441 Microphone**
✅ **Receive notifications** on ESP8266 via **Firebase Real Time Database (FRTDB)**.  
✅ **Compatible with mobile apps** using WebView to display the video feed.  

## 🛠️ Technologies and Hardware  
- **ESP32-CAM** 📷 (for video streaming). 
- **NodeMCU ESP8266** 📡 (to receive notifications via Firebase).
- **NodeMCU ESP32** 📟 (to record audio)
- **INMP441 I2S Omnidirectional Microphone** 🎙️
- **Firebase** 🔥.  
- **WiFi** 🌐 (to connect devices and services).  
