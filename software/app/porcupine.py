import logging
import pvporcupine
from typing import List
from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from setting import *
from common_utils import *

logger = logging.getLogger("root")


class PorcupinePredictor:
    def __init__(self):
        self.porcupine = pvporcupine.create(
            access_key=access_key,
            keyword_paths=keyword_paths_list,
            model_path=model_path
        )

    def predict(self, pcm: List[int]) -> int:
        """
        The incoming audio needs to have a sample rate equal to `.sample_rate` and be 16-bit
        linearly-encoded. Porcupine operates on single-channel audio.
        """
        print("1")
        # returns -1 if no keyword is detected
        print(self.porcupine.frame_length, len(pcm))
        keyword_index = self.porcupine.process(pcm)
        print("2")
        return keyword_index

    def predict_detail(self, pcm):
        keyword_index = self.predict(pcm)
        if logger.isEnabledFor(logging.DEBUG):
            logger.debug("Porcupine predict result index: {}".format(keyword_index))
        logger.debug("Porcupine predict result index: {}".format(keyword_index))

        if keyword_index >= 0:
            keyword = keywords_list[keyword_index]
        else:
            keyword = None
        return keyword


class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []
        self._porcupine: PorcupinePredictor = None

    @property
    def porcupine(self):
        return self._porcupine

    @porcupine.setter
    def porcupine(self, predictor_instance):
        # 可以在 setter 方法中添加额外的逻辑
        self._porcupine = predictor_instance

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)

    async def process_audio_bytes(self, pcm_bytes: bytes, websocket: WebSocket):
        if logger.isEnabledFor(logging.DEBUG):
            logger.debug("Receive audio bytes from websocket.")

        # if enable_recording:
        #     wav_filename = generate_filename()
        #     save_as_wav(pcm_bytes, wav_filename, sample_rate, num_channels, sample_width)
        #     logger.debug("Save pcm audio to {}".format(wav_filename))

        pcm_list = bytes_to_int16_list_np(pcm_bytes).tolist()
        print(pcm_list[:10])
        result = self.porcupine.predict_detail(pcm_list)
        if result is not None:
            await websocket.send_text(result)

    async def broadcast(self, message: str):
        for connection in self.active_connections:
            await connection.send_text(message)


router = APIRouter()
manager = ConnectionManager()
manager.porcupine = PorcupinePredictor()


@router.websocket("/ws/{client_id}")
async def websocket_endpoint(websocket: WebSocket, client_id: int):
    await manager.connect(websocket)
    logger.info("Establish a new connection, websocket client id is {}".format(client_id))
    try:
        while True:
            data = await websocket.receive_bytes()
            await manager.process_audio_bytes(data, websocket)
    except WebSocketDisconnect:
        manager.disconnect(websocket)
        logger.info("Connection closed, client ID is {}".format(client_id))
