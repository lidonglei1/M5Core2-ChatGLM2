#include <Arduino.h>
#include <SD.h>
#include <SPIFFS.h>
#include <M5Unified.h>
#include <nvs.h>
#include <Avatar.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>
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
#define USE_SERIAL Serial1

// constants
const int MAX_TOKEN = 10;
const int preallocateBufferSize = 50*1024;
// set M5Speaker virtual channel (0-7)
static constexpr uint8_t m5spk_virtual_channel = 0;
static constexpr const size_t record_samplerate = 16000;
static constexpr const size_t record_size = record_samplerate * 5;  // 16000() * 16/2(Bytes) *5(s) = 160(KB)

// variables
static int16_t *rec_data;

std::deque<String> tokenHistory;
uint8_t *preallocateBuffer;
bool audioEndFlag;

bool isTask3 = false;
bool isReacting = false;
bool isRandomBehaving = false;
unsigned long lastInterfacingTime;

// queue
QueueHandle_t queueSample;
SemaphoreHandle_t xSemaphore;

// instances
Avatar avatar;
ChatGLM chatglm;
AsyncWebServer server(80);
WebSocketsClient webSocket;
AudioGeneratorMP3 *mp3;
AudioFileSourceBuffer *buff = nullptr;
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

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void hexdump(const void *mem, uint32_t len, uint8_t cols = 16);

// Initialization Routine
void setup() {
  setupM5Box();
  startWifi();

  mp3 = new AudioGeneratorMP3();

  rec_data = (typeof(rec_data))heap_caps_malloc(record_size * sizeof(int16_t), MALLOC_CAP_8BIT);  // TTS录音数据
  memset(rec_data, 0 , record_size * sizeof(int16_t));

  // ESP_LOGI(TAG, "speech recognition start");
  // app_sr_start(false);
  
  // create task
  xTaskCreatePinnedToCore(displayTask, "DisplayTask",  10000,  NULL,  1, &DisplayTaskHandle, 1);
  xTaskCreatePinnedToCore(sampleAudioTask, "SampleAudioTask",  10000,  NULL,  1, &SampleAudioTaskHandle, 1);
  xTaskCreatePinnedToCore(onlineWakeupTask, "OnlineWakeupTask",  4000,  NULL,  1, &OnlineWakeupTaskHandle, 1);
  xTaskCreatePinnedToCore(speechInteractionTask, "SpeechInteractionTask",  4000,  NULL,  1, &SpeechInteractionTaskHandle, 1);
  xTaskCreatePinnedToCore(movementTask, "MovementTask",  4000,  NULL,  1, &MovementTaskHandle, 1);
  xTaskCreatePinnedToCore(randomBehaviorTask, "RandomBehaviorTask",  4000,  NULL,  1, &RandomBehaviorTaskHandle, 1);
  avatar.addTask(lipSync, "lipSync");

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
    if (M5.Mic.isEnabled()) {
      if (!isTask3) {
        // record 1 sample and send
        recordOneSample();
        xQueueSend(queueSample, &sample16, 10);
      }
    }
  }
  vTaskDelete(SampleAudioTaskHandle);
}

void onlineWakeupTask(void * parameter) {
  while (true) {
    if (!isTask3) {
      xQueueReceive(queueSample, &element, portMAX_DELAY);
      webSocket.send(element);
    }
  }
  vTaskDelete(OnlineWakeupTaskHandle);
}

void speechInteractionTask(void * parameter) {
  while (true) {
    // Waiting for notification from Online-Wake-Up task
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    isTask3 = true;

    // do something


    // perform tts
    VoiceText_tts();

    isTask3 = false;
    xQueueReset(queueSample);

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

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

	switch(type) {
		case WStype_DISCONNECTED:
			USE_SERIAL.printf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED:
			USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);

			// send message to server when Connected
			webSocket.sendTXT("Connected");
			break;
		case WStype_TEXT:
			USE_SERIAL.printf("[WSc] get text: %s\n", payload);
      // wake up and start reacting
      if (strcmp((char*)payload, "xiaozhi") == 0) {

        xTaskNotifyGive(&SpeechInteractionTaskHandle);
      }

			// send message to server
			// webSocket.sendTXT("message here");
			break;
		case WStype_BIN:
			USE_SERIAL.printf("[WSc] get binary length: %u\n", length);
			hexdump(payload, length);

			// send data to server
			// webSocket.sendBIN(payload, length);
			break;
		case WStype_ERROR:			
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
			break;
	}
}

void hexdump(const void *mem, uint32_t len, uint8_t cols = 16) {
	const uint8_t* src = (const uint8_t*) mem;
	USE_SERIAL.printf("\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)", (ptrdiff_t)src, len, len);
	for(uint32_t i = 0; i < len; i++) {
		if(i % cols == 0) {
			USE_SERIAL.printf("\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
		}
		USE_SERIAL.printf("%02X ", *src);
		src++;
	}
	USE_SERIAL.printf("\n");
}

void VoiceText_tts(char *text,char *tts_parms) {
    file = new AudioFileSourceVoiceTextStream( text, tts_parms);
    buff = new AudioFileSourceBuffer(file, preallocateBuffer, preallocateBufferSize);
    mp3->begin(buff, &out);
}