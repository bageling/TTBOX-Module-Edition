"""
TTBOX EDID Timing Database — 真实 Display Timing 参数，非估算公式。

来源：CVT 1.2 / CEA-861 / 板端实测。字段：width, height, refresh, pixel_clock(MHz),
h_front_porch, h_sync, h_back_porch, v_front_porch, v_sync, v_back_porch。
"""

from dataclasses import dataclass
from typing import Optional


@dataclass(frozen=True)
class DisplayTiming:
    width: int
    height: int
    refresh: float
    pixel_clock: float  # MHz
    h_front_porch: int
    h_sync: int
    h_back_porch: int
    v_front_porch: int
    v_sync: int
    v_back_porch: int
    interlaced: bool = False
    h_pol: bool = True
    v_pol: bool = True

    @property
    def h_active(self) -> int:
        return self.width

    @property
    def h_blank(self) -> int:
        return self.h_front_porch + self.h_sync + self.h_back_porch

    @property
    def h_total(self) -> int:
        return self.h_active + self.h_blank

    @property
    def v_active(self) -> int:
        return self.height

    @property
    def v_blank(self) -> int:
        return self.v_front_porch + self.v_sync + self.v_back_porch

    @property
    def v_total(self) -> int:
        return self.v_active + self.v_blank

    @property
    def label(self) -> str:
        return f"{self.width}x{self.height}@{int(round(self.refresh))}"

    @property
    def token(self) -> str:
        return f"{self.width}x{self.height}@{int(round(self.refresh))}"

    @property
    def pixel_clock_10khz(self) -> int:
        return int(round(self.pixel_clock * 100))

    def verify(self) -> Optional[str]:
        if self.h_active <= 0 or self.h_total <= self.h_active:
            return "水平参数异常"
        if self.v_active <= 0 or self.v_total <= self.v_active:
            return "垂直参数异常"
        if self.h_front_porch < 0 or self.h_sync < 0 or self.h_back_porch < 0:
            return "水平空白参数负数"
        if self.v_front_porch < 0 or self.v_sync < 0 or self.v_back_porch < 0:
            return "垂直空白参数负数"
        if self.pixel_clock <= 0 or self.pixel_clock > 600:
            return "像素时钟超范围 (0-600 MHz)"
        actual_hz = (self.pixel_clock * 1_000_000) / (self.h_total * self.v_total)
        if abs(actual_hz - self.refresh) > 1.0:
            return f"刷新率不匹配: 计算={actual_hz:.2f}Hz, 声称={self.refresh}Hz"
        return None


# ── 标准时序数据库 ──
TIMING_1080P60 = DisplayTiming(1920, 1080, 60.0, 148.5, 88, 44, 148, 4, 5, 36)
TIMING_1080P120 = DisplayTiming(1920, 1080, 120.0, 297.0, 88, 44, 148, 4, 5, 36)
TIMING_1080P144 = DisplayTiming(1920, 1080, 144.0, 348.941, 48, 32, 80, 3, 5, 77)
TIMING_1080P240 = DisplayTiming(1920, 1080, 240.0, 594.0, 88, 44, 148, 4, 5, 36, h_pol=False, v_pol=False)

# 2K 模式像素时钟必须用 VESA/YU 标准值（此前 CVT-RB 近似值偏低 3-5%，
# PC 显卡模式库无法匹配 → 拒绝 2K → fallback 1080p，即"1K 能注入 2K 注入不了"）
TIMING_1440P60 = DisplayTiming(2560, 1440, 60.0, 248.87, 48, 32, 80, 3, 5, 77)
TIMING_1440P120 = DisplayTiming(2560, 1440, 120.0, 497.75, 48, 32, 80, 3, 5, 77)
TIMING_1440P144 = DisplayTiming(2560, 1440, 144.0, 586.345, 48, 32, 80, 3, 5, 49, h_pol=False, v_pol=False)
TIMING_1440P165 = DisplayTiming(2560, 1440, 165.0, 663.75, 48, 32, 80, 3, 5, 49)

TIMING_4K60 = DisplayTiming(3840, 2160, 60.0, 594.0, 176, 88, 296, 8, 10, 72, h_pol=False, v_pol=False)

TIMING_MAP = {
    "1080p60": TIMING_1080P60,
    "1080p60compat": DisplayTiming(1920, 1080, 60.0, 145.392, 88, 44, 148, 4, 5, 36),
    "1080p90": DisplayTiming(1920, 1080, 90.0, 222.75, 88, 44, 148, 4, 5, 36),
    "1080p120": TIMING_1080P120,
    "1080p120compat": DisplayTiming(1920, 1080, 120.0, 290.784, 88, 44, 148, 4, 5, 36),
    "1080p144": TIMING_1080P144,
    "1080p240": TIMING_1080P240,
    "1080p240compat": DisplayTiming(1920, 1080, 240.0, 581.568, 88, 44, 148, 4, 5, 36),
    "1440p60": TIMING_1440P60,
    "1440p120": TIMING_1440P120,
    "1440p144": TIMING_1440P144,
    "1440p165": TIMING_1440P165,
    "2160p60": TIMING_4K60,
}

# 安全模式表（对齐 yu safe_modes: token, w, h, refresh, pixel_clock_khz）
SAFE_MODES = [
    ("2160p60", 3840, 2160, 60, 594000),
    ("1440p144", 2560, 1440, 144, 586345),
    ("1440p120", 2560, 1440, 120, 497750),
    ("1440p60", 2560, 1440, 60, 248875),
    ("1080p240compat", 1920, 1080, 240, 581568),
    ("1080p144", 1920, 1080, 144, 348941),
    ("1080p120", 1920, 1080, 120, 297000),
    ("1080p60compat", 1920, 1080, 60, 145392),
]

# 动态模式正则（对齐 yu）
DYNAMIC_MODE_RE = None


def reduced_blanking_pixel_clock_khz(width, height, refresh):
    """CVT-RB 像素时钟估算（对齐 yu 公式）。"""
    if width <= 0 or height <= 0 or refresh <= 0:
        return 0
    return ((width + 48 + 32 + 80) * (height + 3 + 5 + 77) * refresh + 500) // 1000


def pixel_clock_fits_hdmi_rx(pixel_clock_khz):
    """像素时钟合法性：25MHz ~ 600MHz（HDMI 2.0 RX 带宽）。"""
    return 25000 <= pixel_clock_khz <= 600000


def mode_info(token):
    """解析 token：内置名或动态 WxH@Hz。返回 (token, w, h, refresh, pc_khz) 或 None。"""
    global DYNAMIC_MODE_RE
    import re
    if DYNAMIC_MODE_RE is None:
        DYNAMIC_MODE_RE = re.compile(r"^(\d{3,4})x(\d{3,4})@(\d{2,3})$")
    for item in SAFE_MODES:
        if item[0] == token:
            return item
    for key, t in TIMING_MAP.items():
        if key == token:
            return (key, t.width, t.height, int(round(t.refresh)), int(round(t.pixel_clock * 1000)))
    m = DYNAMIC_MODE_RE.match(str(token or ""))
    if not m:
        return None
    width = int(m.group(1))
    height = int(m.group(2))
    refresh = int(m.group(3))
    if width < 640 or width > 4095 or height < 400 or height > 4095 or refresh < 24 or refresh > 360:
        return None
    pc_khz = reduced_blanking_pixel_clock_khz(width, height, refresh)
    if not pixel_clock_fits_hdmi_rx(pc_khz):
        return None
    return (f"{width}x{height}@{refresh}", width, height, refresh, pc_khz)


def is_dynamic_mode_token(token):
    global DYNAMIC_MODE_RE
    import re
    if DYNAMIC_MODE_RE is None:
        DYNAMIC_MODE_RE = re.compile(r"^(\d{3,4})x(\d{3,4})@(\d{2,3})$")
    return bool(DYNAMIC_MODE_RE.match(str(token or "")))

BOARD_ACTIVE_TIMING = TIMING_4K60


def lookup_timing(token: str) -> DisplayTiming:
    """通过 token（'1440p144' 或 '2560x1440@144'）查找时序。"""
    if token in TIMING_MAP:
        return TIMING_MAP[token]
    import re
    m = re.match(r"^(\d+)x(\d+)@(\d+)$", token.strip())
    if m:
        w, h, r = int(m.group(1)), int(m.group(2)), int(m.group(3))
        for t in TIMING_MAP.values():
            if t.width == w and t.height == h and int(round(t.refresh)) == r:
                return t
    raise KeyError(f"未知 timing token: {token}")


def list_supported_tokens() -> list:
    return list(TIMING_MAP.keys())


def available_modes() -> list:
    result = []
    for token, t in TIMING_MAP.items():
        err = t.verify()
        result.append({
            "token": t.token,
            "label": f"{t.width}x{t.height} @ {t.height}p{int(t.refresh)}",
            "width": t.width,
            "height": t.height,
            "refresh": int(round(t.refresh)),
            "pixel_clock_khz": int(round(t.pixel_clock * 1000)),
            "valid": err is None,
            "error": err or "",
        })
    return result
