#include <AudioFileSourceTTSStream.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>

AudioFileSourceTTSStream::AudioFileSourceTTSStream()
{
  pos = 0;
  reconnectTries = 0;
  saveURL[0] = 0;
}

AudioFileSourceTTSStream::AudioFileSourceTTSStream(DataStream *streamBuffer)
{
  saveURL[0] = 0;
  reconnectTries = 0;

  dataBuffer = streamBuffer;
}

AudioFileSourceTTSStream::~AudioFileSourceTTSStream()
{
}



