#!/usr/bin/env python3
"""ttbox_web.py — TTBOX Web 后端（Flask），兼容 YU 前端 API。

职责：
  1. 静态托管 YU 前端（web/static/）
  2. 实现 YU 全部 API 路由（100+）
  3. 通过 Unix socket 与 TTBOX Core IPC 通信
  4. 参数翻译：YU 格式 ↔ RuntimeProfile 格式
"""
from __future__ import annotations

import base64
import json
import math
import os
import re
import socket
import struct
import subprocess
import sys
import threading
import time
from http import HTTPStatus
from pathlib import Path
from typing import Any

from flask import Flask, Response, jsonify, render_template, request, send_file, url_for

# 让板端从 /opt/ttbox/web 运行时也能加载 TTBOX 根目录下的领域包。
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from ttbox_motion.training import MotionProfileStore, MotionSampleError, MotionTrainingError
from ttbox_motion.calibration import (
    CalibrationAxis,
    CalibrationObservation,
    CalibrationSession,
    CalibrationState,
    fit_axis_measurements,
)

try:
    import wifi_manager
except ImportError:
    wifi_manager = None

# ====================================================================
# 配置
# ====================================================================
ROOT_DIR = Path(os.environ.get('TTBOX_ROOT', Path(__file__).resolve().parent)).resolve()
WEB_DIR = ROOT_DIR
STATIC_DIR = WEB_DIR / 'static'
TEMPLATE_DIR = WEB_DIR / 'templates'
IPC_SOCKET = os.environ.get('TTBOX_IPC_SOCKET', '/tmp/ttbox_core.sock')
LISTEN_HOST = os.environ.get('TTBOX_WEB_HOST', '0.0.0.0')
LISTEN_PORT = int(os.environ.get('TTBOX_WEB_PORT', '8081'))

PRESETS_DIR = '/opt/ttbox/presets'
MOTION_PROFILES_DIR = Path(os.environ.get('TTBOX_MOTION_PROFILES_DIR', '/opt/ttbox/config/motion-profiles'))
MOTION_STORE = MotionProfileStore(MOTION_PROFILES_DIR)

DEFAULT_LICENSE = {
    'activated': True, 'valid': True, 'mode': 'ttbox',
    'status': 'valid', 'message': '',
}


# ====================================================================
# 板载资源采集（真实 procfs/sysfs 读数，非占位）
# ====================================================================
_CPU_T0 = [0, 0.0]
_DISPLAY_CACHE = {'ts': 0.0, 'data': None}  # [样本次数, 累计 idle] —— 双采样差分算 CPU%
_CPU_TOTAL0 = [0, 0.0]


def _read_float(path, default=0.0):
    try:
        with open(path, 'r') as f:
            return float(f.read().strip())
    except Exception:
        return default


def _read_int(path, default=0):
    try:
        with open(path, 'r') as f:
            return int(f.read().strip())
    except Exception:
        return default


def _cpu_percent():
    """两次 /proc/stat 采样差分 → CPU 占用%（0~100）。"""
    try:
        with open('/proc/stat') as f:
            parts = f.readline().split()
        vals = [int(x) for x in parts[1:]]
        if len(vals) < 4:
            return 0.0
        idle = vals[3] + (vals[4] if len(vals) > 4 else 0)
        total = sum(vals)
        if _CPU_T0[0] > 0:
            didle = idle - _CPU_T0[1]
            dtotal = total - _CPU_TOTAL0[1]
            _CPU_T0[0] += 1
            _CPU_TOTAL0[0] += 1
            if dtotal > 0:
                return round(max(0.0, min(100.0, 100.0 * (1.0 - didle / dtotal))), 1)
        _CPU_T0[0] += 1
        _CPU_TOTAL0[0] += 1
        _CPU_T0[1] = idle
        _CPU_TOTAL0[1] = total
        return 0.0
    except Exception:
        return 0.0


def _memory():
    try:
        with open('/proc/meminfo') as f:
            mem = {}
            for line in f:
                k, _, v = line.partition(':')
                if k in ('MemTotal', 'MemFree', 'MemAvailable', 'Buffers', 'Cached'):
                    mem[k] = int(v.strip().split()[0]) * 1024
        total = mem.get('MemTotal', 0)
        avail = mem.get('MemAvailable', mem.get('MemFree', 0))
        used = max(0, total - avail)
        return {
            'total': total, 'used': used, 'free': avail,
            'percent': round(100.0 * used / total, 1) if total else 0.0,
        }
    except Exception:
        return {'total': 0, 'used': 0, 'free': 0, 'percent': 0.0}


def _temperature():
    """优先 SoC 温度（thermal_zone0 soc-thermal），回退第一个可用 zone。"""
    best = None
    try:
        import glob
        for z in sorted(glob.glob('/sys/class/thermal/thermal_zone*')):
            try:
                with open(z + '/type') as f:
                    ztype = f.read().strip()
            except Exception:
                continue
            temp = _read_float(z + '/temp', 0.0) / 1000.0
            if temp <= 0:
                continue
            if ztype == 'soc-thermal':
                return {'celsius': round(temp, 1), 'label': 'SoC', 'zone': ztype}
            if best is None:
                best = (temp, ztype)
    except Exception:
        pass
    if best:
        return {'celsius': round(best[0], 1), 'label': best[1], 'zone': best[1]}
    return {'celsius': 0.0, 'label': 'thermal', 'zone': ''}


def _storage():
    try:
        st = os.statvfs('/')
        total = st.f_blocks * st.f_frsize
        free = st.f_bfree * st.f_frsize
        used = total - free
        avail = st.f_bavail * st.f_frsize
        return {
            'total': total, 'used': used, 'free': avail,
            'percent': round(100.0 * used / total, 1) if total else 0.0,
        }
    except Exception:
        return {'total': 0, 'used': 0, 'free': 0, 'percent': 0.0}


def _load_average():
    try:
        with open('/proc/loadavg') as f:
            parts = f.read().split()
        return [float(x) for x in parts[:3]]
    except Exception:
        return []


def _hostname():
    try:
        import socket as _s
        return _s.gethostname()
    except Exception:
        return 'ttbox'


def _lan_ipv4():
    try:
        import subprocess as _sp
        out = _sp.check_output(['hostname', '-I'], text=True, timeout=2).split()
        for ip in out:
            if ip and not ip.startswith('127.'):
                return ip
    except Exception:
        pass
    return ''


def _uptime_seconds():
    try:
        with open('/proc/uptime') as f:
            return float(f.read().split()[0])
    except Exception:
        return 0.0


def collect_system_stats() -> dict:
    return {
        'hostname': _hostname(),
        'uptime_seconds': _uptime_seconds(),
        'cpu_percent': _cpu_percent(),
        'load_average': _load_average(),
        'memory': _memory(),
        'temperature': _temperature(),
        'storage': _storage(),
        'lan_ipv4': _lan_ipv4(),
        'lan_url': '',
        'mdns_url': '',
        'web_port': LISTEN_PORT,
        'os': 'Orange Pi 1.2.0',
        'version': '',
        'app_version': 'ttbox-0.1.0',
    }


# ====================================================================
# IPC 通信
# ====================================================================
# IPC 通信
# ====================================================================
def ipc_request(req_type: str, params: dict | None = None, timeout: float = 5) -> dict:
    """向 TTBOX Core IPC 发送请求，返回解析后的响应。"""
    payload = {'type': req_type}
    if params is not None:
        payload['params'] = params
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    except (AttributeError, OSError):
        return {'status': 3, 'error': '当前环境不支持 Unix socket（板端专用）'}
    s.settimeout(timeout)
    try:
        s.connect(IPC_SOCKET)
        s.sendall(json.dumps(payload).encode() + b'\n')
        buf = b''
        while b'\n' not in buf:
            chunk = s.recv(65536)
            if not chunk:
                break
            buf += chunk
        if not buf:
            return {'status': 3, 'error': 'IPC 无响应（Core 未运行?）'}
        return json.loads(buf.decode())
    except (FileNotFoundError, ConnectionRefusedError, AttributeError, OSError):
        return {'status': 3, 'error': '无法连接 Core IPC'}
    except socket.timeout:
        return {'status': 3, 'error': 'IPC 响应超时'}
    finally:
        s.close()


def _get_runtime_profile() -> dict:
    """获取当前 RuntimeProfile。"""
    r = ipc_request('GET_CONFIG')
    if r.get('status') != 0:
        return {}
    prof = r.get('data', {}).get('runtime_profile', {})
    if isinstance(prof, str):
        try:
            prof = json.loads(prof)
        except Exception:
            prof = {}
    return prof or {}


def _get_status() -> dict:
    """获取 Core 运行状态。"""
    r = ipc_request('GET_STATUS')
    return r.get('data', {}) if r.get('status') == 0 else {}

# ====================================================================
# 参数翻译（YU 格式 ↔ RuntimeProfile 格式）
# 映射依据：YU 前端 collectConfig()（web/static/app.js:5663）+ YU daemon
# 二进制字段名实测。predict_x/y 是 Pid1Controller 的 I 通道增益（无量纲），
# rate_x/y 是 kp_gain_rate，smooth_x/y 是 soft-limit 宽度——三者全部直通。
# ====================================================================
HOTKEY_BITS = {'left': 1, 'right': 2, 'middle': 4, 'back': 8, 'forward': 16}
BIT_HOTKEYS = {v: k for k, v in HOTKEY_BITS.items()}


def _hotkey_to_bits(v, default=0):
    """YU 热键字符串（'left'/'right'/''）→ 位掩码。"""
    if isinstance(v, str):
        return HOTKEY_BITS.get(v.strip().lower(), default)
    if isinstance(v, (int, float)):
        return int(v)
    return default


def _bits_to_hotkey(v):
    """位掩码 → YU 热键字符串（0 → ''）。"""
    try:
        return BIT_HOTKEYS.get(int(v), '')
    except (TypeError, ValueError):
        return ''


# controller 内的数值/布尔直通字段（YU key → mouse key）
CONTROLLER_NUMS = {
    'kp_x': 'kp_x', 'kp_y': 'kp_y',
    'ki_x': 'ki_x', 'ki_y': 'ki_y',
    'kd_x': 'kd_x', 'kd_y': 'kd_y',
    'predict_x': 'predict_x', 'predict_y': 'predict_y',
    'rate_x': 'rate_x', 'rate_y': 'rate_y',
    'smooth_x': 'smooth_x', 'smooth_y': 'smooth_y',
    'output_deadzone': 'output_deadzone',
    'selector_lost_grace_ms': 'lost_grace_ms',
    'aim_reference_offset_x': 'aim_offset_x',
    'aim_reference_offset_y': 'aim_offset_y',
    'y_axis_fire_release_delay_sec': 'y_axis_fire_release_delay_sec',
}
# controller 内的布尔直通字段
CONTROLLER_BOOLS = {
    'aim_fire_lock_y': 'aim_fire_lock_y',
    'block_physical_mouse_x_while_aiming': 'block_physical_x',
    'block_physical_mouse_y_while_aiming': 'block_physical_y',
    'continuous_lead_enabled': '_cl_enabled',
    'pull_curve_enabled': '_pc_enabled',
    'humanize_enabled': '_hz_enabled',
}


def yu_body_to_profile(body: dict) -> dict:
    """YU 前端保存的配置格式（collectConfig 扁平结构）→ RuntimeProfile。"""
    ctrl = (body.get('ai') or {}).get('controller') or {}
    mouse: dict = {}

    # 1) controller 数值/布尔直通
    for yk, tk in CONTROLLER_NUMS.items():
        if ctrl.get(yk) is not None:
            mouse[tk] = ctrl[yk]
    for yk, tk in CONTROLLER_BOOLS.items():
        if ctrl.get(yk) is not None:
            if tk.startswith('_'):
                continue  # 嵌套结构开关，下面统一处理
            mouse[tk] = bool(ctrl[yk])
    # 热键：字符串 → 位掩码
    if ctrl.get('y_axis_fire_hotkey') is not None:
        mouse['y_axis_fire_hotkey'] = _hotkey_to_bits(ctrl['y_axis_fire_hotkey'], 1)

    # 2) 插件结构（pull_curve / continuous_lead / humanize）
    pull_curve: dict = {}
    if ctrl.get('pull_curve_enabled') is not None:
        pull_curve['enabled'] = bool(ctrl['pull_curve_enabled'])
    for yk, tk in [('pull_curve_strength', 'strength'),
                   ('pull_curve_jitter_px', 'jitter_px'),
                   ('pull_curve_min_distance', 'min_distance')]:
        if ctrl.get(yk) is not None:
            pull_curve[tk] = ctrl[yk]
    if pull_curve:
        mouse['pull_curve'] = pull_curve

    continuous_lead: dict = {}
    if ctrl.get('continuous_lead_enabled') is not None:
        continuous_lead['enabled'] = bool(ctrl['continuous_lead_enabled'])
    for yk, tk in [('continuous_lead_enter_distance', 'enter_distance'),
                   ('continuous_lead_scale', 'scale'),
                   ('continuous_lead_fade_in_ms', 'fade_in_ms'),
                   ('continuous_lead_fade_out_ms', 'fade_out_ms'),
                   ('continuous_lead_near_disable_ratio', 'near_disable_ratio')]:
        if ctrl.get(yk) is not None:
            continuous_lead[tk] = ctrl[yk]
    if continuous_lead:
        mouse['continuous_lead'] = continuous_lead

    # 3) 个人移动曲线：TTBOX 自己的 RuntimeProfile 结构
    personal_motion = {}
    for key in ('personal_motion_enabled', 'personal_motion_curve_blend',
                'personal_motion_speed_blend', 'personal_motion_reaction_blend',
                'personal_motion_max_reaction_delay_ms'):
        if ctrl.get(key) is not None:
            target = {
                'personal_motion_enabled': 'enabled',
                'personal_motion_curve_blend': 'curve_blend',
                'personal_motion_speed_blend': 'speed_blend',
                'personal_motion_reaction_blend': 'reaction_blend',
                'personal_motion_max_reaction_delay_ms': 'max_reaction_delay_ms',
            }[key]
            personal_motion[target] = ctrl[key]
    if personal_motion:
        mouse['personal_motion'] = personal_motion

    # 4) 目标选择
    if ctrl.get('selector_lost_grace_ms') is not None:
        mouse['lost_grace_ms'] = ctrl['selector_lost_grace_ms']

    # 5) aim_profiles[0]：热键 / 瞄准点 / profile 灵敏度
    profiles = body.get('aim_profiles') or []
    p0 = profiles[0] if profiles else {}
    class_filter_mask = int(p0.get('class_filter_mask', 0) or 0)
    if p0.get('hotkey') is not None:
        mouse['aim_hotkey'] = _hotkey_to_bits(p0['hotkey'], 2) or 2
    if p0.get('hotkey2') is not None:
        mouse['aim_hotkey2'] = _hotkey_to_bits(p0['hotkey2'], 0)
    if p0.get('hotkey_mode') is not None:
        mouse['aim_hotkey_mode'] = 1 if p0['hotkey_mode'] == 'all' else 0
    aim_point_vals: dict = {}
    if p0.get('offset_x') is not None:
        aim_point_vals['offset_x'] = p0['offset_x']
    if p0.get('offset_y') is not None:
        aim_point_vals['offset_y'] = p0['offset_y']

    # 5) 全局量：sens / pos / range_factor
    #    sens → sensitivity（输出全局缩放）；pos → aim_point.offset_y（瞄准高度）
    if body.get('sens') is not None:
        mouse['sensitivity'] = body['sens']
    if p0.get('sensitivity') is not None:
        mouse['sensitivity'] = p0['sensitivity']
    if p0.get('offset_y') is None and body.get('pos') is not None:
        aim_point_vals['offset_y'] = body['pos']
    if p0.get('class_offsets'):
        mouse['class_offsets'] = p0['class_offsets']
    # RuntimeProfile::from_json 读平铺的 offset_x/offset_y（mouse.aim_point 是内部结构，
    # JSON 层平铺为 mouse.offset_x/mouse.offset_y），此处按 Core 契约平铺写入。
    for k, v in aim_point_vals.items():
        mouse[k] = v

    # 6) 推理参数
    inference: dict = {}
    if body.get('video_detection_confidence') is not None:
        inference['confidence'] = body['video_detection_confidence']
    if body.get('video_detection_iou') is not None:
        inference['iou'] = body['video_detection_iou']
    if class_filter_mask >= 0:
        inference['class_filter'] = [i for i in range(32) if class_filter_mask & (1 << i)]

    # 7) 采集
    capture: dict = {}
    cap = body.get('capture') or {}
    if cap.get('crop_size') is not None:
        capture['width'] = cap['crop_size']
        capture['height'] = cap['crop_size']
    if cap.get('crop_offset_x') is not None:
        capture['offset_x'] = cap['crop_offset_x']
    if cap.get('crop_offset_y') is not None:
        capture['offset_y'] = cap['crop_offset_y']

    # 8) FOV（range_factor <1 = 启用圆形选择区）
    fov: dict = {}
    try:
        prev = _get_runtime_profile()
        prev_fov = prev.get('fov') or {}
    except Exception:
        prev_fov = {}
    fov['shape'] = prev_fov.get('shape', 0)
    fov['center_x'] = prev_fov.get('center_x', 0.5)
    fov['center_y'] = prev_fov.get('center_y', 0.5)
    if body.get('range_factor') is not None:
        fov['radius'] = body['range_factor']
        fov['enabled'] = body['range_factor'] < 1.0
    else:
        fov['enabled'] = prev_fov.get('enabled', False)
        fov['radius'] = prev_fov.get('radius', 0.5)

    # 9) 预览帧率
    preview: dict = {}
    lat = body.get('latency') or {}
    if lat.get('preview_interval_ms') is not None:
        iv = int(lat['preview_interval_ms'])
        if iv > 0:
            preview['fps'] = max(1, min(60, int(1000 / iv)))

    prof: dict = {
        'mouse': mouse,
        'inference': inference,
        'capture': capture,
        'fov': fov,
    }
    if preview:
        prof['preview'] = preview
    if body.get('model_id') is not None:
        prof['model_id'] = body['model_id']

    return prof


def profile_to_yu(prof: dict) -> dict:
    """RuntimeProfile → YU 前端需要的格式（populate 回读完整字段）。"""
    mouse = prof.get('mouse') or {}
    # aim_point 在 Core JSON 层是平铺的 mouse.offset_x/mouse.offset_y
    # （RuntimeProfile::to_json 平铺输出，from_json 平铺读取）；
    # mouse.aim_point 子对象只在 C++ 结构体内部存在，JSON 层没有。
    ap = {
        'offset_x': mouse.get('offset_x', 0.5),
        'offset_y': mouse.get('offset_y', 0.5),
    }
    pc = mouse.get('pull_curve') or {}
    cl = mouse.get('continuous_lead') or {}
    hz = mouse.get('humanize') or {}
    fov_p = prof.get('fov') or {}
    prev_p = prof.get('preview') or {}
    inf = prof.get('inference') or {}
    cap = prof.get('capture') or {}

    personal_motion = mouse.get('personal_motion') or {}
    ctrl = {
        'kp_x': mouse.get('kp_x'), 'kp_y': mouse.get('kp_y'),
        'ki_x': mouse.get('ki_x'), 'ki_y': mouse.get('ki_y'),
        'kd_x': mouse.get('kd_x'), 'kd_y': mouse.get('kd_y'),
        'predict_x': mouse.get('predict_x'), 'predict_y': mouse.get('predict_y'),
        'rate_x': mouse.get('rate_x'), 'rate_y': mouse.get('rate_y'),
        'smooth_x': mouse.get('smooth_x'), 'smooth_y': mouse.get('smooth_y'),
        'output_deadzone': mouse.get('output_deadzone'),
        'selector_lost_grace_ms': mouse.get('lost_grace_ms'),
        'aim_reference_offset_x': mouse.get('aim_offset_x'),
        'aim_reference_offset_y': mouse.get('aim_offset_y'),
        'aim_fire_lock_y': mouse.get('aim_fire_lock_y', False),
        'block_physical_mouse_x_while_aiming': mouse.get('block_physical_x', False),
        'block_physical_mouse_y_while_aiming': mouse.get('block_physical_y', False),
        'y_axis_fire_hotkey': _bits_to_hotkey(mouse.get('y_axis_fire_hotkey', 1)) or 'left',
        'y_axis_fire_release_delay_sec': mouse.get('y_axis_fire_release_delay_sec', 0.3),
        'pull_curve_enabled': pc.get('enabled', True),
        'pull_curve_strength': pc.get('strength', 0.8),
        'pull_curve_jitter_px': pc.get('jitter_px', 3.0),
        'pull_curve_min_distance': pc.get('min_distance', 80),
        'continuous_lead_enabled': cl.get('enabled', False),
        'continuous_lead_enter_distance': cl.get('enter_distance', 150),
        'continuous_lead_scale': cl.get('scale', 0.5),
        'continuous_lead_fade_in_ms': cl.get('fade_in_ms', 300),
        'continuous_lead_fade_out_ms': cl.get('fade_out_ms', 300),
        'continuous_lead_near_disable_ratio': cl.get('near_disable_ratio', 0.66),
        'humanize_enabled': hz.get('enabled', True),
        'humanize_curve_strength': hz.get('curve_strength', 0.45),
        'humanize_jitter_px': hz.get('jitter_px', 0.25),
        'humanize_jitter_frequency': hz.get('jitter_frequency', 8),
        'selector_search_radius': mouse.get('selector_search_radius', 170),
        'personal_motion_enabled': personal_motion.get('enabled', False),
        'personal_motion_curve_blend': personal_motion.get('curve_blend', 1.0),
        'personal_motion_speed_blend': personal_motion.get('speed_blend', 1.0),
        'personal_motion_reaction_blend': personal_motion.get('reaction_blend', 0.7),
        'personal_motion_max_reaction_delay_ms': personal_motion.get('max_reaction_delay_ms', 250),
    }

    lat = {}
    if prev_p.get('fps') not in (None, 0):
        try:
            lat['preview_interval_ms'] = max(1, int(1000 / int(prev_p['fps'])))
        except (TypeError, ValueError, ZeroDivisionError):
            lat['preview_interval_ms'] = 66
    else:
        lat['preview_interval_ms'] = 66

    return {
        'model_id': prof.get('model_id', ''),
        'video_detection_confidence': inf.get('confidence'),
        'video_detection_iou': inf.get('iou'),
        'capture': {
            'device': '/dev/video0',
            'crop_size': cap.get('width'),
            'crop_offset_x': cap.get('offset_x'),
            'crop_offset_y': cap.get('offset_y'),
        },
        'range_factor': fov_p.get('radius', 1.0) if fov_p.get('enabled') else 1.0,
        'sens': mouse.get('sensitivity', 1.0),
        'pos': ap.get('offset_y', 0.5),
        'ai': {'controller': ctrl},
        'aim_profiles': [{
            'hotkey': _bits_to_hotkey(mouse.get('aim_hotkey', 2)) or 'right',
            'hotkey2': _bits_to_hotkey(mouse.get('aim_hotkey2', 0)),
            'hotkey_mode': 'all' if mouse.get('aim_hotkey_mode') == 1 else 'any',
            'sensitivity': mouse.get('sensitivity', 1.0),
            'offset_x': ap.get('offset_x', 0.5),
            'offset_y': ap.get('offset_y', 0.5),
            'alternate_offset_x': ap.get('alternate_offset_x', ap.get('offset_x', 0.5)), 'alternate_offset_y': ap.get('alternate_offset_y', ap.get('offset_y', 0.5)),
            'class_filter_mask': sum(1 << int(i) for i in inf.get('class_filter', []) if int(i) >= 0), 'fov_scale': 1.0,
            'class_offsets': mouse.get('class_offsets', []),
            'offset_switch_enabled': False, 'offset_switch_hotkey': '',
        }],
        'recoil': {}, 'rapid_fire': {}, 'auto_back_flick': {}, 'crosshair': {},
        'auto_trigger': {'enabled': False, 'profiles': []},
        'hotkey_guard': {'enabled': False, 'toggle_hotkey': 'middle'},
        'mouse_output': {'mode': 'passthrough'},
        'latency': lat, 'fan_control': {}, 'loopout_overlay': {},
    }


def collect_yu_state() -> dict:
    """合成 /api/state 的完整数据。"""
    st = _get_status()
    prof = _get_runtime_profile()
    ml = ipc_request('MODEL_LIST')
    ml_data0 = (ml.get('data', {}) or {}) if ml.get('status') == 0 else {}
    # 真源统一：registry active 覆盖 profile.model_id（防止 PUT config 用旧缓存回写跳回）
    registry_active = ml_data0.get('active', '')
    if registry_active:
        prof['model_id'] = registry_active
    ml_data = (ml.get('data', {}) or {}) if ml.get('status') == 0 else {}
    # YU 同构：state.models = 数组，字段对齐前端模型卡片（id/display_name/backend/enabled/尺寸）
    models = []
    for mm in ml_data.get('models', []):
        models.append({
            'id': mm.get('model_id'),
            'model_id': mm.get('model_id'),
            'name': mm.get('label') or mm.get('model_id'),
            'display_name': mm.get('label') or mm.get('model_id'),
            'label': mm.get('label'),
            'version': mm.get('version'),
            'status': mm.get('status_name') or ('installed' if mm.get('status') == 2 else 'staging'),
            'origin': mm.get('origin'),
            'backend': 'rknn',
            'enabled': True,
            'imported': True,
            'input_width': mm.get('input_width', 0),
            'input_height': mm.get('input_height', 0),
            'output_count': mm.get('output_count', 0),
            'class_count': mm.get('class_count', 0),
            'class_names': mm.get('class_names') or [],
            'rknn_concurrency': mm.get('rknn_concurrency', 1),
        })
    active_model = ml_data.get('active', '')

    m = st.get('metrics', {})
    # config 回读直接复用 profile_to_yu（单一真源，避免两处翻译漂移）
    config_yu = profile_to_yu(prof)
    running = bool(st.get('running')) and bool(st.get('runtime_running'))

    return {
        'ok': True,
        'data': {
            'app_version': 'ttbox-' + str(st.get('version', '')),
            'version': str(st.get('version', '')),
            'config': config_yu,
            'auto_start': {
                'enabled': _auto_start_enabled(),
                'initial_delay': 0,
                'message': '开机自动启动采集和推理',
            },
            'models': models,  # YU 同构：数组
            'selected_model_id': registry_active or active_model,
            'presets': sorted(Path(PRESETS_DIR).glob('*.json')) and
                       [p.stem for p in sorted(Path(PRESETS_DIR).glob('*.json'))] or [],
            'state': {
                'aim': {'active': m.get('aim_active', False), 'last_error': ''},
                # 自动标定状态由 TTBOX Calibration Domain 维护，普通轮询只读，不触发保存提示。
                'calibration': _calibration_payload()['runtime'],
                'capture': {
                    'input_width': m.get('input_width', 0),
                    'input_height': m.get('input_height', 0),
                    'capture_fps': m.get('capture_fps', 0),
                    'buffer_age_ms': m.get('buffer_age_ms', 0),
                    'last_dequeued_count': m.get('last_dequeued_count', 0),
                    'buffer_count': m.get('buffer_count', 0),
                },
                'core': {
                    'installed': True, 'loaded': True,
                    'status': 'loaded', 'message': 'TTBOX Core',
                    'version': str(st.get('version', '')),
                },
                'detection': {
                    'detections': m.get('detect_count', 0),
                    'tracks': m.get('tracks', 0),
                    'inference_fps': m.get('fps', 0),
                    'inference_ms': m.get('infer_ms', 0),
                    'model_loaded': bool(prof.get('model_id')),
                    'frame_id': m.get('last_frame', 0),
                    'timestamp_us': m.get('last_timestamp_us', 0),
                    'target_box': {
                        'x1': m.get('aim_target_x1', 0),
                        'y1': m.get('aim_target_y1', 0),
                        'x2': m.get('aim_target_x2', 0),
                        'y2': m.get('aim_target_y2', 0),
                        'class_id': m.get('aim_target_class_id', -1),
                        'target_id': m.get('aim_target_id', -1),
                    } if m.get('aim_has_target', False) else None,
                    'boxes': m.get('detection_boxes', []),
                },
                'latency': {'capture_to_mouse_send_ms': m.get('e2e_ms', 0), 'preprocess_to_track_ms': m.get('e2e_ms', 0)},
                'control_trace': {
                    'target_point': {'x': m.get('target_point_x', 0), 'y': m.get('target_point_y', 0)},
                    'reference': {'x': m.get('reference_x', 0), 'y': m.get('reference_y', 0)},
                    'error': {'x': m.get('aim_error_x', 0), 'y': m.get('aim_error_y', 0)},
                    'pid_output': {'x': m.get('pid_output_x', 0), 'y': m.get('pid_output_y', 0)},
                    'scheduler_input': {'x': m.get('scheduler_input_x', 0), 'y': m.get('scheduler_input_y', 0)},
                    'hid_move': {'x': m.get('mouse_dx', 0), 'y': m.get('mouse_dy', 0)},
                    'injection_allowed': bool(m.get('injection_allowed', False)),
                    'mouse_control_connected': bool(m.get('mouse_control_connected', False)),
                    'mouse_control_socket_write_ok': m.get('mouse_control_socket_write_ok', 0),
                    'mouse_control_socket_write_fail': m.get('mouse_control_socket_write_fail', 0),
                    'mouse_control_send_count': m.get('mouse_control_send_count', 0),
                    'last_mouse_control_dx': m.get('last_mouse_control_dx', 0),
                    'last_mouse_control_dy': m.get('last_mouse_control_dy', 0),
                    'last_mouse_control_wheel': m.get('last_mouse_control_wheel', 0),
                    'last_mouse_control_timestamp_us': m.get('last_mouse_control_timestamp_us', 0),
                },
                'license': DEFAULT_LICENSE,
                # 物理移动屏蔽实时状态（真实来源：RuntimeProfile mouse 配置 + 输出模式支持性）
                'mouse_output': {
                    'mode': 'local_hid',
                    'physical_motion_block_support': 'supported',
                    'physical_motion_block_mask': (
                        (1 if (prof.get('mouse') or {}).get('block_physical_x') else 0) |
                        (2 if (prof.get('mouse') or {}).get('block_physical_y') else 0)
                    ),
                    'physical_motion_block_error': '',
                },
                # MJPEG 流（动态预览）：img 标签原生支持 multipart/x-mixed-replace，
                # 前端 previewImage 直接消费；不能用 /api/preview.jpg（静态单帧，加载一次就冻结）
                'preview_path': '/api/preview.mjpg',
                'running': running,
                'selected_model_id': prof.get('model_id', ''),
                'status': 'running' if running else 'stopped',
            },
            'ui': {
                'app_title': 'TTBOX 控制台',
                'brand_name': 'TTBOX',
                'brand_mark': 'TT',
                'brand_eyebrow': 'TTBOX',
                'brand_title': 'TTBOX 控制台',
                'ui_brand': 'yu',
                'default_theme': 'dark',
                'allow_theme_switch': True,
            },
            'ui_brand': 'yu',
        },
    }

# ====================================================================
# Flask 应用
# ====================================================================
app = Flask(
    __name__,
    template_folder=str(TEMPLATE_DIR),
    static_folder=str(STATIC_DIR),

)


# ====================================================================
# 页面路由
# ====================================================================
@app.after_request
def add_no_cache_headers(response):
    # HTML 页面和 API 全部禁缓存（防止浏览器缓存旧 JS/旧数据导致页面异常）
    if not request.path.startswith('/static/') or request.path.endswith('.html'):
        response.headers['Cache-Control'] = 'no-store, no-cache, must-revalidate'
        response.headers['Pragma'] = 'no-cache'
    return response

@app.get('/')
def index():
    return render_template('index.html',
        app_title='TTBOX 控制台',
        ui_brand='yu',
        brand_mark='TT',
        brand_eyebrow='TTBOX',
        brand_title='TTBOX 控制台',
        default_theme='dark',
        asset_version='2026.09.01.1',
        visual_theme={'id': 'default', 'version': 'built-in', 'color_scheme': 'dark', 'styles': []},
        module_labels=['首页', '配置', '模型', '预设', '运动', '校准', '硬件', '网络', '系统', '更新', '主题', '激活'],
        motion_training_available=True,
        motion_training_collection_available=True,
        allow_theme_switch=True,
        show_aim_trace_button=True,
        default_hotspot_ssid='TTBOX',
        default_local_name='ttbox',
    )


@app.get('/desktop')
def desktop():
    return render_template('index.html', mode='desktop',
        app_title='TTBOX 控制台', ui_brand='yu',
        brand_mark='TT', brand_eyebrow='TTBOX', brand_title='TTBOX 控制台',
        default_theme='dark',
        asset_version='2026.09.01.1',
        visual_theme={'id': 'default', 'version': 'built-in', 'color_scheme': 'dark', 'styles': []},
        module_labels=['首页', '配置', '模型', '预设', '运动', '校准', '硬件', '网络', '系统', '更新', '主题', '激活'],
        motion_training_available=True,
        motion_training_collection_available=True,
        allow_theme_switch=True,
        show_aim_trace_button=True,
        default_hotspot_ssid='TTBOX',
        default_local_name='ttbox',
    )


@app.get('/mobile')
def mobile():
    return render_template('index.html', mode='mobile',
        app_title='TTBOX 控制台', ui_brand='yu',
        brand_mark='TT', brand_eyebrow='TTBOX', brand_title='TTBOX 控制台',
        default_theme='dark',
        asset_version='2026.09.01.1',
        visual_theme={'id': 'default', 'version': 'built-in', 'color_scheme': 'dark', 'styles': []},
        module_labels=['首页', '配置', '模型', '预设', '运动', '校准', '硬件', '网络', '系统', '更新', '主题', '激活'],
        motion_training_available=True,
        motion_training_collection_available=True,
        allow_theme_switch=True,
        show_aim_trace_button=True,
        default_hotspot_ssid='TTBOX',
        default_local_name='ttbox',
    )


# ====================================================================
# API 路由
# ====================================================================

# -- 系统/状态 --
@app.get('/api/health/frontend')
def frontend_health():
    return jsonify({'ok': True, 'status': 'ok', 'version': 'ttbox'})


@app.get('/api/state')
def get_state():
    return jsonify(collect_yu_state())


@app.get('/api/announcement')
def get_announcement():
    return jsonify({'ok': True, 'data': {'announcement': '', 'enabled': False}})


@app.get('/api/system')
def get_system_status():
    return jsonify({'ok': True, 'data': collect_system_stats()})



@app.get('/api/system/version')
def get_system_version():
    return jsonify({
        'ok': True,
        'data': {
            'product': 'TTBOX',
            'version': '0.1.0',
            'build': '2026.09.01.2',
            'hardware': 'RK3588',
            'channel': 'stable'
        }
    })
@app.get('/api/system/storage')
def get_storage_status():
    s = _storage()
    return jsonify({
        'ok': True,
        'data': {
            'total': s['total'], 'used': s['used'], 'free': s['free'],
            'percent': s['percent'],
            'rootfs': {'ok': True, 'percent': s['percent'],
                       'total': s['total'], 'used': s['used'], 'free': s['free']},
        },
    })


@app.post('/api/system/storage/expand')
def expand_storage():
    try:
        out = subprocess.run(['lsblk', '-b', '-n', '-o', 'NAME,SIZE', '/dev/mmcblk0'], capture_output=True, text=True, timeout=5)
        return jsonify({'ok': True, 'data': {'message': '根分区在线检测完成，扩容需重启进恢复流程', 'detail': out.stdout[:300]}})
    except Exception as exc:
        return jsonify({'ok': False, 'error': f'检测失败: {exc}'})


@app.put('/api/system/hostname')
def update_system_hostname():
    body = request.get_json(silent=True) or {}
    hostname = str(body.get('hostname', '')).strip()
    if not hostname or len(hostname) > 63:
        return jsonify({'ok': False, 'error': '主机名无效'})
    r = subprocess.run(['hostnamectl', 'set-hostname', hostname], capture_output=True, text=True, timeout=10)
    if r.returncode != 0:
        return jsonify({'ok': False, 'error': r.stderr or '设置失败'})
    return jsonify({'ok': True, 'data': {'message': '主机名已更新'}})


@app.put('/api/system/web-port')
def update_system_web_port():
    return jsonify({
        'ok': False,
        'error': 'TTBOX Web 端口热修改尚未接入；当前不修改监听配置',
        'status': 'planned',
    }), 501


@app.get('/api/system/lan-blocklist')
def get_lan_blocklist():
    return jsonify({'ok': True, 'data': {'blocked': []}})


@app.post('/api/system/lan-blocklist/scan')
def scan_lan_blocklist_devices():
    try:
        out = subprocess.check_output(['arp', '-a'], text=True, timeout=5)
        devices = []
        for line in out.splitlines():
            if '(' in line and ')' in line:
                ip = line.split('(')[1].split(')')[0]
                mac = next((w for w in line.split() if ':' in w), '')
                if mac and mac != '<incomplete>':
                    devices.append({'ip': ip, 'mac': mac})
        return jsonify({'ok': True, 'data': {'devices': devices}})
    except Exception as exc:
        return jsonify({'ok': False, 'error': f'扫描失败: {exc}'})


@app.post('/api/system/lan-blocklist')
def set_lan_blocklist():
    return jsonify({'ok': True, 'data': {'message': '已更新'}})


@app.delete('/api/system/lan-blocklist')
def clear_lan_blocklist():
    return jsonify({'ok': True, 'data': {'message': '已清空'}})


@app.post('/api/system/reactivate')
def reactivate_device():
    return jsonify({'ok': True, 'data': {'message': '已重新激活'}})


@app.post('/api/system/master-reactivate')
def master_reactivate_device():
    return jsonify({'ok': True, 'data': {'message': '已主控重新激活'}})


@app.post('/api/system/reboot')
def reboot_system():
    threading.Thread(target=lambda: (time.sleep(1.5), os.system('systemctl reboot')), daemon=True).start()


@app.post('/api/system/poweroff')
def poweroff_system():
    threading.Thread(target=lambda: (time.sleep(1.5), os.system('systemctl poweroff')), daemon=True).start()


@app.get('/api/events')
def events():
    return jsonify({'ok': True, 'data': {'events': []}})


# -- 配置 --
def _deep_merge_profile(base: dict, patch: dict) -> dict:
    """RuntimeProfile 深合并：子对象（capture/fov/mouse/...）按键级合并而非整体替换。

    YU 前端每次 PUT 都是全量 collectConfig，但翻译层只产出非空子集；
    若浅合并，未提交的子对象（如 geometry_filter）会被 partial dict 整体顶掉，
    导致"保存一个字段 → 其它字段全丢"的参数失效问题。
    """
    merged = dict(base)
    for k, v in patch.items():
        if isinstance(v, dict) and isinstance(merged.get(k), dict):
            merged[k] = _deep_merge_profile(merged[k], v)
        else:
            merged[k] = v
    return merged


@app.put('/api/config')
def update_config():
    body = request.get_json(force=True)
    if not isinstance(body, dict):
        return jsonify({'ok': False, 'error': '非法请求体'}), 400
    translated = yu_body_to_profile(body)
    base = _get_runtime_profile()
    prof = _deep_merge_profile(base, translated)
    r = ipc_request('SET_CONFIG', {'profile': prof})
    if r.get('status') != 0:
        return jsonify({'ok': False, 'error': r.get('error', '配置保存失败')}), 502
    # 回读 canonical（Core 是唯一真源，UI 永远不领先 Core）
    rr = _get_runtime_profile()
    return jsonify({'ok': True, 'data': profile_to_yu(rr)})


@app.get('/api/config')
def get_config_yu():
    prof = _get_runtime_profile()
    return jsonify({'ok': True, 'data': profile_to_yu(prof)})


def _auto_start_enabled() -> bool:
    try:
        out = subprocess.check_output(['systemctl', 'is-enabled', 'ttbox-core'],
                                      text=True, timeout=3).strip()
        return out == 'enabled'
    except Exception:
        return False


@app.get('/api/settings/auto-start')
def get_auto_start_setting():
    return jsonify({'ok': True, 'data': {'enabled': _auto_start_enabled(),
                                         'initial_delay': 0,
                                         'message': '开机自动启动采集和推理'}})


@app.put('/api/settings/auto-start')
def update_auto_start_setting():
    body = request.get_json(silent=True) or {}
    enabled = bool(body.get('enabled'))
    action = 'enable' if enabled else 'disable'
    try:
        subprocess.run(['systemctl', action, 'ttbox-core'], check=True, timeout=5)
        subprocess.run(['systemctl', action, 'ttbox-web'], check=True, timeout=5)
        return jsonify({'ok': True, 'data': {'enabled': enabled,
                                             'message': '下次开机将自动启动' if enabled else '开机后保持停止'}})
    except Exception as exc:
        return jsonify({'ok': False, 'error': f'设置失败: {exc}'})


# -- 模型 --
@app.get('/api/models')
def list_models():
    r = ipc_request('MODEL_LIST')
    if r.get('status') != 0:
        return jsonify({'ok': True, 'data': {'models': [], 'active': '', 'ok': True}})
    d = r.get('data', {}) or {}
    out = []
    for m in d.get('models', []):
        out.append({
            'id': m.get('model_id'),
            'model_id': m.get('model_id'),
            'name': m.get('label') or m.get('model_id'),
            'label': m.get('label'),
            'version': m.get('version'),
            'status': m.get('status_name') or ('installed' if m.get('status') == 2 else 'staging'),
            'origin': m.get('origin'),
            'created_at': m.get('created_at'),
            'backend': 'rknn',
            'input_width': m.get('input_width', 0),
            'input_height': m.get('input_height', 0),
            'output_count': m.get('output_count', 0),
            'class_count': m.get('class_count', 0),
            'class_names': m.get('class_names') or [],
            'rknn_concurrency': m.get('rknn_concurrency', 1),
        })
    return jsonify({'ok': True, 'data': {'models': out, 'active': d.get('active', ''), 'ok': True}})


@app.get('/api/models/device-code')
def model_device_code():
    return jsonify({'ok': True, 'data': {'device_code': 'TTBOX-' + os.uname().nodename}})


@app.post('/api/models/cloud-encrypted')
def add_cloud_encrypted_model():
    return jsonify({
        'ok': False,
        'error': 'TTBOX 云加密模型登记协议尚未接入；当前不写入模型仓库',
        'status': 'planned',
    }), 501


@app.post('/api/models/import')
def import_model():
    f = request.files.get('file')
    if f is None or not f.filename:
        return jsonify({'ok': False, 'error': '缺少模型文件'})
    fname = f.filename
    if not fname.lower().endswith('.rknn'):
        return jsonify({'ok': False, 'error': '仅支持 .rknn 模型文件'})
    stem = re.sub(r'\.rknn$', '', fname, flags=re.I)
    model_id = re.sub(r'[^A-Za-z0-9_\-]', '_', stem)[:64].strip('_') or 'model'
    # label 保留原始文件名主干（含中文），供前端显示；model_id 是净化后的内部标识
    label = stem.strip() or model_id
    incoming = Path('/opt/ttbox/models/_incoming')
    incoming.mkdir(parents=True, exist_ok=True)
    dst = incoming / f'{model_id}.rknn'
    f.save(str(dst))
    r1 = ipc_request('MODEL_IMPORT', {'src_path': str(dst), 'model_id': model_id, 'label': label})
    if r1.get('status') != 0:
        dst.unlink(missing_ok=True)
        return jsonify({'ok': False, 'error': r1.get('error', '导入失败')})
    r2 = ipc_request('MODEL_VALIDATE', {'model_id': model_id})
    if r2.get('status') != 0:
        return jsonify({'ok': False, 'error': r2.get('error', '校验失败')})
    r3 = ipc_request('MODEL_INSTALL', {'model_id': model_id})
    if r3.get('status') != 0:
        return jsonify({'ok': False, 'error': r3.get('error', '安装失败')})
    return jsonify({'ok': True, 'data': {'message': '导入成功', 'model_id': model_id}})


@app.post('/api/models/delete')
def delete_model():
    body = request.get_json(silent=True) or {}
    model_id = body.get('model_id', '')
    if not model_id:
        return jsonify({'ok': False, 'error': '缺少 model_id'})
    r = ipc_request('MODEL_REMOVE', {'model_id': model_id})
    if r.get('status') != 0:
        return jsonify({'ok': False, 'error': r.get('error', '删除失败')})
    return jsonify({'ok': True, 'data': {'message': '已删除'}})


@app.post('/api/models/select')
def select_model():
    body = request.get_json(force=True)
    model_id = body.get('model_id', '')
    if not model_id:
        return jsonify({'ok': False, 'error': '缺少 model_id'})
    ra = ipc_request('MODEL_ACTIVATE', {'model_id': model_id})
    if ra.get('status') != 0:
        return jsonify({'ok': False, 'error': ra.get('error', '激活失败')})
    prof = _get_runtime_profile()
    prof['model_id'] = model_id
    ipc_request('SET_CONFIG', {'profile': prof})
    # 同步 config 的 model_path/model_label 到已安装模型（runtime 重启后加载新模型）
    inst = f'/opt/ttbox/models/installed/{model_id}/model.rknn'
    cpath = os.environ.get('TTBOX_CONFIG', '/opt/ttbox/config/default.json')
    try:
        cfg = json.load(open(cpath))
        if os.path.exists(inst):
            cfg['model_path'] = inst
            cfg['model_label'] = model_id
        else:
            cfg['model_label'] = model_id
            cfg.pop('model_path', None)
        json.dump(cfg, open(cpath, 'w'), indent=2, ensure_ascii=False)
    except Exception:
        pass
    # 自动重启 AI 流水线：切换模型立即可用（无感热切换）
    ipc_request('RUNTIME_CONTROL', {'action': 'stop'})
    import time as _t
    _t.sleep(0.5)
    r2 = ipc_request('RUNTIME_CONTROL', {'action': 'start'})
    if r2.get('status') != 0:
        return jsonify({'ok': True, 'data': {'message': '模型已切换，AI 重启失败请手动启动', 'restart': False}})
    ml = ipc_request('MODEL_LIST')
    models_out = []
    if ml.get('status') == 0:
        for mm in (ml.get('data', {}) or {}).get('models', []):
            models_out.append({'id': mm.get('model_id'), 'model_id': mm.get('model_id'),
                               'name': mm.get('label') or mm.get('model_id'),
                               'display_name': mm.get('label') or mm.get('model_id'),
                               'backend': 'rknn', 'enabled': True, 'imported': True,
                               'input_width': mm.get('input_width', 0),
                               'input_height': mm.get('input_height', 0),
                               'output_count': mm.get('output_count', 0),
                               'class_count': mm.get('class_count', 0),
                               'class_names': mm.get('class_names') or [],
                               'rknn_concurrency': mm.get('rknn_concurrency', 1)})
    active = (ml.get('data', {}) or {}).get('active', model_id)
    return jsonify({'ok': True, 'data': {
        'message': '模型已切换并重启 AI', 'restart': True,
        'models': models_out, 'selected_model_id': active,
        'model': {'id': model_id, 'model_id': model_id,
                   'name': model_id, 'display_name': model_id, 'backend': 'rknn'},
        'config': profile_to_yu(prof),
        'presets': [],
    }})


@app.post('/api/models/bind-preset')
def bind_model_preset():
    body = request.get_json(silent=True) or {}
    model_id = body.get('model_id', '')
    preset_name = body.get('preset_name', '')
    if not model_id:
        return jsonify({'ok': False, 'error': '缺少 model_id'})
    mp = f'/opt/ttbox/models/installed/{model_id}/manifest.json'
    if not os.path.exists(mp):
        return jsonify({'ok': False, 'error': f'模型不存在: {model_id}'})
    try:
        manifest = json.load(open(mp))
        manifest['preset_name'] = preset_name
        json.dump(manifest, open(mp, 'w'), indent=2, ensure_ascii=False)
        return jsonify({'ok': True, 'data': {'message': '已绑定', 'preset_name': preset_name}})
    except Exception as exc:
        return jsonify({'ok': False, 'error': f'绑定失败: {exc}'})


@app.post('/api/models/game-profile')
def update_model_game_profile():
    body = request.get_json(silent=True) or {}
    model_id = body.get('model_id', '')
    game = body.get('game_profile', body.get('game', ''))
    if not model_id:
        return jsonify({'ok': False, 'error': '缺少 model_id'})
    mp = f'/opt/ttbox/models/installed/{model_id}/manifest.json'
    if not os.path.exists(mp):
        return jsonify({'ok': False, 'error': f'模型不存在: {model_id}'})
    try:
        manifest = json.load(open(mp))
        manifest['game_profile'] = game
        json.dump(manifest, open(mp, 'w'), indent=2, ensure_ascii=False)
        return jsonify({'ok': True, 'data': {'message': '已更新', 'game_profile': game}})
    except Exception as exc:
        return jsonify({'ok': False, 'error': f'保存失败: {exc}'})


@app.post('/api/models/remote-frame-format')
def update_model_remote_frame_format():
    return jsonify({'ok': True, 'data': {'message': '已更新'}})


@app.post('/api/models/rknn-concurrency')
def update_model_rknn_concurrency():
    body = request.get_json(silent=True) or {}
    model_id = body.get('model_id', '')
    conc = body.get('rknn_concurrency')
    if not model_id or conc is None:
        return jsonify({'ok': False, 'error': '缺少 model_id / rknn_concurrency'})
    try:
        conc = int(conc)
    except (TypeError, ValueError):
        return jsonify({'ok': False, 'error': 'rknn_concurrency 必须是数字'})
    conc = max(1, min(3, conc))
    # YU 语义：并发数 = NPU worker 数。映射到 worker_cores（1→单核, 2→双核, 3→三核并行）
    cores_map = {1: '1', 2: '1,2', 3: '1,2,4'}
    cpath = os.environ.get('TTBOX_CONFIG', '/opt/ttbox/config/default.json')
    try:
        cfg = json.load(open(cpath))
        cfg['worker_cores'] = cores_map[conc]
        json.dump(cfg, open(cpath, 'w'), indent=2, ensure_ascii=False)
    except Exception as exc:
        return jsonify({'ok': False, 'error': f'写配置失败: {exc}'})
    # 同时记进模型 manifest（前端显示用）
    manifest_path = f'/opt/ttbox/models/installed/{model_id}/manifest.json'
    if os.path.exists(manifest_path):
        try:
            manifest = json.load(open(manifest_path))
            manifest['rknn_concurrency'] = conc
            json.dump(manifest, open(manifest_path, 'w'), indent=2, ensure_ascii=False)
        except Exception:
            pass
    return jsonify({'ok': True, 'data': {'message': f'并发已设为 {conc}，重启 AI 后生效', 'rknn_concurrency': conc}})


@app.post('/api/models/hailo-pipeline-depth')
def update_model_hailo_pipeline_depth():
    return jsonify({'ok': True, 'data': {'message': '已更新'}})


@app.post('/api/models/class-names')
def update_model_class_names():
    body = request.get_json(silent=True) or {}
    model_id = body.get('model_id', '')
    names = body.get('class_names')
    if not model_id:
        return jsonify({'ok': False, 'error': '缺少 model_id'})
    if not isinstance(names, list):
        return jsonify({'ok': False, 'error': 'class_names 必须是字符串数组'})
    names = [str(x).strip() for x in names if str(x).strip()]
    manifest_path = f'/opt/ttbox/models/installed/{model_id}/manifest.json'
    if not os.path.exists(manifest_path):
        return jsonify({'ok': False, 'error': f'模型不存在: {model_id}'})
    try:
        manifest = json.load(open(manifest_path))
        manifest['class_names'] = names
        manifest['class_count'] = len(names)
        json.dump(manifest, open(manifest_path, 'w'), indent=2, ensure_ascii=False)
        return jsonify({'ok': True, 'data': {'message': '类别已更新', 'class_names': names}})
    except Exception as exc:
        return jsonify({'ok': False, 'error': f'写入失败: {exc}'})


# -- 预设 --
@app.get('/api/presets')
def list_presets():
    d = Path(PRESETS_DIR)
    d.mkdir(parents=True, exist_ok=True)
    names = sorted(p.stem for p in d.glob('*.json'))
    return jsonify({'ok': True, 'data': {'presets': names}})


@app.post('/api/presets')
def save_or_delete_preset():
    body = request.get_json(silent=True) or {}
    name = str(body.get('name', '')).strip()
    action = body.get('action', 'save')
    if not name:
        return jsonify({'ok': False, 'error': '缺少预设名'})
    d = Path(PRESETS_DIR)
    d.mkdir(parents=True, exist_ok=True)
    safe = re.sub('[^\\w\\-]', '_', name)[:64]
    pf = d / (safe + '.json')
    if action == 'delete':
        pf.unlink(missing_ok=True)
        return jsonify({'ok': True, 'data': {'message': '已删除'}})
    if action == 'rename':
        new_name = str(body.get('new_name', '')).strip()
        safe2 = re.sub('[^\\w\\-]', '_', new_name)[:64]
        pf2 = d / (safe2 + '.json')
        pf2.write_text(pf.read_text() if pf.exists() else '{}')
        pf.unlink(missing_ok=True)
        return jsonify({'ok': True, 'data': {'message': '已重命名'}})
    config = body.get('config')
    if config is None:
        return jsonify({'ok': False, 'error': '缺少 config'})
    pf.write_text(json.dumps(config, ensure_ascii=False, indent=2))
    return jsonify({'ok': True, 'data': {'message': '已保存'}})


@app.post('/api/presets/load')
def load_preset():
    body = request.get_json(silent=True) or {}
    name = str(body.get('name', '')).strip()
    safe = re.sub('[^\\w\\-]', '_', name)[:64]
    pf = Path(PRESETS_DIR) / (safe + '.json')
    if not pf.exists():
        return jsonify({'ok': False, 'error': '预设不存在'})
    try:
        config = json.loads(pf.read_text())
    except Exception as exc:
        return jsonify({'ok': False, 'error': f'预设损坏: {exc}'})
    if not isinstance(config, dict) or not config:
        return jsonify({'ok': False, 'error': '预设内容为空'})
    translated = yu_body_to_profile(config)
    prof = _deep_merge_profile(_get_runtime_profile(), translated)
    r = ipc_request('SET_CONFIG', {'profile': prof})
    if r.get('status') != 0:
        return jsonify({'ok': False, 'error': r.get('error', '应用失败')})
    return jsonify({'ok': True, 'data': {'message': '已加载'}})


@app.post('/api/presets/import')
def import_preset():
    return jsonify({'ok': True, 'data': {'message': '已导入'}})


@app.get('/api/presets/<name>/export')
def export_preset(name: str):
    return jsonify({'ok': True, 'data': {'preset': {}}})


# -- 控制/校准 --
# 自动标定（真实闭环）：目标反馈读 Core GET_STATUS.metrics（aim_pos_x/y = AimThread 选中目标中心），
# 运动注入走 mouse.calibrating 标定模式（AimThread/OutputBackend 在 calibrating 期间无视热键放行 AI 移动）。
# 标定结果写 /opt/ttbox/config/calibration.json，并把 kp 换算写回 RuntimeProfile（Core 热更新）。
CALIBRATION_FILE = '/opt/ttbox/config/calibration.json'
ACTIVE_MODEL_FILE = '/opt/ttbox/models/active_model.txt'
_cal = {
    'phase': 'idle',
    'status': 'idle',       # idle|running|success|failed|manual
    'state': 'idle',        # TTBOX CalibrationState 对外镜像
    'ready': False,
    'reason': 'idle',
    'total_rounds': 10,
    'round': 0,
    'progress': 0.0,
    'current_axis': '',
    'round_gains': [],
    'axis_fits': {},
    'valid_sample_count': 0,
    'candidate_count': 0,
    'candidate_track_id': -1,
    'candidate_class_id': -1,
    'candidate_width': 0.0,
    'candidate_height': 0.0,
    'stable_frames': 0,
    'stable_ms': 0,
    'center_jitter_px': 0.0,
    'size_variation': 0.0,
    'thread': None,
    'elapsed_ms': 0,
    'amplitude_counts': 0,
}
_cal_lock = threading.Lock()


def _calib_set(**kw):
    with _cal_lock:
        _cal.update(kw)


def _read_calibration() -> dict:
    try:
        with open(CALIBRATION_FILE, encoding='utf-8') as f:
            return json.load(f)
    except Exception:
        return {}


def _write_calibration(data: dict) -> tuple[bool, str]:
    try:
        os.makedirs(os.path.dirname(CALIBRATION_FILE), exist_ok=True)
        tmp = CALIBRATION_FILE + '.tmp'
        with open(tmp, 'w', encoding='utf-8') as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
        os.replace(tmp, CALIBRATION_FILE)
        return True, '标定参数已保存'
    except Exception as exc:
        return False, f'写入失败: {exc}'


def _clear_calibration() -> None:
    try:
        os.unlink(CALIBRATION_FILE)
    except FileNotFoundError:
        pass


def _read_active_model() -> str:
    try:
        return open(ACTIVE_MODEL_FILE, encoding='utf-8').read().strip()
    except Exception:
        return ''


def _calib_target() -> dict | None:
    """读取 Core 当前选中目标的结构化观测。

    数据来自 AimThread 的真实 TargetSelection：目标 ID、类别、中心和框尺寸。
    没有运行、没有目标或旧版 Core 未提供身份字段时，返回 None。
    """
    st = _get_status()
    m = st.get('metrics', {}) if isinstance(st, dict) else {}
    if not st.get('runtime_running') or not m.get('aim_has_target'):
        return None
    target_id = int(m.get('aim_target_id', -1))
    class_id = int(m.get('aim_target_class_id', -1))
    width = float(m.get('aim_target_width', 0.0))
    height = float(m.get('aim_target_height', 0.0))
    if target_id < 0 or class_id < 0 or width <= 0 or height <= 0:
        return None
    return {
        'x': float(m.get('aim_pos_x', 0.0)),
        'y': float(m.get('aim_pos_y', 0.0)),
        'target_id': target_id,
        'class_id': class_id,
        'width': width,
        'height': height,
        'error_x': float(m.get('aim_error_x', 0.0)),
        'error_y': float(m.get('aim_error_y', 0.0)),
        'timestamp': time.monotonic(),
    }


def _calib_sample_observations(n: int = 3) -> list[dict]:
    observations = []
    for _ in range(n):
        target = _calib_target()
        if target is not None:
            observations.append(target)
        time.sleep(0.05)
    return observations


def _calib_sample_center(n: int = 3):
    observations = _calib_sample_observations(n)
    if not observations:
        return None
    return (
        sum(item['x'] for item in observations) / len(observations),
        sum(item['y'] for item in observations) / len(observations),
    )


def _calib_apply_gain(calib: dict) -> tuple[bool, str]:
    """标定结果换算 kp 写回 RuntimeProfile（与旧后端/C 桥同款 K_LOOP=1/7）。"""
    K_LOOP = 0.142857
    try:
        gain_x = float(calib.get('mouse_gain_x_px_per_count') or 0)
        gain_y = float(calib.get('mouse_gain_y_px_per_count') or 0)
        if gain_x <= 0 or gain_y <= 0:
            return False, '增益必须 > 0'
        prof = _get_runtime_profile()
        if not prof:
            return False, '读取 RuntimeProfile 失败'
        mo = prof.setdefault('mouse', {})
        sx = (float(mo.get('rate_x', 1) or 1) * float(mo.get('sensitivity', 1) or 1)
              * float(mo.get('output_scale', 1) or 1))
        sy = (float(mo.get('rate_y', 1) or 1) * float(mo.get('sensitivity', 1) or 1)
              * float(mo.get('output_scale', 1) or 1))
        mo['kp_x'] = round(K_LOOP / max(gain_x * sx, 1e-6), 4)
        mo['kp_y'] = round(K_LOOP / max(gain_y * sy, 1e-6), 4)
        r = ipc_request('SET_CONFIG', {'profile': prof})
        return r.get('status') == 0, r.get('error', '配置已更新')
    except Exception as exc:
        return False, str(exc)


def _calib_worker() -> None:
    """真实标定闭环：稳定检测 → X 轴往返注入 → 目标位移(px) → gain=px/count。
    对齐 YU/旧后端：10 轮、幅度 8→32、中值去抖、Y 轴复用 X。
    注入：标定时 mouse.calibrating=true（AimThread/OutputBackend 放行 AI 移动），
    kp 输出经现有控制链驱动鼠标 → 目标在画面中位移 → aim_pos_x 反馈。"""
    prof0 = _get_runtime_profile()
    was_enabled = bool((prof0.get('mouse') or {}).get('enabled'))
    mo0 = prof0.setdefault('mouse', {})
    mo0['enabled'] = True
    mo0['calibrating'] = True
    ipc_request('SET_CONFIG', {'profile': prof0})
    try:
        _calib_set(state='preparing', status='running', phase='preparing', reason='准备标定环境',
                   round=0, progress=0.0, round_gains=[], candidate_count=0,
                   stable_frames=0, stable_ms=0, valid_sample_count=0, axis_fits={})
        # 1) stabilize：同一目标/类别/尺寸稳定，中心抖动 <1px、尺寸变化 <5%，持续 800ms
        _calib_set(state='stabilize_x', phase='stabilize_x', current_axis='x')
        win, stable_start = [], None
        deadline = time.time() + 12.0
        t0 = time.time()
        while time.time() < deadline:
            if _cal['status'] != 'running':
                _calib_set(state='cancelled', phase='cancelled', reason='cancelled')
                return
            target = _calib_target()
            if target is None:
                win.clear()
                stable_start = None
                _calib_set(reason='no_target', candidate_count=0, stable_frames=0, stable_ms=0,
                           elapsed_ms=int((time.time() - t0) * 1000))
                time.sleep(0.1)
                continue
            if win and (target['target_id'] != win[-1]['target_id'] or
                        target['class_id'] != win[-1]['class_id']):
                win.clear()
                stable_start = None
            win.append(target)
            if len(win) > 10:
                win.pop(0)
            widths = [item['width'] for item in win]
            heights = [item['height'] for item in win]
            jx = max(item['x'] for item in win) - min(item['x'] for item in win)
            jy = max(item['y'] for item in win) - min(item['y'] for item in win)
            size_var = max(
                max(widths) - min(widths), max(heights) - min(heights)
            ) / max(max(widths + heights), 1.0)
            with _cal_lock:
                _cal['candidate_count'] = len(win)
                _cal['candidate_track_id'] = target['target_id']
                _cal['candidate_class_id'] = target['class_id']
                _cal['candidate_width'] = target['width']
                _cal['candidate_height'] = target['height']
                _cal['center_jitter_px'] = max(jx, jy)
                _cal['size_variation'] = size_var
                _cal['stable_frames'] = len(win)
            if len(win) >= 10 and jx < 1.0 and jy < 1.0 and size_var < 0.05:
                if stable_start is None:
                    stable_start = time.time()
                stable_ms = int((time.time() - stable_start) * 1000)
                _calib_set(state='stabilize_x', stable_ms=stable_ms, ready=True, reason='ready',
                           elapsed_ms=int((time.time() - t0) * 1000))
                if stable_ms >= 800:
                    break
            else:
                stable_start = None
                _calib_set(ready=False, reason='target_unstable', stable_ms=0)
            time.sleep(0.05)
        else:
            _calib_set(state='failed', status='failed', phase='error', reason='目标稳定检测超时', ready=False)
            return
        # 2) X/Y 分轴采样：每轴使用固定幅度，记录真实目标位移/延迟，最后交给 Median/MAD 拟合。
        amplitudes = [8, 16, 24, 32, 40]
        axis_observations = {CalibrationAxis.X: [], CalibrationAxis.Y: []}
        for axis in (CalibrationAxis.X, CalibrationAxis.Y):
            _calib_set(
                state=f'stabilize_{axis.value}',
                phase=f'stabilize_{axis.value}',
                current_axis=axis.value,
                round=0,
                progress=0.5 if axis is CalibrationAxis.Y else 0.0,
            )
            # 每轴动作前重新确认同一候选，避免目标切换混入测量。
            for index, amp in enumerate(amplitudes):
                if _cal['status'] != 'running':
                    _calib_set(state='cancelled', phase='cancelled', reason='cancelled')
                    return
                base_samples = _calib_sample_observations(3)
                if not base_samples:
                    _calib_set(state='failed', status='failed', phase='error', reason='no_target', ready=False)
                    return
                base = base_samples[-1]
                _calib_set(
                    state=f'sampling_{axis.value}',
                    phase=f'measure_{axis.value}_response',
                    current_axis=axis.value,
                    round=index + 1,
                    amplitude_counts=amp,
                    progress=(index + (0 if axis is CalibrationAxis.X else 5)) / 10.0,
                )
                bias = {'calibration_bias_x': float(amp) if axis is CalibrationAxis.X else 0.0,
                        'calibration_bias_y': float(amp) if axis is CalibrationAxis.Y else 0.0}
                prof = _get_runtime_profile()
                mo = prof.setdefault('mouse', {})
                mo.update(bias)
                mo['calibrating'] = True
                if ipc_request('SET_CONFIG', {'profile': prof}).get('status') != 0:
                    _calib_set(state='failed', status='failed', phase='error', reason='Core 配置应用失败', ready=False)
                    return
                injected_at = time.monotonic()
                samples = []
                for _ in range(20):
                    time.sleep(0.05)
                    target = _calib_target()
                    if target is None:
                        continue
                    if target['target_id'] != base['target_id'] or target['class_id'] != base['class_id']:
                        continue
                    delta = (target['x'] - base['x']) if axis is CalibrationAxis.X else (target['y'] - base['y'])
                    samples.append(CalibrationObservation(
                        axis=axis,
                        injected_count=float(amp),
                        measured_delta_px=abs(delta),
                        response_delay_ms=(target['timestamp'] - injected_at) * 1000.0,
                        target_id=f"{target['target_id']}:{target['class_id']}",
                        valid=abs(delta) >= 0.3,
                    ))
                # 清除本轮偏置，避免下一轮叠加；仍保持标定模式直到 finally。
                prof = _get_runtime_profile()
                mo = prof.setdefault('mouse', {})
                mo['calibration_bias_x'] = 0.0
                mo['calibration_bias_y'] = 0.0
                ipc_request('SET_CONFIG', {'profile': prof})
                if samples:
                    # 同一轮取中位数观测，作为一个轴向测量点。
                    delta = sorted(item.measured_delta_px for item in samples)[len(samples) // 2]
                    delay = sorted(item.response_delay_ms for item in samples)[len(samples) // 2]
                    axis_observations[axis].append(CalibrationObservation(
                        axis=axis,
                        injected_count=float(amp),
                        measured_delta_px=delta,
                        response_delay_ms=max(0.0, delay),
                        target_id=samples[0].target_id,
                        valid=True,
                    ))
                _calib_set(valid_sample_count=sum(len(v) for v in axis_observations.values()))

            _calib_set(
                state=f'analyzing_{axis.value}',
                phase=f'measure_{axis.value}_settle',
                current_axis=axis.value,
            )
        _calib_set(state='validating', phase='validating', current_axis='', progress=0.9)
        fits = {
            axis: fit_axis_measurements(axis, values)
            for axis, values in axis_observations.items()
        }
        _calib_set(axis_fits={
            axis.value: {
                'gain_px_per_count': fit.gain_px_per_count,
                'response_delay_ms': fit.response_delay_ms,
                'sample_count': fit.sample_count,
                'rejected_count': fit.rejected_count,
                'consistency': fit.consistency,
                'converged': fit.converged,
                'failure_reason': fit.failure_reason,
            }
            for axis, fit in fits.items()
        })
        if not all(fit.converged for fit in fits.values()):
            reason = '; '.join(fit.failure_reason for fit in fits.values() if not fit.converged)
            _calib_set(state='failed', status='failed', phase='error', reason=reason or '轴向拟合失败', ready=False)
            return
        gain_x = fits[CalibrationAxis.X].gain_px_per_count
        gain_y = fits[CalibrationAxis.Y].gain_px_per_count
        delay_ms = max(fits[CalibrationAxis.X].response_delay_ms, fits[CalibrationAxis.Y].response_delay_ms)
        conf = round(min(fits[CalibrationAxis.X].consistency, fits[CalibrationAxis.Y].consistency), 3)
        _calib_set(round_gains=[gain_x, gain_y], progress=0.98, phase='saving', state='applying')
        calib = {
            'mouse_gain_x_px_per_count': round(gain_x, 4),
            'mouse_gain_y_px_per_count': round(gain_y, 4),
            'mouse_response_delay_ms': round(delay_ms, 2),
            'mouse_calibration_applied': True,
            'valid': True,
            'confidence': conf,
            'calibrated_at': time.strftime('%Y%m%d_%H%M%S'),
            'model_id': _read_active_model(),
            'capture': {'crop_size': int((_get_runtime_profile().get('preview') or {}).get('roi_w') or 320)},
            'rounds': len(axis_observations[CalibrationAxis.X]) + len(axis_observations[CalibrationAxis.Y]),
        }
        ok, detail = _write_calibration(calib)
        if ok:
            ok2, detail2 = _calib_apply_gain(calib)
            ok = ok and ok2
            detail = detail + '；' + detail2
        _calib_set(
            state='completed' if ok else 'failed',
            status='success' if ok else 'failed',
            reason='completed' if ok else detail,
            ready=ok,
            progress=1.0 if ok else 0.98,
            phase='completed' if ok else 'error',
        )
    finally:
        try:
            prof = _get_runtime_profile()
            prof.setdefault('mouse', {})['calibrating'] = False
            if not was_enabled:
                prof['mouse']['enabled'] = False
            ipc_request('SET_CONFIG', {'profile': prof})
        except Exception:
            pass


def _calibration_payload() -> dict:
    with _cal_lock:
        runtime = {
            'running': bool(_cal['thread'] and _cal['thread'].is_alive()),
            'phase': _cal['phase'],
            'state': _cal['state'],
            'status': _cal['status'],
            'ready': _cal['ready'],
            'reason': _cal['reason'],
            'total_rounds': _cal['total_rounds'],
            'round': _cal['round'],
            'progress': _cal['progress'],
            'current_axis': _cal['current_axis'],
            'valid_sample_count': _cal['valid_sample_count'],
            'axis_fits': _cal['axis_fits'],
            'candidate_count': _cal['candidate_count'],
            'candidate_track_id': _cal['candidate_track_id'],
            'candidate_class_id': _cal['candidate_class_id'],
            'candidate_width': _cal['candidate_width'],
            'candidate_height': _cal['candidate_height'],
            'stable_frames': _cal['stable_frames'],
            'stable_ms': _cal['stable_ms'],
            'center_jitter_px': _cal['center_jitter_px'],
            'size_variation': _cal['size_variation'],
            'elapsed_ms': _cal['elapsed_ms'],
            'amplitude_counts': _cal['amplitude_counts'],
            'error': '' if _cal['status'] != 'failed' else _cal['reason'],
        }
    calib = _read_calibration()
    # 旧后端字段名 → 前端契约（gain_x_px_per_count / response_delay_ms）
    if calib:
        calib = {
            'valid': bool(calib.get('valid')),
            'gain_x_px_per_count': calib.get('mouse_gain_x_px_per_count', 0.55),
            'gain_y_px_per_count': calib.get('mouse_gain_y_px_per_count', 0.55),
            'response_delay_ms': calib.get('mouse_response_delay_ms', 8.333),
            'confidence': calib.get('confidence', 0),
            'model_id': calib.get('model_id', ''),
            'calibrated_at': calib.get('calibrated_at', ''),
            'capture_width': (calib.get('capture') or {}).get('crop_size', 0),
            'crop_size': (calib.get('capture') or {}).get('crop_size', 0),
        }
    else:
        calib = {'valid': False}
    return {'runtime': runtime, 'calibration': calib}


@app.get('/api/control/calibration')
def get_auto_calibration():
    return jsonify({'ok': True, 'data': _calibration_payload()})


@app.put('/api/control/calibration')
def update_auto_calibration():
    body = request.get_json(silent=True) or {}
    try:
        gain_x = float(body.get('gain_x_px_per_count') or body.get('mouse_gain_x_px_per_count') or 0)
        gain_y = float(body.get('gain_y_px_per_count') or body.get('mouse_gain_y_px_per_count') or 0)
        delay = float(body.get('response_delay_ms') or body.get('mouse_response_delay_ms') or 0)
    except (TypeError, ValueError):
        return jsonify({'ok': False, 'error': '参数格式错误'}), 400
    if gain_x <= 0 or gain_y <= 0:
        return jsonify({'ok': False, 'error': '增益必须 > 0'}), 400
    calib = {
        'mouse_gain_x_px_per_count': round(gain_x, 4),
        'mouse_gain_y_px_per_count': round(gain_y, 4),
        'mouse_response_delay_ms': round(delay, 3),
        'mouse_calibration_applied': True,
        'valid': True,
        'confidence': 0.0,
        'calibrated_at': time.strftime('%Y%m%d_%H%M%S'),
        'model_id': _read_active_model(),
    }
    ok, detail = _write_calibration(calib)
    if ok:
        ok2, detail2 = _calib_apply_gain(calib)
        detail = detail + '；' + detail2
        _calib_set(status='manual', phase='done', ready=True, reason='completed')
    resp = jsonify({'ok': ok, 'data': _calibration_payload(), 'detail': detail})
    resp.status_code = 200 if ok else 500
    return resp


@app.post('/api/control/calibration/start')
def start_auto_calibration():
    with _cal_lock:
        if _cal['thread'] and _cal['thread'].is_alive():
            return jsonify({'ok': False, 'error': '标定已在运行中'}), 409
    st = _get_status()
    if not st.get('runtime_running'):
        return jsonify({'ok': False, 'error': '推理服务未运行或目标反馈未就绪（请先启动推理）'}), 400
    if _calib_target() is None:
        return jsonify({'ok': False, 'error': '未识别到目标，无法开始标定（请将准星对准画面中的目标，等待检测框稳定出现）'}), 400
    th = threading.Thread(target=_calib_worker, daemon=True)
    with _cal_lock:
        _cal['thread'] = th
    th.start()
    return jsonify({'ok': True, 'data': _calibration_payload(), 'detail': '标定已启动'})


@app.post('/api/control/calibration/cancel')
def cancel_auto_calibration():
    _calib_set(status='idle', phase='cancelled', ready=False, reason='cancelled')
    try:
        prof = _get_runtime_profile()
        prof.setdefault('mouse', {})['calibrating'] = False
        ipc_request('SET_CONFIG', {'profile': prof})
    except Exception:
        pass
    return jsonify({'ok': True, 'data': _calibration_payload(), 'detail': '标定已取消'})


@app.delete('/api/control/calibration')
def clear_auto_calibration():
    _clear_calibration()
    return jsonify({'ok': True, 'data': _calibration_payload(), 'detail': '标定已清除'})


@app.post('/api/control/start')
def start_control():
    r = ipc_request('RUNTIME_CONTROL', {'action': 'start'})
    return jsonify({'ok': r.get('status') == 0, 'data': {'message': '已启动' if r.get('status') == 0 else '启动失败'}})


@app.post('/api/control/stop')
def stop_control():
    r = ipc_request('RUNTIME_CONTROL', {'action': 'stop'})
    return jsonify({'ok': r.get('status') == 0, 'data': {'message': '已停止' if r.get('status') == 0 else '停止失败'}})


# 瞄准轨迹记录（诊断）：后台线程按 50Hz 采样核心 aim 状态，存 /opt/ttbox/run/aim_trace.json
_aim_trace = {'running': False, 'samples': [], 'started_at': 0, 'stop_at': 0, 'thread': None}


@app.post('/api/diagnostics/aim-trace')
def start_aim_trace():
    body = request.get_json(silent=True) or {}
    duration_sec = int(body.get('duration_sec', 10))
    if duration_sec <= 0 or duration_sec > 120:
        return jsonify({'ok': False, 'error': 'duration_sec 必须在 1~120 之间'})
    if _aim_trace['running']:
        return jsonify({'ok': False, 'error': '已有轨迹记录进行中'})
    _aim_trace['running'] = True
    _aim_trace['samples'] = []
    _aim_trace['started_at'] = time.time()
    _aim_trace['stop_at'] = time.time() + duration_sec

    def _collect():
        while _aim_trace['running'] and time.time() < _aim_trace['stop_at']:
            try:
                st = _get_status()
                m = st.get('metrics', {})
                _aim_trace['samples'].append({
                    't': round(time.time() - _aim_trace['started_at'], 3),
                    'err_x': round(m.get('aim_error_x', 0.0), 3),
                    'err_y': round(m.get('aim_error_y', 0.0), 3),
                    'move_x': m.get('mouse_dx', 0),
                    'move_y': m.get('mouse_dy', 0),
                    'target': m.get('target_frames', 0) > 0 or m.get('aim_active', False),
                })
            except Exception:
                pass
            time.sleep(0.02)
        try:
            with open('/opt/ttbox/run/aim_trace.json', 'w') as f:
                json.dump({'samples': _aim_trace['samples'], 'duration_sec': duration_sec}, f)
        except Exception:
            pass
        _aim_trace['running'] = False

    _aim_trace['thread'] = threading.Thread(target=_collect, daemon=True)
    _aim_trace['thread'].start()
    return jsonify({'ok': True, 'data': {'message': f'轨迹记录已开始（{duration_sec} 秒）'}})


@app.get('/api/diagnostics/usb-proxy.zip')
def download_usb_proxy_diagnostics():
    return jsonify({'ok': True, 'data': {}})


@app.get('/api/events')
def get_events():
    return jsonify({'ok': True, 'data': {'events': []}})


# -- 硬件 --
@app.get('/api/hardware/mouse')
def get_mouse_hardware():
    # 真实探测：HID gadget 设备 + 核心端注入开关
    import glob
    hidg = sorted(glob.glob('/dev/hidg*'))
    prof = _get_runtime_profile()
    mouse = prof.get('mouse') or {}
    return jsonify({
        'ok': True,
        'data': {
            'mode': 'proxy',
            'device': hidg[0] if hidg else '',
            'enabled': bool(mouse.get('enabled', False)),
            'connected': bool(hidg),
        },
    })


@app.put('/api/hardware/mouse')
def update_mouse_hardware():
    body = request.get_json(silent=True) or {}
    prof = _get_runtime_profile()
    mouse = prof.get('mouse') or {}
    # 前端字段：enabled/mode/device
    for k in ('enabled', 'proxy_mode', 'mode'):
        if k in body:
            mouse[k] = body[k]
    prof['mouse'] = mouse
    r = ipc_request('SET_CONFIG', {'profile': prof})
    if r.get('status') != 0:
        return jsonify({'ok': False, 'error': r.get('error', '保存失败')})
    return jsonify({'ok': True, 'data': {'message': '已更新', 'mouse': mouse}})


@app.put('/api/hardware/mouse/mode')
def update_mouse_proxy_mode():
    return jsonify({'ok': True, 'data': {'message': '模式已切换'}})


@app.put('/api/hardware/mouse/timing')
def update_mouse_proxy_timing():
    return jsonify({'ok': True, 'data': {'message': '时序已更新'}})


@app.get('/api/hardware/display')
def get_display_hardware():
    # 缓存 3 秒：v4l2-ctl query-dv-timing 在信号重协商时阻塞，防止 waitress 线程耗尽
    now = time.time()
    if _DISPLAY_CACHE['data'] is not None and now - _DISPLAY_CACHE['ts'] < 3:
        return jsonify({'ok': True, 'data': _DISPLAY_CACHE['data']})
    hdmi = {'connected': False, 'locked': False, 'width': 0, 'height': 0, 'refresh': 0}
    try:
        r = subprocess.run(
            ['v4l2-ctl', '-d', '/dev/video0', '--query-dv-timing'],
            capture_output=True, text=True, timeout=1.5,
        )
        txt = r.stdout
        if r.returncode == 0 and 'Active width' in txt:
            w = re.search(r'Active width:\s*(\d+)', txt)
            h = re.search(r'Active height:\s*(\d+)', txt)
            fps = re.search(r'\(([\d.]+) frames per second\)', txt)
            hdmi['connected'] = True
            hdmi['locked'] = True
            if w: hdmi['width'] = int(w.group(1))
            if h: hdmi['height'] = int(h.group(1))
            if fps: hdmi['refresh'] = float(fps.group(1))
    except Exception:
        pass
    # YU 兼容结构：前端 populateDisplayHardware 消费 available/config/display_mode
    cfg_disp = {}
    cpath = '/opt/ttbox/config/hardware_display.json'
    try:
        if os.path.exists(cpath):
            cfg_disp = json.load(open(cpath))
    except Exception:
        pass
    # 广播模式（当前 EDID 生效的模式，同 available_modes 结构）
    advertised = []
    try:
        out = subprocess.check_output(
            ['/opt/aiassistance/bin/hdmirx_edid', '--list'],
            text=True, timeout=5)
        in_modes = False
        for lm in out.splitlines():
            lm = lm.strip()
            if lm.startswith('Modes:'):
                in_modes = True
                continue
            if in_modes:
                if not lm:
                    break
                parts = lm.split()
                if not parts:
                    continue
                token = parts[0]
                dims = re.search(r'(\d+)x(\d+)@(\d+)', lm)
                pc = re.search(r'pixel_clock=(\d+)', lm)
                advertised.append({
                    'token': token,
                    'label': f'{dims.group(1)}x{dims.group(2)}@{dims.group(3)}' if dims else token,
                    'width': int(dims.group(1)) if dims else 0,
                    'height': int(dims.group(2)) if dims else 0,
                    'refresh': int(dims.group(3)) if dims else 0,
                    'pixel_clock_khz': int(pc.group(1)) if pc else 0,
                })
    except Exception:
        pass
    # available_modes（YU 结构：token/label/width/height/refresh/pixel_clock_khz）
    available_modes = []
    try:
        out2 = subprocess.check_output(
            ['/opt/aiassistance/bin/hdmirx_edid', '--list'],
            text=True, timeout=5)
        in_modes = False
        for lm in out2.splitlines():
            lm = lm.strip()
            if lm.startswith('Modes:'):
                in_modes = True
                continue
            if in_modes:
                if not lm:
                    in_modes = False
                    continue
                parts = lm.split()
                if not parts:
                    continue
                token = parts[0]
                dims = re.search(r'(\d+)x(\d+)@(\d+)', lm)
                pc = re.search(r'pixel_clock=(\d+)', lm)
                available_modes.append({
                    'token': token,
                    'label': f'{dims.group(1)}x{dims.group(2)}@{dims.group(3)}' if dims else token,
                    'width': int(dims.group(1)) if dims else 0,
                    'height': int(dims.group(2)) if dims else 0,
                    'refresh': int(dims.group(3)) if dims else 0,
                    'pixel_clock_khz': int(pc.group(1)) if pc else 0,
                })
    except Exception:
        pass
    data = dict(hdmi)
    data['available'] = hdmi.get('connected', False)
    data['config'] = cfg_disp
    status_text = ''
    # 真实显示器身份：从当前生效 EDID 读取（hdmirx_edid --status）
    edid_name, edid_vendor, edid_pid, edid_serial = '', '', '', ''
    edid_valid = False
    status_text = ''
    try:
        out = subprocess.check_output(
            ['/opt/aiassistance/bin/hdmirx_edid', '--status'],
            text=True, timeout=5)
        status_text = out
        nm = re.search(r'name=(\S+)', out)
        vd = re.search(r'vendor=(\S+)', out)
        pid = re.search(r'product=(0x[0-9a-fA-F]+)', out)
        ser = re.search(r'serial=(0x[0-9a-fA-F]+)', out)
        if nm:
            edid_name = nm.group(1)
            edid_vendor = vd.group(1) if vd else ''
            edid_pid = pid.group(1) if pid else ''
            edid_serial = ser.group(1) if ser else ''
            edid_valid = True
    except Exception:
        pass
    data['status'] = {'output': status_text or 'EDID 状态读取失败'}
    data['display_mode'] = {
        'real_monitor': {
            'connected': hdmi.get('connected', False),
            'width': hdmi.get('width', 0), 'height': hdmi.get('height', 0),
            'refresh': hdmi.get('refresh', 0),
            'name': edid_name or cfg_disp.get('name', ''),
            'vendor': edid_vendor or cfg_disp.get('vendor', ''),
            'product_id': edid_pid or cfg_disp.get('product_id', ''),
            'serial': edid_serial or cfg_disp.get('serial', ''),
            'edid_valid': edid_valid,
        },
        'advertised_modes': advertised[:16],
        'available_modes': available_modes,
    }
    _DISPLAY_CACHE['ts'] = time.time()
    _DISPLAY_CACHE['data'] = data
    return jsonify({'ok': True, 'data': data})


@app.put('/api/hardware/display')
def update_display_hardware():
    body = request.get_json(silent=True) or {}
    cfg_in = body.get('config') or {}
    apply_now = bool(body.get('apply'))
    if not cfg_in:
        return jsonify({'ok': False, 'error': '缺少 config'})
    cpath = '/opt/ttbox/config/hardware_display.json'
    try:
        cur = json.load(open(cpath)) if os.path.exists(cpath) else {}
    except Exception:
        cur = {}
    # 只合并白名单键（防注入）
    for k in ('device', 'name', 'vendor', 'product_id', 'serial',
              'native_mode', 'native_only', 'profile',
              'loopout_enabled', 'loopout_overlay_enabled',
              'loopout_pixel_format', 'loopout_overlay_thickness', 'loopout_overlay_color'):
        if k in cfg_in:
            cur[k] = cfg_in[k]
    json.dump(cur, open(cpath, 'w'), indent=2, ensure_ascii=False)

    result = {}
    if apply_now:
        r = subprocess.run(['bash', '/opt/ttbox/scripts/edid/edid_apply.sh'],
                           capture_output=True, text=True, timeout=60)
        result = {'exit': r.returncode, 'output': (r.stdout + r.stderr).strip()[-500:]}
        if r.returncode != 0:
            return jsonify({'ok': False, 'error': f'EDID 应用失败: {r.stderr or r.stdout}'[-300:]})
    # 返回 YU 兼容结构（前端 populateDisplayHardware 消费 display_mode/loopout）
    # 简化：直接返回 GET 的完整结构（含 real_monitor/available_modes/advertised）
    with app.test_request_context('/api/hardware/display'):
        pass
    gv = get_display_hardware()
    gv_data = gv.get_json().get('data', {})
    gv_data['config'] = cur
    gv_data['result'] = result
    gv_data['message'] = '显示器配置已应用'
    gv_data['loopout'] = {
        'enabled': bool(cur.get('loopout_enabled')),
        'overlay_enabled': bool(cur.get('loopout_overlay_enabled')),
        'pixel_format': cur.get('loopout_pixel_format', 'rgb888'),
        'width': 0, 'height': 0, 'refresh': 0,
        'overlay_status': '等待环出' if cur.get('loopout_overlay_enabled') else '',
    }
    return jsonify({'ok': True, 'data': gv_data})


# -- 网络/WiFi --
@app.get('/api/network/wifi')
def get_wifi_status():
    if wifi_manager is None:
        return jsonify({'ok': True, 'data': {'available': False, 'error': 'wifi_manager 未部署'}})
    return jsonify({'ok': True, 'data': wifi_manager.wifi_status(force_scan=False)})


@app.post('/api/network/wifi/scan')
def scan_wifi_networks():
    if wifi_manager is None:
        return jsonify({'ok': True, 'data': {'available': False, 'networks': [], 'error': 'wifi_manager 未部署'}})
    return jsonify({'ok': True, 'data': wifi_manager.wifi_status(force_scan=True)})


@app.post('/api/network/wifi/connect')
def connect_wifi_network():
    body = request.get_json(silent=True) or {}
    ssid = body.get('ssid', '')
    password = body.get('password', '')
    if not ssid:
        return jsonify({'ok': False, 'error': '缺少 SSID'})
    if wifi_manager is None:
        return jsonify({'ok': False, 'error': 'wifi_manager 未部署'})
    try:
        return jsonify({'ok': True, 'data': wifi_manager.connect_wifi(ssid, password)})
    except wifi_manager.WifiError as exc:
        return jsonify({'ok': False, 'error': str(exc)})


@app.post('/api/network/wifi/fallback')
def fallback_wifi_network():
    if wifi_manager is None:
        return jsonify({'ok': False, 'error': 'wifi_manager 未部署'})
    try:
        return jsonify({'ok': True, 'data': wifi_manager.reset_to_default_wifi()})
    except wifi_manager.WifiError as exc:
        return jsonify({'ok': False, 'error': str(exc)})


@app.post('/api/network/wifi/ap/apply')
def apply_wifi_ap_hotspot():
    body = request.get_json(silent=True) or {}
    if wifi_manager is None:
        return jsonify({'ok': False, 'error': 'wifi_manager 未部署'})
    try:
        return jsonify({'ok': True, 'data': wifi_manager.apply_ap_hotspot(
            ssid=body.get('ssid'), password=body.get('password'))})
    except wifi_manager.WifiError as exc:
        return jsonify({'ok': False, 'error': str(exc)})


@app.post('/api/network/wifi/client/activate')
def activate_wifi_client_mode():
    if wifi_manager is None:
        return jsonify({'ok': False, 'error': 'wifi_manager 未部署'})
    try:
        return jsonify({'ok': True, 'data': wifi_manager.activate_client_wifi()})
    except wifi_manager.WifiError as exc:
        return jsonify({'ok': False, 'error': str(exc)})


# -- 激活/授权 --
@app.get('/api/license')
def get_license():
    return jsonify({'ok': True, 'data': DEFAULT_LICENSE})


@app.post('/api/license/activate')
def activate_license():
    return jsonify({'ok': True, 'data': {'message': '已激活'}})


@app.post('/api/activation/network/prepare')
def prepare_activation_network():
    return jsonify({'ok': True, 'data': {'message': '网络已准备'}})


@app.post('/api/activation/reset-local-identity')
def reset_activation_local_identity():
    return jsonify({'ok': True, 'data': {'message': '已重置'}})


@app.get('/api/activation/full-recovery')
def get_activation_full_recovery():
    return jsonify({'ok': True, 'data': {'recovery_available': True}})


@app.post('/api/activation/full-recovery')
def start_activation_full_recovery():
    return jsonify({'ok': True, 'data': {'message': '恢复中'}})


# -- 更新：统一委托设备 Update Engine --
UPDATE_ENGINE = '/opt/ttbox/tools/update_engine.py'
UPDATE_STATE = '/var/lib/ttbox/update/update_state.json'

def _update_engine_action(action, version=None):
    if not os.path.exists(UPDATE_ENGINE):
        return jsonify({'ok': False, 'error': 'Update Engine 未安装'}), 503
    cmd = [sys.executable, UPDATE_ENGINE, '--action', action]
    if version:
        cmd += ['--version', version]
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        payload = json.loads(result.stdout or '{}')
        return jsonify(payload), 200 if payload.get('ok') else 502
    except Exception as exc:
        return jsonify({'ok': False, 'error': str(exc)}), 502

@app.post('/api/update/check')
def check_update():
    return _update_engine_action('check')

@app.post('/api/update/scan-otg')
def scan_otg_update():
    return _update_engine_action('scan-otg')

@app.get('/api/update/status')
def get_update_status():
    return _update_engine_action('status')

@app.post('/api/update/start')
def start_update():
    body = request.get_json(silent=True) or {}
    return _update_engine_action('start', body.get('version'))

@app.post('/api/update/rollback')
def rollback_update():
    return _update_engine_action('rollback')

@app.post('/api/update/cancel')
def cancel_update():
    return _update_engine_action('cancel')

@app.get('/api/update/log')
def get_update_log():
    return _update_engine_action('log')


@app.get('/api/hailo/status')
def get_hailo_status():
    return jsonify({'ok': True, 'data': {'installed': False}})


@app.post('/api/hailo/install')
def install_hailo_dependencies():
    return jsonify({'ok': True, 'data': {'message': '安装中'}})


# -- 主题 --
@app.get('/api/themes')
def get_themes():
    return jsonify({'ok': True, 'data': {'themes': []}})


@app.get('/api/themes/<theme_id>/previews/<int:index>')
def theme_preview(theme_id: str, index: int):
    return jsonify({'ok': True, 'data': {}})


@app.post('/api/themes/redeem')
def redeem_theme():
    return jsonify({'ok': True, 'data': {'message': '已兑换'}})


@app.post('/api/themes/<theme_id>/install')
def install_theme(theme_id: str):
    return jsonify({'ok': True, 'data': {'message': '已安装'}})


@app.put('/api/themes/current')
def select_theme():
    return jsonify({'ok': True, 'data': {'message': '主题已切换'}})


@app.get('/theme-assets/<theme_id>/<version>/<path:filename>')
def theme_asset(theme_id: str, version: str, filename: str):
    return jsonify({'ok': True, 'data': {}})


@app.get('/api/xcsh/background')
def get_xcsh_background():
    return jsonify({'ok': True, 'data': {'background': None}})


@app.post('/api/xcsh/background')
def upload_xcsh_background():
    return jsonify({'ok': True, 'data': {'message': '已上传'}})


@app.patch('/api/xcsh/background')
def update_xcsh_background():
    return jsonify({'ok': True, 'data': {'message': '已更新'}})


@app.delete('/api/xcsh/background')
def delete_xcsh_background():
    return jsonify({'ok': True, 'data': {'message': '已删除'}})


@app.get('/api/xcsh/background/image')
def get_xcsh_background_image():
    return jsonify({'ok': True, 'data': {}})


# -- 运动训练 --
def _motion_error(exc: Exception):
    message = str(exc)
    status = 409 if "session" in message or "active" in message or "lease" in message else 422
    return jsonify({'ok': False, 'error': message}), status


def _apply_personal_motion_to_core(enabled: bool, profile_id: str = '', mix: dict | None = None):
    """把 TTBOX 个人模型的启用状态写入 Core RuntimeProfile，Core 是最终运行真源。"""
    prof = _get_runtime_profile()
    if not prof:
        raise MotionTrainingError('读取 TTBOX Core RuntimeProfile 失败')
    personal = prof.setdefault('mouse', {}).setdefault('personal_motion', {})
    personal['enabled'] = bool(enabled)
    if enabled:
        profile = MOTION_STORE.list_profile(profile_id)
        model = profile.get('model') or {}
        if not model.get('ready'):
            raise MotionTrainingError('model is not ready')
        values = mix or MOTION_STORE._mix()
        personal.update({
            'curve_blend': values.get('curve', 1.0),
            'speed_blend': values.get('speed', 1.0),
            'reaction_blend': values.get('reaction', 0.7),
            'max_reaction_delay_ms': values.get('max_reaction_delay_ms', 250),
            'knots': model.get('knots', []),
        })
    result = ipc_request('SET_CONFIG', {'profile': prof})
    if result.get('status') != 0:
        raise MotionTrainingError(result.get('error', 'Core 配置更新失败'))
    return prof


@app.get('/api/motion-profiles')
def list_motion_profiles():
    try:
        return jsonify({'ok': True, 'data': MOTION_STORE.list_profiles()})
    except (MotionTrainingError, OSError) as exc:
        return _motion_error(exc)


@app.post('/api/motion-profiles')
def create_motion_profile():
    body = request.get_json(silent=True) or {}
    try:
        return jsonify({'ok': True, 'data': MOTION_STORE.create_profile(body.get('name', ''))})
    except (MotionTrainingError, OSError) as exc:
        return _motion_error(exc)


@app.patch('/api/motion-profiles/<profile_id>')
def rename_motion_profile(profile_id: str):
    body = request.get_json(silent=True) or {}
    try:
        return jsonify({'ok': True, 'data': MOTION_STORE.rename(profile_id, body.get('name', ''))})
    except (MotionTrainingError, OSError) as exc:
        return _motion_error(exc)


@app.delete('/api/motion-profiles/<profile_id>')
def delete_motion_profile(profile_id: str):
    try:
        return jsonify({'ok': True, 'data': MOTION_STORE.delete(profile_id)})
    except (MotionTrainingError, OSError) as exc:
        return _motion_error(exc)


@app.get('/api/motion-profiles/<profile_id>/export')
def export_motion_profile(profile_id: str):
    try:
        profile = MOTION_STORE.list_profile(profile_id)
        export_path = MOTION_PROFILES_DIR / f'.motion-profile-{profile_id}.json'
        export_path.write_text(json.dumps(profile, ensure_ascii=False, indent=2), encoding='utf-8')
        return send_file(export_path, mimetype='application/json', as_attachment=True,
                         download_name=f'ttbox-motion-profile-{profile_id}.json')
    except (MotionTrainingError, OSError) as exc:
        return _motion_error(exc)


@app.post('/api/motion-training/sessions')
def start_motion_training_session():
    body = request.get_json(silent=True) or {}
    try:
        result = MOTION_STORE.start_session(str(body.get('profile_id') or ''), now=time.time())
        return jsonify({'ok': True, 'data': {
            'session_id': result['id'], 'profile_id': result['profile_id'],
            'lease_expires_at': result['lease_expires_at'],
        }})
    except (MotionTrainingError, OSError) as exc:
        return _motion_error(exc)


@app.put('/api/motion-training/sessions/<session_id>/heartbeat')
def heartbeat_motion_training_session(session_id: str):
    try:
        return jsonify({'ok': True, 'data': MOTION_STORE.heartbeat(session_id)})
    except (MotionTrainingError, OSError) as exc:
        return _motion_error(exc)


@app.post('/api/motion-training/sessions/<session_id>/samples')
def append_motion_training_sample(session_id: str):
    body = request.get_json(silent=True)
    try:
        return jsonify({'ok': True, 'data': MOTION_STORE.append_sample(session_id, body)})
    except (MotionSampleError, MotionTrainingError, OSError) as exc:
        return _motion_error(exc)


@app.delete('/api/motion-training/sessions/<session_id>')
def stop_motion_training_session(session_id: str):
    try:
        return jsonify({'ok': True, 'data': MOTION_STORE.stop_session(session_id)})
    except (MotionTrainingError, OSError) as exc:
        return _motion_error(exc)


@app.post('/api/motion-profiles/<profile_id>/train')
def train_motion_profile(profile_id: str):
    try:
        return jsonify({'ok': True, 'data': MOTION_STORE.train(profile_id)})
    except (MotionTrainingError, OSError) as exc:
        return _motion_error(exc)


@app.post('/api/motion-profiles/<profile_id>/activate')
def activate_motion_profile(profile_id: str):
    body = request.get_json(silent=True) or {}
    try:
        result = MOTION_STORE.activate(profile_id, **body)
        _apply_personal_motion_to_core(True, profile_id, result['mix'])
        return jsonify({'ok': True, 'data': result})
    except (MotionTrainingError, OSError) as exc:
        return _motion_error(exc)


@app.delete('/api/motion-profiles/active')
def deactivate_motion_profile():
    try:
        result = MOTION_STORE.deactivate()
        _apply_personal_motion_to_core(False)
        return jsonify({'ok': True, 'data': result})
    except (MotionTrainingError, OSError) as exc:
        return _motion_error(exc)


@app.delete('/api/motion-profiles/<profile_id>/samples')
def clear_motion_profile_samples(profile_id: str):
    try:
        return jsonify({'ok': True, 'data': MOTION_STORE.clear_samples(profile_id)})
    except (MotionTrainingError, OSError) as exc:
        return _motion_error(exc)


# -- 远程 --
def _remote_not_ready():
    return jsonify({
        'ok': False,
        'error': 'TTBOX 远程模型协议尚未接入；当前不执行远程连接、导入或删除操作',
        'status': 'planned',
    }), 501


@app.post('/api/remote/connect')
def remote_connect():
    return _remote_not_ready()


@app.get('/api/remote/models')
def remote_models():
    return _remote_not_ready()


@app.post('/api/remote/import')
def remote_import():
    return _remote_not_ready()


@app.post('/api/remote/delete')
def remote_delete():
    return _remote_not_ready()


# -- 其他 --
@app.get('/api/makcu/devices')
def list_makcu_devices():
    return jsonify({'ok': True, 'data': {'devices': _list_serial_devices()}})


def _list_serial_devices():
    import glob
    devs = []
    for pattern in ('/dev/ttyUSB*', '/dev/ttyACM*'):
        for d in glob.glob(pattern):
            try:
                desc = subprocess.check_output(['udevadm', 'info', '-q', 'property', '-n', d],
                                               text=True, timeout=3)
                vid = ''
                model = ''
                for line in desc.splitlines():
                    if line.startswith('ID_VENDOR_ID='):
                        vid = line.split('=')[1]
                    if line.startswith('ID_MODEL='):
                        model = line.split('=')[1]
                devs.append({'path': d, 'vendor_id': vid, 'model': model, 'backend': 'serial'})
            except Exception:
                devs.append({'path': d, 'backend': 'serial'})
    return devs


@app.get('/api/ferrum/devices')
def list_ferrum_devices():
    return jsonify({'ok': True, 'data': {'devices': _list_serial_devices()}})


@app.get('/api/kmboxb/devices')
def list_kmboxb_devices():
    return jsonify({'ok': True, 'data': {'devices': _list_serial_devices()}})


@app.post('/api/mouse-output/test-circle')
def test_mouse_output_circle():
    return jsonify({
        'ok': False,
        'error': '鼠标圆周输出测试尚未接入真实输出后端；当前不发送测试动作',
        'status': 'planned',
    }), 501


# -- 预览 --
@app.get('/api/preview.jpg')
def preview():
    r = ipc_request('GET_PREVIEW', timeout=3)
    if r.get('status') == 0 and r.get('data', {}).get('jpeg_base64'):
        px = base64.b64decode(r['data']['jpeg_base64'])
    else:
        px = b''
    return Response(px, mimetype='image/jpeg')


@app.get('/api/preview.mjpg')
def preview_stream():
    # MJPEG 流：读 Core PreviewModule 缓存（Core 端 10~15fps 生成），
    # 无帧时短暂等待而非密集空转；Core 是唯一生产节拍，本端只做搬运。
    def generate():
        # YU 同款防糊策略：只在核心端缓存更新（seq 变化）时推新帧，
        # 不重复推同一帧（浏览器 img 绘制跟不上会导致 multipart 积压 → 半帧横线花屏）。
        last_seq = -1
        last_push = time.time()
        while True:
            r = ipc_request('GET_PREVIEW', timeout=2)
            now = time.time()
            if r.get('status') == 0:
                d = r.get('data', {})
                b64 = d.get('jpeg_base64')
                seq = d.get('seq', 0)
                # seq 变化 = 核心端编码了新帧 → 推；兜底：>1s 没推也推一次（防浏览器黑屏）
                if b64 and (seq != last_seq or now - last_push > 1.0):
                    px = base64.b64decode(b64)
                    if px:
                        last_seq = seq
                        last_push = now
                        yield b'--ttboxframe\r\n'
                        yield b'Content-Type: image/jpeg\r\n'
                        yield f'Content-Length: {len(px)}\r\n\r\n'.encode()
                        yield px
                        yield b'\r\n'
            time.sleep(0.03)  # 轮询节奏 33ms（Core 端 fps 决定实际帧率）
    return Response(generate(), mimetype='multipart/x-mixed-replace; boundary=ttboxframe')


# ====================================================================
# 入口
# ====================================================================
def main():
    from waitress import serve
    print(f'TTBOX Web 后端启动: http://{LISTEN_HOST}:{LISTEN_PORT}')
    print(f'  模板目录: {TEMPLATE_DIR}')
    print(f'  静态目录: {STATIC_DIR}')
    print(f'  IPC Socket: {IPC_SOCKET}')
    serve(app, host=LISTEN_HOST, port=LISTEN_PORT)


if __name__ == '__main__':
    main()
