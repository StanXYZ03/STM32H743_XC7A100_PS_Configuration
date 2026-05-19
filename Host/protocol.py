import struct
from dataclasses import dataclass
from typing import Optional


@dataclass
class ParsedData:
    """解析后的数据"""
    mouse_buttons: int      # 0-7, bit0=左键, bit1=右键, bit2=中键
    wheel_delta: int        # 正=向上, 负=向下, 0=无滚动
    x: int                  # 鼠标X位移
    y: int                  # 鼠标Y位移
    modifier: int           # 键盘修饰键
    keys: list              # 6个普通键的HID码列表 (0表示未按下)
    raw_bytes: bytes        # 原始数据


def parse_packet(data: bytes) -> Optional[ParsedData]:
    """
    解析一帧数据
    返回 ParsedData 或 None(解析失败)
    """
    PACKET_LEN = 14
    HEADER = 0x55

    if len(data) < PACKET_LEN:
        return None
    if data[0] != HEADER:
        return None

    # 无校验位，14字节均为数据: [0]包头 [1]鼠标 [2]滚轮 [3-4]X [5-6]Y [7]modifier [8-13]keys

    b1 = data[1]
    # 鼠标按键: bit0=左键(1), bit1=右键(2), bit2=中键/滚轮按下(4)
    mouse_buttons = b1 & 0x07

    # 滚轮: 独立在data[2]，1=向上, 0xFF=向下
    wheel_delta = 0
    wheel_byte = data[2]
    if wheel_byte == 0x01:
        wheel_delta = 1    # 滚轮向上
    elif wheel_byte == 0xFF:
        wheel_delta = -1   # 滚轮向下

    # X, Y 有符号16位小端 (data[3-6])
    x = struct.unpack("<h", data[3:5])[0]
    y = struct.unpack("<h", data[5:7])[0]

    modifier = data[7]
    keys = [data[i] for i in range(8, 14)]

    return ParsedData(
        mouse_buttons=mouse_buttons,
        wheel_delta=wheel_delta,
        x=x, y=y,
        modifier=modifier,
        keys=keys,
        raw_bytes=data[:14],
    )