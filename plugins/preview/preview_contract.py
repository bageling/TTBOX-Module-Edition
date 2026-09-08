"""Preview Plugin 对外数据契约。"""
from __future__ import annotations
from dataclasses import dataclass
from enum import Enum
import time

class PixelFormat(str, Enum): JPEG="jpeg"; BGR888="bgr888"; RGB888="rgb888"; NV12="nv12"
@dataclass(frozen=True)
class PreviewFrame:
    data: bytes
    frame_number: int
    timestamp_us: int
    width: int
    height: int
    pixel_format: str = PixelFormat.JPEG.value
    source: str = "core_ipc"
@dataclass
class PreviewConfig:
    fps: int = 12
    jpeg_quality: int = 70
    width: int = 640
    height: int = 360
    streaming: bool = True
    def validate(self):
        if not 1 <= self.fps <= 60: raise ValueError("fps 必须在 1~60")
        if not 1 <= self.jpeg_quality <= 100: raise ValueError("jpeg_quality 必须在 1~100")
        if not 1 <= self.width <= 3840 or not 1 <= self.height <= 2160: raise ValueError("preview 尺寸无效")
@dataclass
class PreviewStatus:
    enabled: bool = True
    running: bool = False
    health: str = "unknown"
    frame_number: int = 0
    timestamp_us: int = 0
    width: int = 0
    height: int = 0
    pixel_format: str = "jpeg"
    source: str = "core_ipc"
    fps: float = 0.0
    dropped: int = 0
    error: str = ""

def now_us(): return time.time_ns() // 1000
