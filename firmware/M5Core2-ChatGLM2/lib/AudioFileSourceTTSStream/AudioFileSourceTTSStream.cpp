#include <AudioFileSourceTTSStream.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>

AudioFileSourceTTSStream::AudioFileSourceTTSStream()
{
  pos = 0;
}

AudioFileSourceTTSStream::AudioFileSourceTTSStream(DataStream *streamBuffer)
{
  dataBuffer = streamBuffer;
}

AudioFileSourceTTSStream::~AudioFileSourceTTSStream()
{
}

bool AudioFileSourceTTSStream::open(const char *url)
{
  return dataBuffer != nullptr;
}

/*
** 返回读取到的字节数，当没有可用的数据或已经读到了末尾，则返回0
*/
uint32_t AudioFileSourceTTSStream::read(void *data, uint32_t len)
{
  int read = dataBuffer->read(reinterpret_cast<uint8_t*>(data), len);
  pos += read;
  return read;
}

bool AudioFileSourceTTSStream::seek(int32_t pos, int dir)
{
  audioLogger->printf_P(PSTR("ERROR! AudioFileSourceTTSStream::seek not implemented!"));
  (void) pos;
  (void) dir;
  return false;
}

bool AudioFileSourceTTSStream::close()
{
  return true;
}

bool AudioFileSourceTTSStream::isOpen()
{
  return true;
}

uint32_t AudioFileSourceTTSStream::getSize()
{
  return size;
}

uint32_t AudioFileSourceTTSStream::getPos()
{
  return pos;
}
