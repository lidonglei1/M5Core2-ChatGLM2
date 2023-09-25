import logging
import pvporcupine
from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from setting import *
from common_utils import *

logger = logging.getLogger(__name__)


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
        # returns -1 if no keyword is detected
        keyword_index = self.porcupine.process(pcm)
        return keyword_index

    def predict_detail(self, pcm):
        keyword_index = self.predict(pcm)
        logger.debug("Porcupine predict result index: {}".format(keyword_index))

        if keyword_index >= 0:
            keyword = keywords_list[keyword_index]
        else:
            keyword = "NONE"
        return keyword


class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []
        self._porcupine: PorcupinePredictor = None

        self.frame_cnt = 0
        self.frame_bytes = bytes()

    @property
    def porcupine(self):
        return self._porcupine

    @porcupine.setter
    def porcupine(self, predictor_instance):
        # 可以在 setter 方法中添加额外的逻辑
        self._porcupine = predictor_instance

    def reset_frame_cnt(self):
        self.frame_cnt = 0
        self.frame_bytes = bytes()

    def increment_frame_cnt(self, sub_bytes):
        self.frame_cnt += 1
        self.frame_bytes += sub_bytes

    def check_frame_cnt(self):
        return self.frame_cnt >= total_frame_num

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)

    async def process_audio_bytes(self, pcm_bytes: bytes, websocket: WebSocket):
        logger.info("Receive audio bytes from websocket.")

        if enable_recording and self.check_frame_cnt():
            wav_filename = generate_filename()
            save_as_wav(self.frame_bytes, wav_filename, sample_rate, num_channels, sample_width)
            logger.debug("Save pcm audio to {}".format(wav_filename))

            self.reset_frame_cnt()
        else:
            self.increment_frame_cnt(pcm_bytes)

        pcm_list = bytes_to_int16_list_np(pcm_bytes).tolist()
        result = self.porcupine.predict_detail(pcm_list)
        logger.info("Detected: {}".format(result))
        print("Detected: {}".format(result))
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
