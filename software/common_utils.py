import wave
import struct
import datetime
import numpy as np
from typing import List


def generate_filename(file_extension='wav'):
    """根据后缀生成基于时间戳的文件名"""
    current_time = datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
    filename = '.'.join([current_time, file_extension])
    return filename


def save_as_wav(audio_data, filename, sample_rate, num_channels, sample_width):
    with wave.open(filename, 'wb') as wav_file:
        wav_file.setnchannels(num_channels)
        wav_file.setsampwidth(sample_width)
        wav_file.setframerate(sample_rate)
        wav_file.writeframes(audio_data)


def bytes_to_int16_list(byte_data) -> List[int]:
    int_list = []
    for i in range(0, len(byte_data), 2):  # 每两个字节为一组进行处理
        sample_bytes = byte_data[i:i+2]
        sample_value = struct.unpack('<h', sample_bytes)[0]  # 使用小端字节序解码为 int16 值
        int_list.append(sample_value)
    return int_list


def bytes_to_int16_list_np(byte_data) -> np.ndarray:
    int_array = np.frombuffer(byte_data, dtype=np.int16)
    return int_array


if __name__ == '__main__':
    # unit-test: filename generation
    print("Auto-generated timestamp based filename: {}".format(generate_filename("wav")))

    # unit-test: bytes to List[int]
    test_bytes = b'this is an testing bytes-array'
    print(bytes_to_int16_list(test_bytes))
    print(bytes_to_int16_list_np(test_bytes).tolist() == bytes_to_int16_list(test_bytes))
