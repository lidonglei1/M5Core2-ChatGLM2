#include <Arduino.h>
#include <SD.h>
#include <SPIFFS.h>
#include <M5Unified.h>
#include <nvs.h>
#include <Avatar.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

#include <AudioOutput.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>

#include <deque>
#include <base64.h>
#include <ArduinoJson.h>

#include "ChatGLM.h"
#include "tts_api.h"
#include "AudioOutputM5Speaker.h"
#include "app_sr.h"

#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
bool audioEndFlag;

bool isReacting;
bool isRandomBehaving;
unsigned long lastInterfacingTime;

// queue
QueueHandle_t queueSample;
SemaphoreHandle_t xSemaphore;

// instances
Avatar avatar;
ChatGLM chatglm;
AsyncWebServer server(80);
static AudioOutputM5Speaker out(&M5.Speaker, m5spk_virtual_channel);

// task handlers
TaskHandle_t DisplayTaskHandle;
TaskHandle_t SampleAudioTaskHandle;
TaskHandle_t OnlineWakeupTaskHandle;
TaskHandle_t SpeechInteractionTaskHandle;
TaskHandle_t MovementTaskHandle;
TaskHandle_t RandomBehaviorTaskHandle;

// Function Declaration
void displayTask(void * parameter);
void sampleAudioTask(void * parameter);
void onlineWakeupTask(void * parameter);
void speechInteractionTask(void * parameter);
void movementTask(void * parameter);
void randomBehaviorTask(void * parameter);

// Initialization Routine
void setup() {
  setupM5Box();
  startWifi();

  ESP_LOGI(TAG, "speech recognition start");
  app_sr_start(false);
  
  // create task
  xTaskCreatePinnedToCore(displayTask, "DisplayTask",  10000,  NULL,  1, &DisplayTaskHandle, 1);
  xTaskCreatePinnedToCore(sampleAudioTask, "SampleAudioTask",  10000,  NULL,  1, &SampleAudioTaskHandle, 1);
  xTaskCreatePinnedToCore(onlineWakeupTask, "OnlineWakeupTask",  4000,  NULL,  1, &OnlineWakeupTaskHandle, 1);
  xTaskCreatePinnedToCore(speechInteractionTask, "SpeechInteractionTask",  4000,  NULL,  1, &SpeechInteractionTaskHandle, 1);
  xTaskCreatePinnedToCore(movementTask, "MovementTask",  4000,  NULL,  1, &MovementTaskHandle, 1);
  xTaskCreatePinnedToCore(randomBehaviorTask, "RandomBehaviorTask",  4000,  NULL,  1, &RandomBehaviorTaskHandle, 1);
  avatar.addTask(c, "lipSync");

  Serial.println("Finish Setuped.");
}

// Main Program
void loop() {
  // 

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
}

static void audio_play_finish_cb(void)
{
    ESP_LOGI(TAG, "replay audio end");
    if (audioEndFlag) {
        audioEndFlag = true;
    }
}

void displayTask(void * parameter) {
  while (true) {
    
  }
  vTaskDelete(DisplayTaskHandle);
}

void sampleAudioTask(void * parameter) {
  while (true) {
    
  }
  vTaskDelete(SampleAudioTaskHandle);
}

void onlineWakeupTask(void * parameter) {
  while (true) {

  }
  vTaskDelete(OnlineWakeupTaskHandle);
}

void speechInteractionTask(void * parameter) {
  while (true) {

  }
  vTaskDelete(SpeechInteractionTaskHandle);
}

void movementTask(void * parameter) {
  while (true) {

  }
  vTaskDelete(MovementTaskHandle);
}

void randomBehaviorTask(void * parameter) {
  while (true) {

  }
  vTaskDelete(RandomBehaviorTaskHandle);
}

/**
 * https://github.com/robo8080/M5Unified_StackChan_ChatGPT
*/
void lipSync(void *args)
{
  float gazeX, gazeY;
  int level = 0;
  DriveContext *ctx = (DriveContext *)args;
  Avatar *avatar = ctx->getAvatar();
  for (;;)
  {
    level = abs(*out.getBuffer());
    if(level<100) level = 0;
    if(level > 15000)
    {
      level = 15000;
    }
    float open = (float)level/15000.0;
    avatar->setMouthOpenRatio(open);
    avatar->getGaze(&gazeY, &gazeX);
    avatar->setRotation(gazeX * 5);
    delay(50);
  }
}