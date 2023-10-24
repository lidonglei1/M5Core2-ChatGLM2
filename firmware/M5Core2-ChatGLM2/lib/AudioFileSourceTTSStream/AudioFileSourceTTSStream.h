#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <base64.h>
#include "AudioFileSource.h"
#include "DataStream.h"

class AudioFileSourceTTSStream : public AudioFileSource {
    public:
        friend class AudioFileSourceICYStream;

        AudioFileSourceTTSStream();
        AudioFileSourceTTSStream(DataStream *streamBuffer);
        virtual ~AudioFileSourceTTSStream() override;

        virtual bool open(const char *url) override;
        virtual uint32_t read(void *data, uint32_t len) override;
        virtual uint32_t readNonBlock(void *data, uint32_t len) override;
        virtual bool seek(int32_t pos, int dir) override;
        virtual bool close() override;
        virtual bool isOpen() override;
        virtual uint32_t getSize() override;
        virtual uint32_t getPos() override;

    private:
        DataStream *dataBuffer;

        virtual uint32_t readInternal(void *data, uint32_t len, bool nonBlock);
        int pos;
        int size;
};
