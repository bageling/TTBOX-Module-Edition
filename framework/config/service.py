"""Framework 配置服务：JSON 读写、校验与变更通知。"""
from __future__ import annotations
import json
from pathlib import Path
from typing import Any, Callable

class ConfigService:
    def __init__(self, validators: dict[str, Callable[[Any], bool]] | None = None):
        self._data: dict[str, Any] = {}
        self._path: Path | None = None
        self._validators = validators or {}
        self._listeners: list[Callable[[str, Any], None]] = []
    def load(self, path: str | Path) -> bool:
        p = Path(path); data = json.loads(p.read_text(encoding="utf-8"))
        if not isinstance(data, dict): raise ValueError("config root must be an object")
        self._data, self._path = data, p; return self.validate()
    def get(self, key: str, default: Any = None) -> Any:
        value: Any = self._data
        for part in key.split("."):
            if not isinstance(value, dict) or part not in value: return default
            value = value[part]
        return value
    def set(self, key: str, value: Any) -> bool:
        validator = self._validators.get(key)
        if validator and not validator(value): return False
        parts = key.split("."); target = self._data
        for part in parts[:-1]: target = target.setdefault(part, {})
        target[parts[-1]] = value
        for listener in tuple(self._listeners): listener(key, value)
        return True
    def validate(self) -> bool:
        return all(fn(self.get(key)) for key, fn in self._validators.items())
    def save(self, path: str | Path | None = None) -> bool:
        p = Path(path) if path else self._path
        if p is None: raise ValueError("config path is not set")
        p.parent.mkdir(parents=True, exist_ok=True); p.write_text(json.dumps(self._data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"); self._path = p; return True
    def subscribe(self, listener: Callable[[str, Any], None]) -> None: self._listeners.append(listener)
