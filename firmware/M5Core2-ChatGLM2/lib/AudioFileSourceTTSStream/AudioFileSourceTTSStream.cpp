#include <AudioFileSourceTTSStream.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>

#define USE_SERIAL Serial1

const char* server = "tts-api.xfyun.cn";
const int port = 443;
const char* path = "/v2/tts";
const char* apiKey = "your_API_KEY";
const char* apiSecret = "your_API_SECRET";
const char* text = "这是一个语音合成示例";
const char* webSocketServer = "wss://your_websocket_server";


class WebSocket {
public:
  WebSocketsClient websocketsClient;

public:
  void open(String host, uint16_t port, String url) {
    websocketsClient.begin(host, port, url);
    websocketsClient.onEvent([this](WStype_t type, uint8_t *payload, size_t length){
      webSocketEvent(type, payload, length);
    });
  }

  void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
      case WStype_DISCONNECTED:
        USE_SERIAL.printf("[WSc] Disconnected!\n");
        break;
      case WStype_CONNECTED:
        USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);

        // send message to server when Connected
        websocketsClient.sendTXT("Connected");
        break;
      case WStype_TEXT:
        USE_SERIAL.printf("[WSc] get text: %s\n", payload);

        // send message to server
        // webSocket.sendTXT("message here");
        break;
      case WStype_BIN:
        USE_SERIAL.printf("[WSc] get binary length: %u\n", length);

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

  String createUrl(const char *tts_text) {
    return "";
  }
};

class DataStream {
private:
    std::vector<uint8_t> dataStream;  // 数据流存储

public:
    void writeData(const uint8_t* data, size_t size) {
        dataStream.insert(dataStream.end(), data, data + size);
    }

    size_t available() const {
        return dataStream.size();
    }

    void read(uint8_t* buf, size_t size) {
        if (size > dataStream.size()) {
            USE_SERIAL.printf("Error: Not enough data available in the stream.");
            return;
        }

        memcpy(buf, dataStream.data(), size);
        dataStream.erase(dataStream.begin(), dataStream.begin() + size);
    }
};

AudioFileSourceTTSStream::AudioFileSourceTTSStream()
{
  pos = 0;
  reconnectTries = 0;
  saveURL[0] = 0;
}

AudioFileSourceTTSStream::AudioFileSourceTTSStream(const char *tts_text, const char *tts_params)
{
  saveURL[0] = 0;
  reconnectTries = 0;

  String url = websocket->createUrl(tts_text);
  websocket->open(server, port, url);
}

