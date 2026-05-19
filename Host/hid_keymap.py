# -*- coding: utf-8 -*-
"""
USB HID 键盘码值映射表 - 基于 USB HID Usage Tables
参考: https://www.usb.org/hid
"""

# USB HID 键码 -> 显示标签 (主键/无Shift)
HID_KEY_MAP = {
    0x00: "",
    0x01: "ErrRoll",
    0x02: "POSTFail",
    0x03: "ErrUndef",
    0x04: "A",
    0x05: "B",
    0x06: "C",
    0x07: "D",
    0x08: "E",
    0x09: "F",
    0x0A: "G",
    0x0B: "H",
    0x0C: "I",
    0x0D: "J",
    0x0E: "K",
    0x0F: "L",
    0x10: "M",
    0x11: "N",
    0x12: "O",
    0x13: "P",
    0x14: "Q",
    0x15: "R",
    0x16: "S",
    0x17: "T",
    0x18: "U",
    0x19: "V",
    0x1A: "W",
    0x1B: "X",
    0x1C: "Y",
    0x1D: "Z",
    0x1E: "1",
    0x1F: "2",
    0x20: "3",
    0x21: "4",
    0x22: "5",
    0x23: "6",
    0x24: "7",
    0x25: "8",
    0x26: "9",
    0x27: "0",
    0x28: "Enter",
    0x29: "Esc",
    0x2A: "Bksp",
    0x2B: "Tab",
    0x2C: "Space",
    0x2D: "-",
    0x2E: "=",
    0x2F: "[",
    0x30: "]",
    0x31: "\\",
    0x32: "#",
    0x33: ";",
    0x34: "'",
    0x35: "`",
    0x36: ",",
    0x37: ".",
    0x38: "/",
    0x39: "Caps",
    0x3A: "F1",
    0x3B: "F2",
    0x3C: "F3",
    0x3D: "F4",
    0x3E: "F5",
    0x3F: "F6",
    0x40: "F7",
    0x41: "F8",
    0x42: "F9",
    0x43: "F10",
    0x44: "F11",
    0x45: "F12",
    0x46: "PrtSc",
    0x47: "ScrLk",
    0x48: "Pause",
    0x49: "Ins",
    0x4A: "Home",
    0x4B: "PgUp",
    0x4C: "Del",
    0x4D: "End",
    0x4E: "PgDn",
    0x4F: "→",
    0x50: "←",
    0x51: "↓",
    0x52: "↑",
    0x53: "Num",
    0x54: "KP/",
    0x55: "KP*",
    0x56: "KP-",
    0x57: "KP+",
    0x58: "KPEnter",
    0x59: "KP1",
    0x5A: "KP2",
    0x5B: "KP3",
    0x5C: "KP4",
    0x5D: "KP5",
    0x5E: "KP6",
    0x5F: "KP7",
    0x60: "KP8",
    0x61: "KP9",
    0x62: "KP0",
    0x63: "KP.",
    0x64: "\\|",
    0x65: "Menu",
    0xE0: "LCtrl",
    0xE1: "LShift",
    0xE2: "LAlt",
    0xE3: "LWin",
    0xE4: "RCtrl",
    0xE5: "RShift",
    0xE6: "RAlt",
    0xE7: "RWin",
}


# 修饰键位定义 (Modifier byte: bit0-7)
MODIFIER_BITS = {
    0: 0xE0,  # Left Ctrl
    1: 0xE1,  # Left Shift
    2: 0xE2,  # Left Alt
    3: 0xE3,  # Left GUI
    4: 0xE4,  # Right Ctrl
    5: 0xE5,  # Right Shift
    6: 0xE6,  # Right Alt
    7: 0xE7,  # Right GUI
}


def get_key_label(hid_code: int) -> str:
    """根据USB HID键码获取显示标签"""
    return HID_KEY_MAP.get(hid_code, f"0x{hid_code:02X}")


def get_modifier_keys(mod_byte: int) -> list:
    """从修饰字节解析按下的修饰键HID码列表"""
    keys = []
    for bit, hid_code in MODIFIER_BITS.items():
        if (mod_byte >> bit) & 1:
            keys.append(hid_code)
    return keys
