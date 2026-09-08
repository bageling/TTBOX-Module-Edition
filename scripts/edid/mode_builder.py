"""TTBOX EDID 模式构建器 — 对齐 yu apply_display_edid.sh 的核心逻辑。

包含：
  build_display_mode_tokens_for_config(config, monitor)
  apply_real_monitor_to_config(config)     # loopout 环出
  load_config()                            # 配置校验/白名单
"""
import re
from .timing_db import SAFE_MODES, mode_info, is_dynamic_mode_token
from .monitor import (
    read_real_monitor_info,
    monitor_has_resolution,
    monitor_supports_refresh,
    at_or_below_native,
)

MAX_ADVERTISED_MODES = 6
PROFILES_SET = {
    "boot-safe-1080p240", "boot-safe-full", "standard-dual",
    "single-1440p60", "single-1080p120-compat", "single-1080p60-compat",
}
NATIVE_MODES_SET = {
    "", "1080p60", "1080p60compat", "1080p90", "1080p120",
    "1080p120compat", "1080p144", "1080p240", "1080p240compat",
    "1440p60", "1440p120", "1440p144", "2160p60",
}


def _safe_ascii(value, limit, fallback):
    text = "".join(ch for ch in str(value or "") if 32 <= ord(ch) <= 126)[:limit]
    return text or fallback


def _hex_text(value, width, fallback):
    text = str(value or "").strip()
    try:
        number = int(text, 16 if text.lower().startswith("0x") else 10)
    except ValueError:
        return fallback
    if number <= 0 or number >= (1 << (width * 4)):
        return fallback
    return f"0x{number:0{width}x}"


def _bool_value(value, fallback=False):
    if isinstance(value, bool):
        return value
    if isinstance(value, int):
        return value != 0
    text = str(value or "").strip().lower()
    if text in {"1", "true", "yes", "on"}:
        return True
    if text in {"0", "false", "no", "off"}:
        return False
    return fallback


def _add_unique(values, token):
    if token and token not in values:
        values.append(token)


def _available_tokens_for_monitor(monitor):
    """显示器支持的模式 token 列表。"""
    tokens = []
    for mode in monitor.get("modes", []):
        token = mode.get("token") or ""
        if not token:
            w, h, r = mode.get("width"), mode.get("height"), mode.get("refresh")
            if w and h and r:
                token = f"{w}x{h}@{r}"
        _add_unique(tokens, token)
    return tokens


def build_display_mode_tokens_for_config(config, monitor):
    """根据配置 + 显示器构建模式 token 列表（对齐 yu 核心逻辑）。"""
    requested_native = config.get("native_mode", "")
    native_only = _bool_value(config.get("native_only"), False)

    if monitor.get("connected") and not monitor.get("edid_valid"):
        tokens = []
        if mode_info(requested_native):
            _add_unique(tokens, requested_native)
        if not tokens:
            _add_unique(tokens, "1080p60compat")
        if not native_only:
            _add_unique(tokens, "1080p60compat")
        return tokens

    available_tokens = _available_tokens_for_monitor(monitor)
    safe_tokens = []
    if not (monitor.get("connected") and available_tokens):
        for token, width, height, refresh, _clock in SAFE_MODES:
            if (at_or_below_native(monitor, width, height)
                    and monitor_has_resolution(monitor, width, height)
                    and monitor_supports_refresh(monitor, width, height, refresh)):
                safe_tokens.append(token)
    if not safe_tokens:
        safe_tokens.append("1080p60compat")
    if not monitor.get("connected"):
        return safe_tokens

    tokens = []
    requested = mode_info(requested_native)
    if requested and (
        is_dynamic_mode_token(requested_native)
        or requested_native in available_tokens
        or requested_native in safe_tokens
        or (not available_tokens
            and at_or_below_native(monitor, requested[1], requested[2])
            and monitor_has_resolution(monitor, requested[1], requested[2]))
    ):
        _add_unique(tokens, requested_native)
    if not tokens:
        for token in available_tokens:
            _add_unique(tokens, token)
            break
    if not tokens:
        _add_unique(tokens, safe_tokens[0])
    if not native_only:
        for token in available_tokens:
            _add_unique(tokens, token)
            if len(tokens) >= MAX_ADVERTISED_MODES:
                break
        if not available_tokens and len(tokens) < 2:
            for token in safe_tokens:
                _add_unique(tokens, token)
                if len(tokens) >= 2:
                    break
        if len(tokens) > MAX_ADVERTISED_MODES:
            tokens = tokens[:MAX_ADVERTISED_MODES]
    return tokens or ["1080p60compat"]


def apply_real_monitor_to_config(config):
    """loopout 环出：若开启，用真实显示器身份覆盖配置。"""
    if not _bool_value(config.get("loopout_enabled"), False):
        return config
    monitor = read_real_monitor_info()
    if not (monitor.get("connected") and monitor.get("edid_valid")):
        return config
    config = dict(config)
    config["name"] = _safe_ascii(monitor.get("name"), 13, config["name"])
    vendor = _safe_ascii(monitor.get("vendor"), 3, config["vendor"]).upper()
    if re.fullmatch(r"[A-Z]{3}", vendor):
        config["vendor"] = vendor
    config["product_id"] = _hex_text(monitor.get("product_id"), 4, config["product_id"])
    config["serial"] = _hex_text(monitor.get("serial"), 8, config["serial"])
    tokens = build_display_mode_tokens_for_config(config, monitor)
    if tokens:
        config["native_mode"] = tokens[0]
        config["added_modes"] = tokens[1:]
    print("Loopout real monitor EDID: "
          f"name={config['name']} vendor={config['vendor']} "
          f"product={config['product_id']} serial={config['serial']} "
          f"connector={monitor.get('connector', 'unknown')}")
    return config


def load_config(config_path="/opt/ttbox/config/hardware_display.json"):
    """读取并校验配置（对齐 yu load_config：白名单校验）。"""
    import json
    config = {}
    try:
        with open(config_path, encoding="utf-8") as f:
            loaded = json.load(f)
            if isinstance(loaded, dict):
                config = loaded
    except FileNotFoundError:
        pass

    device = _safe_ascii(config.get("device", "auto"), 48, "auto")
    if device != "auto" and not re.fullmatch(r"/dev/video\d+", device):
        device = "auto"

    profile = _safe_ascii(config.get("profile", "boot-safe-full"), 32, "boot-safe-full")
    if profile not in PROFILES_SET:
        profile = "boot-safe-full"

    native_mode = _safe_ascii(config.get("native_mode", ""), 32, "")
    if native_mode == "auto":
        native_mode = ""
    elif native_mode not in NATIVE_MODES_SET and not mode_info(native_mode):
        native_mode = ""

    vendor = _safe_ascii(config.get("vendor", "OPI"), 3, "OPI").upper()
    if not re.fullmatch(r"[A-Z]{3}", vendor):
        vendor = "OPI"

    config = {
        "device": device,
        "profile": profile,
        "native_mode": native_mode,
        "native_only": _bool_value(config.get("native_only"), False),
        "loopout_enabled": _bool_value(config.get("loopout_enabled"), False),
        "name": _safe_ascii(config.get("name", "OPI-COMPAT"), 13, "OPI-COMPAT"),
        "vendor": vendor,
        "product_id": _hex_text(config.get("product_id", "0x3588"), 4, "0x3588"),
        "serial": _hex_text(config.get("serial", "0x20260414"), 8, "0x20260414"),
    }
    return apply_real_monitor_to_config(config)
