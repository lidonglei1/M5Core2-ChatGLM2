#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <base64.h>
#include "AudioFileSource.h"

class WebSocket;
class DataStream;

class AudioFileSourceTTSStream : public AudioFileSource {
    public:
        friend class AudioFileSourceICYStream;

        AudioFileSourceTTSStream();
        AudioFileSourceTTSStream(const char *tts_text, const char *tts_params);
        virtual ~AudioFileSourceTTSStream() override;

        virtual bool open(const char *url) override;
        virtual uint32_t read(void *data, uint32_t len) override;
        virtual uint32_t readNonBlock(void *data, uint32_t len) override;
        virtual bool seek(int32_t pos, int dir) override;
        virtual bool close() override;
        virtual bool isOpen() override;
        virtual uint32_t getSize() override;
        virtual uint32_t getPos() override;
        bool SetReconnect(int tries, int delayms) { reconnectTries = tries; reconnectDelayMs = delayms; return true; }

    private:
        WebSocket *websocket;
        DataStream *datastream;

        virtual uint32_t readInternal(void *data, uint32_t len, bool nonBlock);
        int pos;
        int size;
        int reconnectTries;
        int reconnectDelayMs;
        char saveURL[128];
};
