import asyncio
import websockets
import pyaudio
import numpy as np
from time import sleep


async def websocket_client():
    uri = "ws://localhost:8000/ws/1"  # WebSocket 服务器的地址
    async with websockets.connect(uri) as websocket:
        print("WebSocket 连接已建立")

        # 初始化录音
        CHUNK = 1024
        FORMAT = pyaudio.paInt16
        CHANNELS = 1
        RATE = 16000
        audio = pyaudio.PyAudio()
        stream = audio.open(format=FORMAT, channels=CHANNELS,
                            rate=RATE, input=True,
                            frames_per_buffer=CHUNK)

        # 循环录音并发送
        while True:
            try:
                # 从麦克风读取音频数据
                data = stream.read(CHUNK)
                audio_data = np.frombuffer(data, dtype=np.int16)

                # 发送音频数据到 WebSocket 服务器
                await websocket.send(audio_data.tobytes())
                print("发送音频数据:{} 长度:{}".format(audio_data, len(audio_data)))
                sleep(5)

            except websockets.ConnectionClosedOK:
                break

        # 关闭录音流和 WebSocket 连接
        stream.stop_stream()
        stream.close()
        audio.terminate()


asyncio.get_event_loop().run_until_complete(websocket_client())
