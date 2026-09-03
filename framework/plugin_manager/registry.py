"""Plugin Registry：已安装插件记录的注册表与 JSON 持久化。"""
from __future__ import annotations

import json
import tempfile
from pathlib import Path
from typing import Iterable

from .models import PluginNotFoundError, PluginRecord


class PluginRegistry:
    def __init__(self, path: str | Path | None = None):
        self.path: Path | None = Path(path) if path else None
        self._records: dict[str, PluginRecord] = {}

    def add(self, record: PluginRecord) -> None:
        self._records[record.plugin_id] = record

    def get(self, plugin_id: str) -> PluginRecord | None:
        return self._records.get(plugin_id)

    def require(self, plugin_id: str) -> PluginRecord:
        record = self._records.get(plugin_id)
        if record is None: raise PluginNotFoundError(f"插件未注册: {plugin_id}")
        return record

    def remove(self, plugin_id: str) -> bool:
        return self._records.pop(plugin_id, None) is not None

    def update(self, record: PluginRecord) -> None:
        self._records[record.plugin_id] = record

    def has(self, plugin_id: str) -> bool:
        return plugin_id in self._records

    def list(self) -> list[PluginRecord]:
        return list(self._records.values())

    def ids(self) -> list[str]:
        return list(self._records.keys())

    def to_dict(self) -> dict:
        return {"plugins": [record.to_dict() for record in self._records.values()]}

    def load(self) -> None:
        if self.path is None or not self.path.is_file(): return
        try:
            data = json.loads(self.path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return
        self._records = {}
        for item in data.get("plugins", []):
            record = PluginRecord.from_dict(item)
            self._records[record.plugin_id] = record

    def save(self) -> None:
        if self.path is None: return
        self.path.parent.mkdir(parents=True, exist_ok=True)
        payload = json.dumps(self.to_dict(), ensure_ascii=False, indent=2)
        fd, tmp = tempfile.mkstemp(dir=str(self.path.parent), suffix=".tmp")
        try:
            with open(fd, "w", encoding="utf-8") as handle:
                handle.write(payload)
            Path(tmp).replace(self.path)
        finally:
            if Path(tmp).exists(): Path(tmp).unlink()

    def set_state(self, plugin_id: str, state) -> None:
        record = self.require(plugin_id)
        record.state = state
        self.save()