# -*- coding: utf-8 -*-
"""
FPGA-SPI 上位机 - 键盘鼠标可视化
串口接收STM32数据，解析并实时显示按键/鼠标状态
"""

import sys
import serial
import serial.tools.list_ports
from collections import deque
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QComboBox, QPushButton, QLabel, QFrame, QGraphicsScene, QGraphicsView,
    QGraphicsRectItem, QGraphicsTextItem, QGraphicsEllipseItem,
)
from PyQt6.QtCore import Qt, QTimer, QRectF, pyqtSignal, QObject
from PyQt6.QtGui import QPainter, QColor, QBrush, QPen, QFont, QKeyEvent

from protocol import parse_packet, ParsedData
from hid_keymap import get_modifier_keys
from keyboard_layout import KEYBOARD_ROWS



# 映射本地按键到USB HID码，用于本机控制
QT_TO_HID = {
    Qt.Key.Key_A: 0x04, Qt.Key.Key_B: 0x05, Qt.Key.Key_C: 0x06, Qt.Key.Key_D: 0x07,
    Qt.Key.Key_E: 0x08, Qt.Key.Key_F: 0x09, Qt.Key.Key_G: 0x0A, Qt.Key.Key_H: 0x0B,
    Qt.Key.Key_I: 0x0C, Qt.Key.Key_J: 0x0D, Qt.Key.Key_K: 0x0E, Qt.Key.Key_L: 0x0F,
    Qt.Key.Key_M: 0x10, Qt.Key.Key_N: 0x11, Qt.Key.Key_O: 0x12, Qt.Key.Key_P: 0x13,
    Qt.Key.Key_Q: 0x14, Qt.Key.Key_R: 0x15, Qt.Key.Key_S: 0x16, Qt.Key.Key_T: 0x17,
    Qt.Key.Key_U: 0x18, Qt.Key.Key_V: 0x19, Qt.Key.Key_W: 0x1A, Qt.Key.Key_X: 0x1B,
    Qt.Key.Key_Y: 0x1C, Qt.Key.Key_Z: 0x1D,
    Qt.Key.Key_1: 0x1E, Qt.Key.Key_2: 0x1F, Qt.Key.Key_3: 0x20, Qt.Key.Key_4: 0x21,
    Qt.Key.Key_5: 0x22, Qt.Key.Key_6: 0x23, Qt.Key.Key_7: 0x24, Qt.Key.Key_8: 0x25,
    Qt.Key.Key_9: 0x26, Qt.Key.Key_0: 0x27,
    Qt.Key.Key_Return: 0x28, Qt.Key.Key_Enter: 0x28, Qt.Key.Key_Escape: 0x29,
    Qt.Key.Key_Backspace: 0x2A, Qt.Key.Key_Tab: 0x2B, Qt.Key.Key_Space: 0x2C,
    Qt.Key.Key_Minus: 0x2D, Qt.Key.Key_Equal: 0x2E, Qt.Key.Key_BracketLeft: 0x2F,
    Qt.Key.Key_BracketRight: 0x30, Qt.Key.Key_Backslash: 0x31, Qt.Key.Key_Semicolon: 0x33,
    Qt.Key.Key_Apostrophe: 0x34, Qt.Key.Key_QuoteLeft: 0x35, Qt.Key.Key_Comma: 0x36,
    Qt.Key.Key_Period: 0x37, Qt.Key.Key_Slash: 0x38, Qt.Key.Key_CapsLock: 0x39,
    Qt.Key.Key_F1: 0x3A, Qt.Key.Key_F2: 0x3B, Qt.Key.Key_F3: 0x3C, Qt.Key.Key_F4: 0x3D,
    Qt.Key.Key_F5: 0x3E, Qt.Key.Key_F6: 0x3F, Qt.Key.Key_F7: 0x40, Qt.Key.Key_F8: 0x41,
    Qt.Key.Key_F9: 0x42, Qt.Key.Key_F10: 0x43, Qt.Key.Key_F11: 0x44, Qt.Key.Key_F12: 0x45,
    Qt.Key.Key_Home: 0x4A, Qt.Key.Key_PageUp: 0x4B, Qt.Key.Key_Delete: 0x4C,
    Qt.Key.Key_End: 0x4D, Qt.Key.Key_PageDown: 0x4E,
    Qt.Key.Key_Right: 0x4F, Qt.Key.Key_Left: 0x50, Qt.Key.Key_Down: 0x51, Qt.Key.Key_Up: 0x52,
    # 修饰键
    Qt.Key.Key_Control: 0xE0, Qt.Key.Key_Shift: 0xE1, Qt.Key.Key_Alt: 0xE2,
    Qt.Key.Key_Meta: 0xE3,
}

class SerialWorker(QObject):
    """串口读取线程 - 使用定时器轮询方式简化"""
    dataReceived = pyqtSignal(bytes)

    def __init__(self, port: str, baud: int = 115200):
        super().__init__()
        self.port = port
        self.baud = baud
        self.ser = None
        self.buffer = bytearray()

    def open(self) -> bool:
        try:
            # 8N1: 8bit数据位, 无校验, 1bit停止位, 115200
            self.ser = serial.Serial(
                self.port, self.baud,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.01
            )
            return True
        except Exception:
            return False

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.ser = None

    def read_available(self):
        if not self.ser or not self.ser.is_open:
            return
        try:
            data = self.ser.read(256)
            if data:
                self.buffer.extend(data)
                # 按帧解析，每帧14字节，支持头对齐与噪声剔除
                while len(self.buffer) >= 14:
                    # 找到帧头
                    if self.buffer[0] != 0x55:
                        idx = self.buffer.find(b"\x55")
                        if idx == -1:
                            self.buffer.clear()
                            break
                        else:
                            del self.buffer[:idx]
                            if len(self.buffer) < 14:
                                break
                    # 此时首字节是 0x55
                    frame = bytes(self.buffer[:14])
                    del self.buffer[:14]
                    self.dataReceived.emit(frame)
        except Exception:
            pass


class CoordinateMapWidget(QFrame):
    """1920x1080 等比例坐标映射窗口（Y轴向上为正）
    支持本地鼠标输入，用于在串口空闲时控制。
    可以通过 set_local_mode 切换点的颜色。"""
    def __init__(self, map_w: int = 1920, map_h: int = 1080):
        super().__init__()
        self.map_w = map_w
        self.map_h = map_h
        self.mouse_x = 0
        self.mouse_y = 0
        self.local_mode = False
        # 缓存最近一次计算的绘图区域，用于坐标反算
        self._inner_x = self._inner_y = 0
        self._inner_w = self._inner_h = 0
        self.setMinimumHeight(300)
        self.setStyleSheet("""
            CoordinateMapWidget {
                background: #2b2d42;
                border-radius: 8px;
                border: 2px solid #4a4e69;
            }
        """)
        # 启用鼠标追踪以获取 move 事件
        self.setMouseTracking(True)

    def set_local_mode(self, flag: bool):
        self.local_mode = flag
        self.update()

    def map_to_coords(self, px: int, py: int) -> tuple[int, int]:
        """将窗口坐标转换到 0..map_w-1 / 0..map_h-1"""
        if self._inner_w <= 1 or self._inner_h <= 1:
            return self.mouse_x, self.mouse_y
        # 限制在内部网格
        ix = px - self._inner_x
        iy = py - self._inner_y
        if ix < 0 or iy < 0 or ix > self._inner_w or iy > self._inner_h:
            return self.mouse_x, self.mouse_y
        x = int(ix * (self.map_w - 1) / max(1, self._inner_w - 1))
        # Y 轴翻转
        y = int((self._inner_h - iy) * (self.map_h - 1) / max(1, self._inner_h - 1))
        return x, y

    def mouseMoveEvent(self, event):
        win = self.window()
        if getattr(win, "allow_local", False) and getattr(win, "local_enabled", False):
            pos = event.position()
            x, y = self.map_to_coords(int(pos.x()), int(pos.y()))
            win.set_local_mouse_pos(x, y)
        super().mouseMoveEvent(event)

    def mousePressEvent(self, event):
        win = self.window()
        if getattr(win, "allow_local", False):
            if event.button() == Qt.MouseButton.LeftButton:
                win.local_mouse_state = (True, win.local_mouse_state[1], win.local_mouse_state[2], win.local_mouse_state[3], win.local_mouse_state[4], win.local_mouse_state[5])
            elif event.button() == Qt.MouseButton.RightButton:
                win.local_mouse_state = (win.local_mouse_state[0], True, win.local_mouse_state[2], win.local_mouse_state[3], win.local_mouse_state[4], win.local_mouse_state[5])
            elif event.button() == Qt.MouseButton.MiddleButton:
                win.local_mouse_state = (win.local_mouse_state[0], win.local_mouse_state[1], True, win.local_mouse_state[3], win.local_mouse_state[4], win.local_mouse_state[5])
            win.update_display()
        super().mousePressEvent(event)

    def mouseReleaseEvent(self, event):
        win = self.window()
        if getattr(win, "allow_local", False):
            if event.button() == Qt.MouseButton.LeftButton:
                win.local_mouse_state = (False, win.local_mouse_state[1], win.local_mouse_state[2], win.local_mouse_state[3], win.local_mouse_state[4], win.local_mouse_state[5])
            elif event.button() == Qt.MouseButton.RightButton:
                win.local_mouse_state = (win.local_mouse_state[0], False, win.local_mouse_state[2], win.local_mouse_state[3], win.local_mouse_state[4], win.local_mouse_state[5])
            elif event.button() == Qt.MouseButton.MiddleButton:
                win.local_mouse_state = (win.local_mouse_state[0], win.local_mouse_state[1], False, win.local_mouse_state[3], win.local_mouse_state[4], win.local_mouse_state[5])
            win.update_display()
        super().mouseReleaseEvent(event)

    def wheelEvent(self, event):
        win = self.window()
        if getattr(win, "allow_local", False):
            delta = int(event.angleDelta().y() / 120)
            # accumulate wheel
            left, right, middle, wheel, x, y = win.local_mouse_state
            win.local_mouse_state = (left, right, middle, wheel + delta, x, y)
            win.update_display()
        super().wheelEvent(event)

    def set_position(self, x: int, y: int):
        self.mouse_x = x
        self.mouse_y = y
        self.update()

    def paintEvent(self, e):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        w = self.width()
        h = self.height()
        margin = 14

        # 外框 16:9
        outer_w = w - 2 * margin
        outer_h = int(outer_w * 9 / 16)
        if outer_h > h - 2 * margin:
            outer_h = h - 2 * margin
            outer_w = int(outer_h * 16 / 9)

        outer_x = (w - outer_w) // 2
        outer_y = (h - outer_h) // 2

        pad = 10
        inner_x = outer_x + pad
        inner_y = outer_y + pad
        inner_w = max(1, outer_w - 2 * pad)
        inner_h = max(1, outer_h - 2 * pad)

        # 保存用于坐标反算
        self._inner_x, self._inner_y = inner_x, inner_y
        self._inner_w, self._inner_h = inner_w, inner_h

        p.setBrush(QBrush(QColor("#1f2233")))
        p.setPen(QPen(QColor("#6c7086"), 2))
        p.drawRoundedRect(outer_x, outer_y, outer_w, outer_h, 10, 10)

        # 网格
        p.setPen(QPen(QColor("#313244"), 1))
        for i in range(1, 4):
            gx = inner_x + int(inner_w * i / 4)
            p.drawLine(gx, inner_y, gx, inner_y + inner_h)
        for i in range(1, 3):
            gy = inner_y + int(inner_h * i / 3)
            p.drawLine(inner_x, gy, inner_x + inner_w, gy)

        # 点位（Y 轴翻转：屏幕坐标向下，但我们希望向上为正）
        x = max(0, min(self.map_w - 1, int(self.mouse_x)))
        y = max(0, min(self.map_h - 1, int(self.mouse_y)))
        dot_x = inner_x + int(x * (inner_w - 1) / (self.map_w - 1))
        dot_y = inner_y + int((self.map_h - 1 - y) * (inner_h - 1) / (self.map_h - 1))

        dot_color = QColor("#87ceeb") if self.local_mode else QColor("#06d6a0")
        p.setBrush(QBrush(dot_color))
        p.setPen(QPen(QColor("#118ab2"), 2))
        p.drawEllipse(dot_x - 4, dot_y - 4, 8, 8)

        p.setPen(QColor("#94a3b8"))
        p.drawText(outer_x + 10, outer_y + outer_h - 8, f"X:{self.mouse_x}  Y:{self.mouse_y}")


class KeyboardWidget(QFrame):
    """键盘可视化部件"""
    KEY_W = 28
    KEY_H = 24
    GAP = 2

    def __init__(self):
        super().__init__()
        # 做成更“方”的观感，同时后续会按窗口自适应缩放绘制
        self.setMinimumSize(720, 260)
        self.pressed_hids = set()
        self.local_mode = False
        self.serial_color = QColor("#06d6a0")
        self.local_color = QColor("#87ceeb")
        self.setStyleSheet("""
            KeyboardWidget {
                background: #2b2d42;
                border-radius: 8px;
                border: 2px solid #4a4e69;
            }
        """)

    def set_pressed_keys(self, hids: set, local: bool = False):
        self.pressed_hids = hids
        self.local_mode = local
        self.update()

    def paintEvent(self, e):
        from PyQt6.QtGui import QPainter
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)
        p.setRenderHint(QPainter.RenderHint.TextAntialiasing)

        margin_x, margin_y = 20, 12
        kw, kh, gap = self.KEY_W, self.KEY_H, self.GAP

        # 自适应缩放：保证所有按键完整显示
        max_units = 0.0
        for row in KEYBOARD_ROWS:
            units = 0.0
            for _, w, _ in row:
                units += float(w)
            max_units = max(max_units, units)

        rows = len(KEYBOARD_ROWS)
        avail_w = max(1, self.width() - 2 * margin_x)
        avail_h = max(1, self.height() - 2 * margin_y)

        scale_w = avail_w / max(1.0, (max_units * kw))
        scale_h = avail_h / max(1.0, (rows * kh + (rows - 1) * gap))
        s = min(scale_w, scale_h)

        kw_s = kw * s
        kh_s = kh * s
        gap_s = gap * s

        x0 = margin_x
        y0 = margin_y

        for row_idx, row in enumerate(KEYBOARD_ROWS):
            x = x0
            y = y0 + row_idx * (kh_s + gap_s)
            for hid, w, label in row:
                w_px = kw_s * float(w)
                rect = (x, y, w_px - gap_s, kh_s)
                pressed = hid in self.pressed_hids
                if pressed:
                    color = self.local_color if self.local_mode else self.serial_color
                    p.setBrush(QBrush(color))
                    p.setPen(QPen(QColor("#118ab2"), 2))
                else:
                    p.setBrush(QBrush(QColor("#4a4e69")))
                    p.setPen(QPen(QColor("#6c7086"), 1))
                r = max(3, int(4 * s))
                p.drawRoundedRect(int(rect[0]), int(rect[1]), int(rect[2]), int(rect[3]), r, r)
                p.setPen(QColor("#edf2f4"))
                font = QFont("Consolas", max(7, int(8 * s)))
                p.setFont(font)
                txt = label if label else "?"
                p.drawText(int(rect[0]), int(rect[1]), int(rect[2]), int(rect[3]),
                           Qt.AlignmentFlag.AlignCenter, txt)
                x += w_px


class MouseWidget(QFrame):
    """鼠标可视化部件 - 左键/右键/中键 + 滚轮动画"""
    def __init__(self):
        super().__init__()
        self.setMinimumSize(300, 340)
        self.left_pressed = False
        self.right_pressed = False
        self.middle_pressed = False
        self.wheel_offset = 0.0  # 滚轮滚动偏移(动画)
        self.wheel_target = 0.0
        self.mouse_x = 0
        self.mouse_y = 0
        self.local_mode = False
        self.serial_color = QColor("#06d6a0")
        self.local_color = QColor("#87ceeb")
        self._anim_timer = QTimer(self)
        self._anim_timer.timeout.connect(self._animate)
        self._anim_timer.start(30)
        self.setStyleSheet("""
            MouseWidget {
                background: #2b2d42;
                border-radius: 8px;
                border: 2px solid #4a4e69;
            }
        """)

    def set_mouse_state(self, left: bool, right: bool, middle: bool, wheel_delta: int, x: int = 0, y: int = 0, local: bool = False):
        self.left_pressed = left
        self.right_pressed = right
        self.middle_pressed = middle
        # 方向修正：向上滚动视觉上向上移动（offset 为负）
        self.wheel_target += (-wheel_delta) * 10
        self.wheel_target = max(-18, min(18, self.wheel_target))
        self.mouse_x = x
        self.mouse_y = y
        self.local_mode = local
        self.update()

    def _animate(self):
        # 更短的动画：更快跟随 + 更快回弹衰减
        self.wheel_offset += (self.wheel_target - self.wheel_offset) * 0.35
        self.wheel_target *= 0.75
        if abs(self.wheel_target) < 0.4:
            self.wheel_target = 0.0
        self.update()

    def paintEvent(self, e):
        p = QPainter(self)
        p.setRenderHint(QPainter.RenderHint.Antialiasing)

        w = self.width()
        h = self.height()
        margin = 14

        # 鼠标形状区域（更“高”一些）
        mouse_x0 = margin
        mouse_y0 = margin
        mouse_w = w - 2 * margin
        mouse_h = h - 2 * margin

        # 鼠标主体（圆角矩形）
        body_r = 22
        p.setBrush(QBrush(QColor("#4a4e69")))
        p.setPen(QPen(QColor("#6c7086"), 2))
        p.drawRoundedRect(mouse_x0, mouse_y0, mouse_w, mouse_h, body_r, body_r)

        # 顶部按键区域：左/右键 + 中间滚轮槽
        top_h = int(mouse_h * 0.50)
        sep_w = 2
        wheel_w = int(mouse_w * 0.18)
        left_w = int((mouse_w - wheel_w - 2 * sep_w) / 2)
        right_w = mouse_w - wheel_w - 2 * sep_w - left_w

        key_y = mouse_y0
        left_x = mouse_x0
        wheel_x = left_x + left_w + sep_w
        right_x = wheel_x + wheel_w + sep_w

        # 左键
        color = self.local_color if (self.left_pressed and self.local_mode) else self.serial_color if self.left_pressed else QColor("#6c7086")
        p.setBrush(QBrush(color))
        p.setPen(QPen(QColor("#edf2f4"), 1))
        p.drawRoundedRect(left_x, key_y, left_w, top_h, 14, 14)

        # 右键
        color = self.local_color if (self.right_pressed and self.local_mode) else self.serial_color if self.right_pressed else QColor("#6c7086")
        p.setBrush(QBrush(color))
        p.setPen(QPen(QColor("#edf2f4"), 1))
        p.drawRoundedRect(right_x, key_y, right_w, top_h, 14, 14)

        # 分隔线（更像鼠标）
        p.setPen(QPen(QColor("#2b2d42"), 2))
        p.drawLine(left_x + left_w, key_y + 10, left_x + left_w, key_y + top_h - 10)
        p.drawLine(wheel_x + wheel_w, key_y + 10, wheel_x + wheel_w, key_y + top_h - 10)

        # 滚轮槽
        p.setBrush(QBrush(QColor("#2b2d42")))
        p.setPen(QPen(QColor("#45475a"), 1))
        p.drawRoundedRect(wheel_x, key_y, wheel_w, top_h, 10, 10)

        # 滚轮本体（圆角矩形），随 wheel_offset 上下移动
        wheel_slot_pad = 6
        wheel_item_w = wheel_w - 2 * wheel_slot_pad
        wheel_item_h = int(top_h * 0.35)
        base_center = key_y + top_h // 2
        wheel_center_y = int(base_center + self.wheel_offset)
        wheel_center_y = max(key_y + wheel_slot_pad + wheel_item_h // 2,
                             min(key_y + top_h - wheel_slot_pad - wheel_item_h // 2, wheel_center_y))

        brush_color = QColor("#cdd6f4")
        if self.middle_pressed:
            brush_color = self.local_color if self.local_mode else self.serial_color
        p.setBrush(QBrush(brush_color))
        p.setPen(QPen(QColor("#118ab2"), 1))
        p.drawRoundedRect(
            wheel_x + wheel_slot_pad,
            wheel_center_y - wheel_item_h // 2,
            wheel_item_w,
            wheel_item_h,
            8, 8
        )

        


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("FPGA-SPI 键盘鼠标上位机")
        self.setMinimumSize(950, 420)
        self.setStyleSheet("""
            QMainWindow { background: #1a1b26; }
            QLabel { color: #cdd6f4; }
            QComboBox {
                background: #313244;
                color: #cdd6f4;
                border: 1px solid #45475a;
                padding: 6px 12px;
                border-radius: 6px;
                min-width: 120px;
            }
            QPushButton {
                background: #45475a;
                color: #cdd6f4;
                border: none;
                padding: 8px 20px;
                border-radius: 6px;
            }
            QPushButton:hover { background: #585b70; }
            QPushButton:checked { background: #06d6a0; color: #1a1b26; }
        """)

        self.serial_worker = None
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.poll_serial)

        # 状态跟踪
        self.last_pkt = None              # 上次收到的串口数据
        self.allow_local = True           # 是否允许本机输入覆盖（由串口数据变化决定）
        self.local_enabled = False        # 用户点击“本地控制”按钮启用
        self.serial_hids = set()          # 串口按键集合
        self.local_hids = set()           # 本机按键集合
        # 鼠标状态:(left,right,middle,wheel,x,y)
        self.serial_mouse_state = (False, False, False, 0, 0, 0)
        self.local_mouse_state = (False, False, False, 0, 0, 0)

        central = QWidget()
        self.setCentralWidget(central)
        layout = QVBoxLayout(central)

        # 串口选择
        top = QHBoxLayout()
        top.addWidget(QLabel("串口:"))
        self.port_combo = QComboBox()
        self.refresh_ports()
        top.addWidget(self.port_combo)
        self.btn_refresh = QPushButton("刷新")
        self.btn_refresh.clicked.connect(self.refresh_ports)
        top.addWidget(self.btn_refresh)
        self.btn_connect = QPushButton("连接")
        self.btn_connect.setCheckable(True)
        self.btn_connect.clicked.connect(self.toggle_connect)
        top.addWidget(self.btn_connect)
        # 本地控制按钮
        self.btn_local = QPushButton("本地控制")
        self.btn_local.setCheckable(True)
        self.btn_local.clicked.connect(self.toggle_local_control)
        top.addWidget(self.btn_local)
        top.addStretch()
        layout.addLayout(top)

        # 三块布局：上方坐标映射窗口；下方左键盘右鼠标
        self.map_widget = CoordinateMapWidget(1920, 1080)
        layout.addWidget(self.map_widget, 0)

        bottom = QHBoxLayout()
        self.keyboard = KeyboardWidget()
        bottom.addWidget(self.keyboard, 3)
        self.mouse_widget = MouseWidget()
        bottom.addWidget(self.mouse_widget, 1)
        layout.addLayout(bottom, 1)

        self.status_label = QLabel("未连接")
        layout.addWidget(self.status_label)

        # 初始化显示状态
        self.update_display()

    def refresh_ports(self):
        self.port_combo.clear()
        for p in serial.tools.list_ports.comports():
            self.port_combo.addItem(f"{p.device} - {p.description}", p.device)
        if self.port_combo.count() == 0:
            self.port_combo.addItem("(无串口)", None)

    def toggle_connect(self):
        # 连接串口时禁用本地控制按钮
        if self.btn_connect.isChecked():
            self.btn_local.setChecked(False)
            self.local_enabled = False
        
        if self.btn_connect.isChecked():
            port = self.port_combo.currentData()
            if not port:
                self.btn_connect.setChecked(False)
                self.status_label.setText("请选择串口")
                return
            self.serial_worker = SerialWorker(port, 115200)
            if self.serial_worker.open():
                self.serial_worker.dataReceived.connect(self.on_data)
                self.timer.start(5)
                self.btn_connect.setText("断开")
                self.status_label.setText(f"已连接 {port} @ 115200")
            else:
                self.btn_connect.setChecked(False)
                self.status_label.setText("连接失败")
        else:
            self.timer.stop()
            if self.serial_worker:
                self.serial_worker.close()
                self.serial_worker = None
            self.btn_connect.setText("连接")
            self.status_label.setText("未连接")

    # -------- 本地输入处理 --------
    def keyPressEvent(self, event: QKeyEvent):
        if self.allow_local and self.local_enabled and not event.isAutoRepeat():
            hid = QT_TO_HID.get(event.key())
            if hid:
                self.local_hids.add(hid)
                self.update_display()
        super().keyPressEvent(event)

    def keyReleaseEvent(self, event: QKeyEvent):
        if self.allow_local and self.local_enabled and not event.isAutoRepeat():
            hid = QT_TO_HID.get(event.key())
            if hid and hid in self.local_hids:
                self.local_hids.remove(hid)
                self.update_display()
        super().keyReleaseEvent(event)

    def set_local_mouse_pos(self, x: int, y: int):
        left, right, middle, wheel, _, _ = self.local_mouse_state
        self.local_mouse_state = (left, right, middle, wheel, x, y)
        self.update_display()

    def poll_serial(self):
        if self.serial_worker:
            self.serial_worker.read_available()

    def toggle_local_control(self):
        self.local_enabled = self.btn_local.isChecked()
        self.update_display()

    def update_display(self):
        """根据串口/本地状态更新可视化界面"""
        # 鼠标状态
        # 判断本地是否有激活输入（按钮/移动/滚轮）
        local_activity = any(self.local_mouse_state[:3]) or \
                         (self.local_mouse_state[4:] != self.serial_mouse_state[4:]) or \
                         (self.local_mouse_state[3] != self.serial_mouse_state[3]) or \
                         bool(self.local_hids)
        if self.allow_local and self.local_enabled and local_activity:
            # 本地有活动输入（鼠标或键盘）且用户启用本地控制
            left, right, middle, wheel, x, y = self.local_mouse_state
            self.mouse_widget.set_mouse_state(left, right, middle, wheel, x, y, local=True)
            # 滚轮使用一次后重置
            self.local_mouse_state = (left, right, middle, 0, x, y)
            self.map_widget.set_position(x, y)
            display_hids = self.local_hids
            local_flag = True
        else:
            left, right, middle, wheel, x, y = self.serial_mouse_state
            self.mouse_widget.set_mouse_state(left, right, middle, wheel, x, y, local=False)
            self.map_widget.set_position(x, y)
            display_hids = self.serial_hids
            local_flag = False

        # 通知映射窗口当前模式以便改变点颜色
        self.map_widget.set_local_mode(local_flag)

        self.keyboard.set_pressed_keys(display_hids, local=local_flag)
        # 状态标签
        if self.allow_local and local_flag and self.local_enabled:
            self.status_label.setText("本地控制（串口空闲）")
        elif not self.local_enabled:
            self.status_label.setText("本地控制未启用")
        else:
            self.status_label.setText("串口控制")

    def on_data(self, data: bytes):
        pkt = parse_packet(data)
        if not pkt:
            return

        # 串口对本机是否占用：只要当前包含有效按键/鼠标就认为串口在操控
        serial_has_input = bool(pkt.mouse_buttons) or bool(pkt.modifier) or any(pkt.keys) or pkt.wheel_delta != 0

        if self.last_pkt is None:
            # 首帧到来：无输入允许本地控制
            self.allow_local = not serial_has_input
        else:
            if serial_has_input:
                # 只要串口当前包含按键/鼠标按键/滚轮，串口优先
                self.allow_local = False
            else:
                # 串口当前无输入，立即允许本地控制
                self.allow_local = True

        self.last_pkt = pkt

        # 串口状态更新（始终更新最新数据）
        self.serial_hids.clear()
        for hid in get_modifier_keys(pkt.modifier):
            self.serial_hids.add(hid)
        for k in pkt.keys:
            if k:
                self.serial_hids.add(k)

        left = bool(pkt.mouse_buttons & 1)
        right = bool(pkt.mouse_buttons & 2)
        middle = bool(pkt.mouse_buttons & 4)
        self.serial_mouse_state = (left, right, middle, pkt.wheel_delta, pkt.x, pkt.y)

        # 计算是否串口当前有输入（按键/按钮/滚轮/移动）
        old_x, old_y = getattr(self, 'last_serial_xy', (None, None))
        serial_moved = (old_x is not None and old_y is not None and (pkt.x != old_x or pkt.y != old_y))
        self.last_serial_xy = (pkt.x, pkt.y)

        serial_has_input = bool(pkt.mouse_buttons or pkt.modifier or any(pkt.keys) or pkt.wheel_delta) or serial_moved

        if serial_has_input:
            self.allow_local = False
            # 串口主动操作抑制本地，清空本地输入状态
            self.local_hids.clear()
            self.local_mouse_state = (False, False, False, 0, pkt.x, pkt.y)
        else:
            # 串口空闲时，允许本地控制
            self.allow_local = True
            # 清空串口按键状态，避免残留按键
            self.serial_hids.clear()

        self.update_display()


def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    win = MainWindow()
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
