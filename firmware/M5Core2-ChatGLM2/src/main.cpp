#include <Arduino.h>
#include <SD.h>
#include <SPIFFS.h>
#include <M5Unified.h>
#include <nvs.h>
#include <Avatar.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
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

#define BLOCK_SIZE        32         // 每次读取的采样数（帧数）
#define BLOCK_NUMS        (16000*6/BLOCK_SIZE)  // 总共需要读取的次数

// constants
const int MAX_MESSAGES = 10;
const int preallocateBufferSize = 50*1024;
const char KEY_WORD[] = "xiaozhi";
// set M5Speaker virtual channel (0-7)
static constexpr uint8_t m5spk_virtual_channel = 0;
static constexpr const size_t recordSampleRate = 16000;
static constexpr const size_t record_size = recordSampleRate * 5;  // 16000() * 16/2(Bytes) *5(s) = 160(KB)

const String openApiUrl = "http://192.168.0.115:8000/v1/chat/completions";

const int queueSampleSize = recordSampleRate * 2;

// variables
static int16_t *rec_data;
char subText[100] = "";
char resultASRText[500] = "";  // 存储ASR结果字符串，最大长度为 500
DynamicJsonDocument chatBufferDoc(1024*6);
JsonArray messages = chatBufferDoc.createNestedArray("messages");

std::deque<String> tokenHistory;
uint8_t *preallocateBuffer;
bool audioEndFlag;

bool isTask3 = false;
bool isReacting = false;
bool isRandomBehaving = false;
unsigned long lastInterfacingTime;

struct timeval currentTime;
struct timeval previousTime;

m5::mic_config_t micCfg;

// queue
QueueHandle_t queueSample;
QueueHandle_t queueText;
SemaphoreHandle_t xSemaphore;

// instances
Avatar avatar;
//ChatGLM chatglm;
HTTPClient http;
AsyncWebServer server(80);
WebSocketsClient webSocketVWU;
WebSocketsClient webSocketASR;
WebSocketsClient webSocketTTS;
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

void webSocketEventVWU(WStype_t type, uint8_t * payload, size_t length);
void webSocketEventASR(WStype_t type, uint8_t * payload, size_t length);
void webSocketEventTTS(WStype_t type, uint8_t * payload, size_t length);
void hexdump(const void *mem, uint32_t len, uint8_t cols = 16);

String chatglmRequest(char* asrText);
void voiceTextTTS(char *text,char *tts_parms);

void getServoAngle(int& angleYaw, int& anglePitch);

// Initialization Routine
void setup() {
  setupM5Box();
  startWifi();
  initChatGLMRequest();

  mp3 = new AudioGeneratorMP3();

  queueSample = xQueueCreate( queueSampleSize, sizeof( int16_t ) );

  rec_data = (typeof(rec_data))heap_caps_malloc(record_size * sizeof(int16_t), MALLOC_CAP_8BIT);  // TTS录音数据
  memset(rec_data, 0 , record_size * sizeof(int16_t));

  // ESP_LOGI(TAG, "speech recognition start");
  // app_sr_start(false);

  // websocket
	webSocketVWU.begin("192.168.0.115", 8000, "/ws/1");
	webSocketVWU.onEvent(webSocketEventVWU);
	//webSocket.setAuthorization("user", "Password"); // use HTTP Basic Authorization this is optional remove if not needed
	webSocketVWU.setReconnectInterval(5000);

  webSocketASR.begin("wss://ws-api.xfyun.cn", 443, "/v2/iat");
	webSocketASR.onEvent(webSocketEventASR);
	webSocketASR.setReconnectInterval(5000);

  webSocketTTS.begin("tts-api.xfyun.cn", 443, "/v2/tts");
	webSocketTTS.onEvent(webSocketEventTTS);
	webSocketTTS.setReconnectInterval(5000);
  
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

void initChatGLMRequest() {
  String initResponse = chatglmRequest("system", "/start");
  Serial.println("Response of Init-ChatGLM-Request: " + initResponse);
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
        // record a sample and send to queue
        int16_t sample16;
        size_t bytes_read = 0;
        if (i2s_read(micCfg.i2s_port, &sample16, sizeof(sample16), &bytes_read, portMAX_DELAY)){
          xQueueSend(queueSample, &sample16, 10);
        } else {
          Serial.println("The microphone recording has encountered an exception. Audio sampling failed!");
        }
      }
    }
  }
  vTaskDelete(SampleAudioTaskHandle);
}

void onlineWakeupTask(void * parameter) {
  while (true) {
    if (!isTask3) {
      int16_t element;
      xQueueReceive(queueSample, &element, portMAX_DELAY);
      webSocketVWU.sendTXT(reinterpret_cast<uint8_t*>(&element), sizeof(element));
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
    int16_t sample16;
    size_t bytesRead = 0;
    int16_t audioData[BLOCK_NUMS * BLOCK_SIZE];  // 存储音频数据的数组
    int audioDataIndex = 0;                       // 当前音频数据的索引
    uint16_t buffer[BLOCK_SIZE];

    // unsigned long timeElapse = 0;
    // unsigned long startTime = millis();

    size_t cnt = 0;
    while (cnt < BLOCK_NUMS/2 || (cnt < BLOCK_NUMS)) {  //vadPass() & 
      if(i2s_read(micCfg.i2s_port, buffer, BLOCK_SIZE * sizeof(uint16_t), &bytesRead, portMAX_DELAY)) {
        // 将读取的音频数据拷贝到全局数组中
        memcpy(&audioData[audioDataIndex], buffer, bytesRead);
        audioDataIndex += bytesRead / sizeof(int16_t);
      } else {
          Serial.println("The microphone recording has encountered an exception. Audio sampling failed!");
      }

      // timeElapse = millis() - startTime;
      cnt += 1;
    }
    // 将音频数据转换为 Base64 格式
    String pcmBase64Data = base64::encode((const uint8_t*)audioData, audioDataIndex * sizeof(int16_t) * 2);
    webSocketASR.sendTXT(pcmBase64Data);
    vTaskDelay(pdMS_TO_TICKS(50)); // Wait for the text being sent

    // perform tts
    while(strcmp(resultASRText, "") == 0) {
      break;
    }
    processQueueTextData();
    memset(resultASRText, 0, sizeof(resultASRText));

    String resultChatText = chatglmRequest(resultASRText);
    if (!resultChatText.isEmpty()) {
      webSocketTTS.sendTXT(resultChatText);
    }
    
    isTask3 = false;
    xQueueReset(queueSample);
  }
  vTaskDelete(SpeechInteractionTaskHandle);
}

void movementTask(void * parameter) {
  int yawAngle = 0;
  int pitchAngle = 0;
  while (true) {
    getServoAngle(yawAngle, pitchAngle);


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

void webSocketEventVWU(WStype_t type, uint8_t * payload, size_t length) {

	switch(type) {
		case WStype_DISCONNECTED:
			USE_SERIAL.printf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED:
			USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);

			// send message to server when Connected
			webSocketVWU.sendTXT("VWU Service Connected");
			break;
		case WStype_TEXT:
			USE_SERIAL.printf("[WSc] get text: %s\n", payload);
      // wake up and start reacting
      if (strcmp((char*)payload, KEY_WORD) == 0) {
        USE_SERIAL.printf("Keyword Detected: %s\n", KEY_WORD);

        xTaskNotifyGive(&SpeechInteractionTaskHandle);
      }

			// send message to server
			// webSocket.sendTXT("message here");
			break;
		case WStype_BIN:
			USE_SERIAL.printf("[WSc] get binary length: %u\n", length);
			hexdump(payload, length);
			break;
		case WStype_ERROR:			
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
			break;
	}
}

void webSocketEventASR(WStype_t type, uint8_t * payload, size_t length) {

	switch(type) {
		case WStype_DISCONNECTED:
			USE_SERIAL.printf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED:
			USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);
			break;
		case WStype_TEXT:
			USE_SERIAL.printf("[WSc] get text: %s\n", payload);
      // send the speech recognition result to the queue
      if (strcmp((char*)payload, KEY_WORD) == 0) {
        USE_SERIAL.printf("Detected: %s\n", KEY_WORD);

        xTaskNotifyGive(&SpeechInteractionTaskHandle);
      }
			break;
		case WStype_BIN:
			USE_SERIAL.printf("[WSc] get binary length: %u\n", length);
			hexdump(payload, length);
			break;
		case WStype_ERROR:			
		case WStype_FRAGMENT_TEXT_START:
		case WStype_FRAGMENT_BIN_START:
		case WStype_FRAGMENT:
		case WStype_FRAGMENT_FIN:
			break;
	}
}

void webSocketEventTTS(WStype_t type, uint8_t * payload, size_t length) {

	switch(type) {
		case WStype_DISCONNECTED:
			USE_SERIAL.printf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED:
			USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);
			break;
		case WStype_TEXT:
			USE_SERIAL.printf("[WSc] get text: %s\n", payload);
      // wake up and start reacting
      if (strcmp((char*)payload, KEY_WORD) == 0) {
        USE_SERIAL.printf("Detected: %s\n", KEY_WORD);

        xTaskNotifyGive(&SpeechInteractionTaskHandle);
      }
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

void addMessage(char* role, const char* content) {
  // 添加新消息
  JsonObject message = messages.createNestedObject();
  message["role"] = role;
  message["content"] = content;

  // 如果超过最大消息数量，删除第一个消息
  while (messages.size() > MAX_MESSAGES) {
    messages.remove(0);
  }
}

String chatglmRequest(char* role, char* asrText) {
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(1000);

  // 构建请求体和序列化
  String requestBody;
  addMessage(role, asrText);
  serializeJson(chatBufferDoc, requestBody);

  // 请求
  http.begin(openApiUrl);
  int httpResponseCode = http.POST(requestBody);

  String reply;
  if (httpResponseCode > 0) {
    String responseBody = http.getString();
    Serial.println(responseBody);
    
    // 解析响应
    DynamicJsonDocument jsonResult(2048);
    deserializeJson(jsonResult, responseBody);

    reply = jsonResult["choices"][0]["message"]["content"].as<String>();
    Serial.println(reply);
  } else {
    Serial.print("ChatGLM接口请求失败. Error code: ");
    Serial.println(httpResponseCode);

    reply = "";
  }

  // 释放资源
  http.end();

  return reply;
}

void voiceTextTTS(char *text,char *tts_parms) {
  // 修改为tts源
  file = new AudioFileSourceVoiceTextStream( text, tts_parms);
  buff = new AudioFileSourceBuffer(file, preallocateBuffer, preallocateBufferSize);
  mp3->begin(buff, &out);
}

void getExpectedServoAngle(int& angleYaw, int& anglePitch) {

}

void processQueueTextData()
{
    BaseType_t received;

    //while (1)
    while (uxQueueMessagesWaiting(queueText) > 0)
    {
        received = xQueueReceive(queueText, subText, pdMS_TO_TICKS(1000));

        if (received == pdTRUE) {
            strcat(resultASRText, subText);
        } else {
            break;  // 超时或队列为空，跳出循环
        }
    }

    printf("Concatenated text: %s\n", resultASRText);
}