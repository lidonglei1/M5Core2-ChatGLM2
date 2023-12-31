import asyncio
import websockets
import pyaudio
import numpy as np
from time import sleep


async def websocket_client():
    uri = "ws://192.168.0.109:8000/ws/1"  # WebSocket 服务器的地址
    async with websockets.connect(uri) as websocket:
        print("WebSocket 连接已建立")

        # 初始化录音
        CHUNK = 512
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

                # 发送音频数据到 WebSocket 服务器
                await websocket.send(data)
                print("发送音频数据:{} 长度:{}".format(data, len(data)))

                # 接收服务器返回的数据
                response = await websocket.recv()
                print("接收到服务器返回的数据: {}".format(response))

            except websockets.ConnectionClosedOK:
                break

        # 关闭录音流和 WebSocket 连接
        stream.stop_stream()
        stream.close()
        audio.terminate()


asyncio.get_event_loop().run_until_complete(websocket_client())
