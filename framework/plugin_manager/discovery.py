"""Plugin 自动发现：扫描插件根目录下的 plugin.json。"""
from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Iterable

from .models import PluginHealth, PluginRecord, PluginState
from .standard import PluginManifest, PluginManifestError


class PluginDiscovery:
    def __init__(self, root: str | Path):
        self.root = Path(root)

    def scan(self) -> list[PluginRecord]:
        if not self.root.is_dir(): return []
        records: list[PluginRecord] = []
        for directory in sorted(self.root.iterdir()):
            if not directory.is_dir() or directory.name.startswith("."):
                continue
            # 跳过缓存/仓库等非插件目录（__pycache__ 为 Python 缓存，repository 为包存储）
            if directory.name.startswith("__") or directory.name == "repository":
                continue
            manifest_path = directory / "plugin.json"
            if not manifest_path.is_file():
                records.append(self._invalid(directory.name, str(directory), "缺少 plugin.json"))
                continue
            try:
                raw = json.loads(manifest_path.read_text(encoding="utf-8"))
                manifest = PluginManifest.from_dict(raw)
            except (OSError, json.JSONDecodeError, PluginManifestError) as exc:
                records.append(self._invalid(directory.name, str(directory), str(exc)))
                continue
            if manifest.plugin_id != directory.name:
                records.append(self._invalid(directory.name, str(directory), "目录名与 manifest id 不一致"))
                continue
            records.append(self._from_manifest(manifest, str(directory)))
        return records

    def _from_manifest(self, manifest: PluginManifest, path: str) -> PluginRecord:
        return PluginRecord(
            plugin_id=manifest.plugin_id,
            version=manifest.version,
            plugin_type=manifest.plugin_type,
            path=path,
            enabled=manifest.enabled,
            state=PluginState.INSTALLED,
            autostart=manifest.autostart,
            health=PluginHealth.UNKNOWN,
            permissions=manifest.permissions,
            dependencies=manifest.dependencies,
            api_version=manifest.api_version,
            entry=manifest.entry,
            installed_at=time.strftime("%Y-%m-%dT%H:%M:%S"),
        )

    def _invalid(self, plugin_id: str, path: str, error: str) -> PluginRecord:
        return PluginRecord(
            plugin_id=plugin_id,
            version="",
            plugin_type="invalid",
            path=path,
            enabled=False,
            state=PluginState.INVALID,
            error=error,
        )