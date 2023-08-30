#include <Arduino.h>
#include <SD.h>
#include <SPIFFS.h>
#include <M5Unified.h>
#include <nvs.h>
#include <Avatar.h>
#include <ESP32WebServer.h>

#include <deque>
#include <ArduinoJson.h>

#include "ChatGLM.h"
#include "tts_api.h"

using namespace m5avatar;

#define WIFI_SSID "LeeSophia"
#define WIFI_PASS "xxxxxx"
#define ARDUINO_M5STACK_Core2

// constants
const int MAX_TOKEN = 10;
const int preallocateBufferSize = 50*1024;
// set M5Speaker virtual channel (0-7)
static constexpr uint8_t m5spk_virtual_channel = 0;

// variables
std::deque<String> tokenHistory;
uint8_t *preallocateBuffer;

// instances
Avatar avatar;
ChatGLM chatglm;
ESP32WebServer server(80);

void setup() {
  setupM5Box();
  startWifi();
  
}

void loop() {
  // put your main code here, to run repeatedly:
}

// put function definitions here:
void startWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
}

void setupM5Box() {

  { // global setting
    auto cfg = M5.config();
    cfg.external_spk = true;
    M5.begin(cfg);
  }

  { // speaker setting
    auto spk_cfg = M5.Speaker.config();
    // Increasing the sample_rate will improve the sound quality instead of increasing the CPU load.
    spk_cfg.sample_rate = 96000; // default:64000 (64kHz)  e.g. 48000 , 50000 , 80000 , 96000 , 100000 , 128000 , 144000 , 192000 , 200000
    M5.Speaker.config(spk_cfg);
    M5.Speaker.begin();
  }


{

}
}