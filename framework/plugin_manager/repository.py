"""插件来源抽象与本地仓库。"""
from __future__ import annotations
import json
from abc import ABC, abstractmethod
from pathlib import Path
from .models import CatalogEntry
from .standard import PluginPackage

class PluginRepository(ABC):
    @abstractmethod
    def list_plugins(self) -> list[CatalogEntry]: ...
    @abstractmethod
    def get_plugin(self, plugin_id: str) -> CatalogEntry | None: ...
    @abstractmethod
    def get_versions(self, plugin_id: str) -> list[str]: ...
    @abstractmethod
    def get_manifest(self, plugin_id: str, version: str): ...
    @abstractmethod
    def download_package(self, plugin_id: str, version: str) -> Path: ...
    def search_plugins(self, query: str) -> list[CatalogEntry]:
        query = query.lower()
        return [x for x in self.list_plugins() if query in (x.plugin_id + " " + x.name + " " + x.description).lower()]

class LocalRepository(PluginRepository):
    def __init__(self, root: str | Path): self.root = Path(root)
    def _packages(self): return sorted(self.root.glob("*.tpk")) if self.root.is_dir() else []
    def list_plugins(self):
        result = []
        for package in self._packages():
            try:
                m = PluginPackage.inspect(package).manifest
                result.append(CatalogEntry(plugin_id=m.plugin_id, name=m.name, description=m.description, version=m.version, versions=(m.version,), api_version=m.api_version, plugin_type=m.plugin_type, permissions=m.permissions, dependencies=m.dependencies, compatible_core=m.compatible_core or "", compatible_framework=m.compatible_framework or "", download_url=str(package), package_size=package.stat().st_size))
            except Exception: continue
        return result
    def get_plugin(self, plugin_id):
        items = [x for x in self.list_plugins() if x.plugin_id == plugin_id]
        return items[0] if items else None
    def get_versions(self, plugin_id): return [x.version for x in self.list_plugins() if x.plugin_id == plugin_id]
    def get_manifest(self, plugin_id, version):
        for package in self._packages():
            try:
                inspected = PluginPackage.inspect(package)
                if inspected.manifest.plugin_id == plugin_id and inspected.manifest.version == version: return inspected.manifest
            except Exception: pass
        return None
    def download_package(self, plugin_id, version):
        for package in self._packages():
            try:
                m = PluginPackage.inspect(package).manifest
                if m.plugin_id == plugin_id and m.version == version: return package
            except Exception: pass
        raise FileNotFoundError(f"本地仓库没有插件: {plugin_id}@{version}")

class HttpRepository(PluginRepository):
    def __init__(self, base_url: str): self.base_url = base_url.rstrip("/")
    def list_plugins(self): raise NotImplementedError
    def get_plugin(self, plugin_id): raise NotImplementedError
    def get_versions(self, plugin_id): raise NotImplementedError
    def get_manifest(self, plugin_id, version): raise NotImplementedError
    def download_package(self, plugin_id, version): raise NotImplementedError
