"""TTBOX EDID 工具包 — 生成/验证/应用/模式构建 EDID。"""
from .builder import build_from_config, edid_to_hex_text, EdidBuilder
from .validator import validate_edid, verify_edid
from .timing_db import (
    DisplayTiming,
    TIMING_MAP,
    lookup_timing,
    list_supported_tokens,
    available_modes,
    TIMING_4K60,
    SAFE_MODES,
    mode_info,
    is_dynamic_mode_token,
    reduced_blanking_pixel_clock_khz,
    pixel_clock_fits_hdmi_rx,
)
from .monitor import (
    read_real_monitor_info,
    read_hdmirx_status,
    parse_edid_identity,
    monitor_has_resolution,
    monitor_supports_refresh,
    at_or_below_native,
)
from .mode_builder import (
    load_config,
    build_display_mode_tokens_for_config,
    apply_real_monitor_to_config,
)

__all__ = [
    "build_from_config", "edid_to_hex_text", "EdidBuilder",
    "validate_edid", "verify_edid",
    "DisplayTiming", "TIMING_MAP", "lookup_timing",
    "list_supported_tokens", "available_modes", "TIMING_4K60",
    "SAFE_MODES", "mode_info", "is_dynamic_mode_token",
    "reduced_blanking_pixel_clock_khz", "pixel_clock_fits_hdmi_rx",
    "read_real_monitor_info", "read_hdmirx_status", "parse_edid_identity",
    "monitor_has_resolution", "monitor_supports_refresh", "at_or_below_native",
    "load_config", "build_display_mode_tokens_for_config",
    "apply_real_monitor_to_config",
]
