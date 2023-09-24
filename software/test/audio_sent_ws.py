"""
音频拼接部分不合理导致识别效果不佳。仅为验证数据格式和结构，请勿使用
"""

import asyncio
import websockets
import pyaudio
import webrtcvad
import numpy as np

USING_NUMPY = False
PORCUPINE_CHUNK = 512
WEBRTCVAD_CHUNK = 480
PORCUPINE_CHUNK_LENGTH = PORCUPINE_CHUNK * 2

audio_bytes_cache = bytes()


async def websocket_client():
    global audio_bytes_cache

    # uri = "ws://192.168.0.115:8000/ws/1"  # WebSocket 服务器的地址
    uri = "ws://localhost:8000/ws/1"
    async with websockets.connect(uri) as websocket:
        print("WebSocket 连接已建立")

        # 初始化录音
        CHUNK = WEBRTCVAD_CHUNK
        FORMAT = pyaudio.paInt16
        CHANNELS = 1
        RATE = 16000
        audio = pyaudio.PyAudio()
        stream = audio.open(format=FORMAT, channels=CHANNELS,
                            rate=RATE, input=True,
                            frames_per_buffer=CHUNK)

        # 初始化 webrtcvad 实例
        vad = webrtcvad.Vad()
        vad.set_mode(3)  # 设置 VAD 的敏感度级别，取值范围为0-3，2最敏感

        # 循环录音并发送
        while True:
            try:
                # 从麦克风读取音频数据
                data = stream.read(WEBRTCVAD_CHUNK)

                # 判断音频是否包含语音活动
                if vad.is_speech(data, RATE):
                    audio_bytes_cache += data

                    # 发送音频数据到 WebSocket 服务器
                    if len(audio_bytes_cache) >= PORCUPINE_CHUNK_LENGTH:
                        if USING_NUMPY:  # 效果无差异
                            audio_data = np.frombuffer(audio_bytes_cache[:PORCUPINE_CHUNK_LENGTH], dtype=np.int16)
                            await websocket.send(audio_data.tobytes())
                        else:
                            await websocket.send(audio_bytes_cache[:PORCUPINE_CHUNK_LENGTH])

                        audio_bytes_cache = audio_bytes_cache[PORCUPINE_CHUNK_LENGTH:]
                        print(len(audio_bytes_cache))

            except websockets.ConnectionClosedOK:
                break

        # 关闭录音流和 WebSocket 连接
        stream.stop_stream()
        stream.close()
        audio.terminate()


asyncio.get_event_loop().run_until_complete(websocket_client())
