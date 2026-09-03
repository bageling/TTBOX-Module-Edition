"""TTBOX 个人移动曲线训练领域模型。

这是 TTBOX 自己的本地数据层，不依赖任何外部产品、daemon 或 ABI。
"""
from __future__ import annotations

import json
import math
import os
import re
import time
import uuid
from pathlib import Path
from typing import Any


class MotionSampleError(ValueError):
    """训练样本不符合 TTBOX 协议。"""


class MotionTrainingError(ValueError):
    """训练档案或会话状态不允许执行操作。"""


PROFILE_ID_RE = re.compile(r"[a-z0-9][a-z0-9._-]{0,63}")
SAMPLE_MAX_BYTES = 256 * 1024
POINTS_MIN = 2
POINTS_MAX = 2048
SESSION_LEASE_SECONDS = 30.0


def _finite(value: Any, name: str) -> float:
    if isinstance(value, bool):
        raise MotionSampleError(f"{name} must be finite")
    try:
        result = float(value)
    except (TypeError, ValueError):
        raise MotionSampleError(f"{name} must be finite") from None
    if not math.isfinite(result):
        raise MotionSampleError(f"{name} must be finite")
    return result


def _point(value: Any, name: str) -> tuple[float, float]:
    if not isinstance(value, dict):
        raise MotionSampleError(f"{name} must be an object")
    return _finite(value.get("x"), f"{name}.x"), _finite(value.get("y"), f"{name}.y")


def validate_motion_sample(payload: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(payload, dict):
        raise MotionSampleError("sample must be an object")
    if payload.get("schema") != "ttbox.motion-sample.v1":
        raise MotionSampleError("schema must be ttbox.motion-sample.v1")
    mode = payload.get("mode")
    if mode not in {"reaction", "continuous"}:
        raise MotionSampleError("mode is invalid")
    if payload.get("completion") != "dwell":
        raise MotionSampleError("completion is invalid")
    canvas = payload.get("canvas")
    if not isinstance(canvas, dict):
        raise MotionSampleError("canvas is required")
    width = _finite(canvas.get("width"), "canvas.width")
    height = _finite(canvas.get("height"), "canvas.height")
    if not (64 <= width <= 4096 and 64 <= height <= 4096):
        raise MotionSampleError("canvas is outside the accepted range")
    start = _point(payload.get("start"), "start")
    target = _point(payload.get("target"), "target")
    radius = _finite(payload.get("radius"), "radius")
    if not 1 <= radius <= 512:
        raise MotionSampleError("radius is outside the accepted range")
    points = payload.get("points")
    if not isinstance(points, list) or not POINTS_MIN <= len(points) <= POINTS_MAX:
        raise MotionSampleError("point count is invalid")
    encoded = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(encoded) > SAMPLE_MAX_BYTES:
        raise MotionSampleError("motion sample exceeds 256KB")

    x, y = start
    total_ms = 0.0
    path_length = 0.0
    for index, item in enumerate(points):
        if not isinstance(item, dict):
            raise MotionSampleError(f"point {index} is invalid")
        dt = _finite(item.get("dt"), f"points[{index}].dt")
        dx = _finite(item.get("dx"), f"points[{index}].dx")
        dy = _finite(item.get("dy"), f"points[{index}].dy")
        if not 0 <= dt <= 2000:
            raise MotionSampleError("point dt is outside the accepted range")
        x += dx
        y += dy
        if not (0 <= x <= width and 0 <= y <= height):
            raise MotionSampleError("motion path leaves canvas bounds")
        total_ms += dt
        path_length += math.hypot(dx, dy)
    if total_ms > 120000:
        raise MotionSampleError("motion sample duration is too long")
    if math.hypot(x - target[0], y - target[1]) > radius:
        raise MotionSampleError("motion path did not finish inside target")
    straight = math.hypot(target[0] - start[0], target[1] - start[1])
    return {
        "mode": mode,
        "point_count": len(points),
        "duration_ms": total_ms,
        "path_length": path_length,
        "path_efficiency": straight / path_length if path_length > 0 else 0.0,
        "reaction_ms": total_ms if mode == "reaction" else None,
    }


def _atomic_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    temp.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    os.replace(temp, path)


class MotionProfileStore:
    def __init__(self, root: str | Path):
        self.root = Path(root)
        self.root.mkdir(parents=True, exist_ok=True)
        self._session: dict[str, Any] | None = None

    def _path(self, profile_id: str) -> Path:
        if not PROFILE_ID_RE.fullmatch(profile_id):
            raise MotionTrainingError("profile_id is invalid")
        return self.root / profile_id / "profile.json"

    def _read(self, profile_id: str) -> dict[str, Any]:
        path = self._path(profile_id)
        if not path.exists():
            raise MotionTrainingError("profile does not exist")
        try:
            return json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            raise MotionTrainingError(f"profile is damaged: {exc}") from exc

    def _write(self, profile: dict[str, Any]) -> None:
        _atomic_json(self._path(profile["id"]), profile)

    @staticmethod
    def _summary(profile: dict[str, Any]) -> dict[str, Any]:
        samples = profile.get("samples", [])
        reaction = [s["statistics"] for s in samples if s["mode"] == "reaction"]
        continuous = [s["statistics"] for s in samples if s["mode"] == "continuous"]
        durations = [float(s["duration_ms"]) for s in reaction]
        efficiencies = [float(s["path_efficiency"]) for s in samples]
        profile["sample_count"] = len(samples)
        profile["reaction_count"] = len(reaction)
        profile["continuous_count"] = len(continuous)
        profile["statistics"] = {
            "reaction_mean_ms": sum(durations) / len(durations) if durations else None,
            "path_efficiency": sum(efficiencies) / len(efficiencies) if efficiencies else None,
        }

    def create_profile(self, name: str) -> dict[str, Any]:
        name = str(name or "").strip()
        if not name:
            raise MotionTrainingError("name is required")
        index = 1
        while (self.root / f"profile-{index}").exists():
            index += 1
        now = int(time.time() * 1000)
        profile = {
            "schema": "ttbox.motion-profile.v1",
            "id": f"profile-{index}",
            "name": name,
            "created_at_ms": now,
            "updated_at_ms": now,
            "samples": [],
            "model": {"schema": "ttbox.motion-model.v1", "ready": False, "quality": 0, "knots": []},
            "statistics": {"reaction_mean_ms": None, "path_efficiency": None},
        }
        self._summary(profile)
        self._write(profile)
        return profile

    def list_profiles(self) -> dict[str, Any]:
        profiles = []
        for path in sorted(self.root.glob("*/profile.json")):
            try:
                profile = json.loads(path.read_text(encoding="utf-8"))
                self._summary(profile)
                profiles.append(profile)
            except (OSError, json.JSONDecodeError):
                continue
        if not profiles:
            profiles = [self.create_profile("默认曲线")]
        active = self._active_id()
        profiles.sort(key=lambda item: (item.get("id") != active, item.get("id", "")))
        return {"profiles": [self._public(p) for p in profiles], "active_profile_id": active or "", "enabled": bool(active), "mix": self._mix(), "session": self._session_public()}

    def list_profile(self, profile_id: str) -> dict[str, Any]:
        return self._read(profile_id)

    def rename(self, profile_id: str, name: str) -> dict[str, Any]:
        profile = self._read(profile_id)
        name = str(name or "").strip()
        if not name:
            raise MotionTrainingError("name is required")
        profile["name"] = name
        profile["updated_at_ms"] = int(time.time() * 1000)
        self._write(profile)
        return self._public(profile)

    def delete(self, profile_id: str) -> dict[str, Any]:
        self._read(profile_id)
        if self._active_id() == profile_id:
            self.deactivate()
        import shutil
        shutil.rmtree(self.root / profile_id)
        return {"deleted": True, "profile_id": profile_id}

    def clear_samples(self, profile_id: str) -> dict[str, Any]:
        profile = self._read(profile_id)
        profile["samples"] = []
        profile["model"] = {"schema": "ttbox.motion-model.v1", "ready": False, "quality": 0, "knots": []}
        self._summary(profile)
        profile["updated_at_ms"] = int(time.time() * 1000)
        self._write(profile)
        return self._public(profile)

    @staticmethod
    def _public(profile: dict[str, Any]) -> dict[str, Any]:
        result = dict(profile)
        result.pop("samples", None)
        result["model"] = dict(result.get("model") or {})
        result["model"]["exists"] = bool(result["model"].get("knots"))
        return result

    def _active_id(self) -> str:
        path = self.root / "active.json"
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
            return str(data.get("profile_id") or "")
        except (OSError, json.JSONDecodeError):
            return ""

    def _mix(self) -> dict[str, Any]:
        try:
            return json.loads((self.root / "active.json").read_text(encoding="utf-8")).get("mix", {})
        except (OSError, json.JSONDecodeError):
            return {"curve": 1.0, "speed": 1.0, "reaction": 0.7, "max_reaction_delay_ms": 250}

    def _session_public(self) -> dict[str, Any]:
        if not self._session:
            return {"active": False, "id": "", "lease_remaining_ms": 0, "profile_id": ""}
        remaining = max(0, int((self._session["lease_expires_at"] - time.time()) * 1000))
        return {"active": remaining > 0, "id": self._session["id"], "lease_remaining_ms": remaining, "profile_id": self._session["profile_id"]}

    def start_session(self, profile_id: str, now: float | None = None) -> dict[str, Any]:
        profile = self._read(profile_id)
        now = time.time() if now is None else now
        if self._session and self._session["lease_expires_at"] > now:
            raise MotionTrainingError("active training session exists")
        self._session = {"id": uuid.uuid4().hex, "profile_id": profile["id"], "lease_expires_at": now + SESSION_LEASE_SECONDS}
        return dict(self._session)

    def heartbeat(self, session_id: str, now: float | None = None) -> dict[str, Any]:
        session = self._require_session(session_id, now)
        session["lease_expires_at"] = (time.time() if now is None else now) + SESSION_LEASE_SECONDS
        return self._session_public()

    def _require_session(self, session_id: str, now: float | None = None) -> dict[str, Any]:
        now = time.time() if now is None else now
        if not self._session or self._session["id"] != session_id or self._session["lease_expires_at"] <= now:
            raise MotionTrainingError("training session lease expired")
        return self._session

    def append_sample(self, session_id: str, payload: dict[str, Any], now: float | None = None) -> dict[str, Any]:
        session = self._require_session(session_id, now)
        stats = validate_motion_sample(payload)
        profile = self._read(session["profile_id"])
        sample_record = {
            "schema": payload["schema"], "mode": payload["mode"], "completion": payload["completion"],
            "canvas": payload["canvas"], "start": payload["start"], "target": payload["target"],
            "radius": payload["radius"], "browser": payload.get("browser", {}), "points": payload["points"],
            "duration_ms": stats["duration_ms"], "path_efficiency": stats["path_efficiency"],
            "statistics": stats,
        }
        profile.setdefault("samples", []).append(sample_record)
        profile["updated_at_ms"] = int(time.time() * 1000)
        self._summary(profile)
        self._write(profile)
        return {"sample_count": profile["sample_count"], "reaction_count": profile["reaction_count"], "continuous_count": profile["continuous_count"], "profile_statistics": profile["statistics"]}

    def stop_session(self, session_id: str, now: float | None = None) -> dict[str, Any]:
        self._require_session(session_id, now)
        self._session = None
        return {"stopped": True}

    def train(self, profile_id: str) -> dict[str, Any]:
        profile = self._read(profile_id)
        self._summary(profile)
        modes = {s["mode"] for s in profile.get("samples", [])}
        if not {"reaction", "continuous"}.issubset(modes):
            raise MotionTrainingError("both modes are required")
        samples = profile["samples"]
        speeds = []
        for sample in samples:
            duration = max(float(sample["duration_ms"]), 1.0)
            distance = sum(math.hypot(float(p["dx"]), float(p["dy"])) for p in sample["points"])
            speeds.append(distance / duration)
        mean_speed = sum(speeds) / len(speeds)
        mean_eff = float(profile["statistics"]["path_efficiency"] or 0.0)
        quality = max(0, min(100, round(50 * min(1.0, len(samples) / 12.0) + 50 * max(0.0, min(1.0, mean_eff)))))
        # 确定性 32 节点：按样本平均速度归一化，始终可重复。
        base = max(mean_speed, 1e-6)
        knots = [round(max(0.0, min(1.0, (mean_speed / base) * (0.72 + 0.28 * i / 31))), 6) for i in range(32)]
        profile["model"] = {"schema": "ttbox.motion-model.v1", "version": 1, "knots": knots, "quality": quality, "ready": quality >= 60, "coverage": {"reaction": profile["reaction_count"], "continuous": profile["continuous_count"]}}
        self._write(profile)
        return self._public(profile)

    def activate(self, profile_id: str, **mix: Any) -> dict[str, Any]:
        profile = self._read(profile_id)
        model = profile.get("model") or {}
        if not model.get("ready"):
            raise MotionTrainingError("model is not ready")
        values = {"curve": float(mix.get("curve_blend", 1.0)), "speed": float(mix.get("speed_blend", 1.0)), "reaction": float(mix.get("reaction_blend", 0.7)), "max_reaction_delay_ms": int(mix.get("max_reaction_delay_ms", 250))}
        if not all(0 <= values[k] <= 1 for k in ("curve", "speed", "reaction")) or not 0 <= values["max_reaction_delay_ms"] <= 1000:
            raise MotionTrainingError("mix values are outside the accepted range")
        _atomic_json(self.root / "active.json", {"profile_id": profile_id, "mix": values})
        return {"active_profile_id": profile_id, "enabled": True, "mix": values}

    def deactivate(self) -> dict[str, Any]:
        try:
            (self.root / "active.json").unlink()
        except FileNotFoundError:
            pass
        return {"active_profile_id": "", "enabled": False, "mix": self._mix()}
