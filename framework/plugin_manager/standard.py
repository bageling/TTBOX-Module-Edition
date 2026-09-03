"""TTBOX Plugin 标准：manifest 校验与 .tpk 只读检查。"""
from __future__ import annotations

import json
import re
import shutil
import tarfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any

SUPPORTED_API_VERSION = "1"
_ALLOWED_TYPES = {"process", "builtin"}
_ALLOWED_PERMISSIONS = {
    "read_config", "write_config", "control_service", "manage_plugin",
    "sensitive", "read_core_status", "manage_model", "network", "filesystem", "hardware",
}
_SEMVER = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-[0-9A-Za-z.-]+)?(?:\+[0-9A-Za-z.-]+)?$")
_ID = re.compile(r"^[a-z0-9][a-z0-9._-]{0,63}$")


class PluginManifestError(ValueError):
    """manifest 或插件包不符合标准。"""


@dataclass(frozen=True)
class PluginManifest:
    plugin_id: str
    name: str
    version: str
    description: str
    api_version: str
    entry: str
    plugin_type: str
    autostart: bool
    enabled: bool
    dependencies: tuple[str, ...] = ()
    permissions: tuple[str, ...] = ()
    stop_timeout: int = 10
    health_check: dict[str, Any] | None = None
    config: str = "config"
    data: str = "data"
    compatible_core: str | None = None
    compatible_framework: str | None = None

    @classmethod
    def from_dict(cls, value: dict[str, Any]) -> "PluginManifest":
        if not isinstance(value, dict):
            raise PluginManifestError("manifest 必须是对象")
        required = ("id", "name", "version", "api_version", "entry", "type")
        missing = [key for key in required if key not in value]
        if missing: raise PluginManifestError(f"缺少必填字段: {', '.join(missing)}")
        plugin_id = value["id"]
        if not isinstance(plugin_id, str) or not _ID.fullmatch(plugin_id):
            raise PluginManifestError("id 必须是小写字母、数字、点、下划线或短横线组成")
        for key in ("name", "version", "api_version", "entry"):
            if not isinstance(value[key], str) or not value[key].strip():
                raise PluginManifestError(f"{key} 必须是非空字符串")
        if not _SEMVER.fullmatch(value["version"]): raise PluginManifestError("version 必须是语义版本")
        if value["api_version"] != SUPPORTED_API_VERSION: raise PluginManifestError("不支持的 api_version")
        plugin_type = value["type"]
        if plugin_type not in _ALLOWED_TYPES: raise PluginManifestError("type 只能是 process 或 builtin")
        entry = PurePosixPath(value["entry"])
        if entry.is_absolute() or ".." in entry.parts or not value["entry"].startswith("bin/"):
            raise PluginManifestError("entry 必须是 bin/ 下的相对路径")
        for key in ("autostart", "enabled"):
            if key in value and not isinstance(value[key], bool): raise PluginManifestError(f"{key} 必须是布尔值")
        stop_timeout = value.get("stop_timeout", 10)
        if not isinstance(stop_timeout, int) or isinstance(stop_timeout, bool) or not 0 < stop_timeout <= 3600:
            raise PluginManifestError("stop_timeout 必须是 1 到 3600 的整数")
        dependencies = _string_tuple(value.get("dependencies", ()), "dependencies")
        permissions = _string_tuple(value.get("permissions", ()), "permissions")
        unknown = set(permissions) - _ALLOWED_PERMISSIONS
        if unknown: raise PluginManifestError(f"未知权限: {', '.join(sorted(unknown))}")
        health = value.get("health_check")
        if health is not None and not isinstance(health, dict): raise PluginManifestError("health_check 必须是对象")
        return cls(plugin_id, value["name"], value["version"], value.get("description", ""), value["api_version"], value["entry"], plugin_type, value.get("autostart", False), value.get("enabled", False), dependencies, permissions, stop_timeout, health, value.get("config", "config"), value.get("data", "data"), value.get("compatible_core"), value.get("compatible_framework"))


def _string_tuple(value: Any, key: str) -> tuple[str, ...]:
    if not isinstance(value, (list, tuple)) or any(not isinstance(item, str) or not item for item in value):
        raise PluginManifestError(f"{key} 必须是字符串数组")
    return tuple(value)


@dataclass(frozen=True)
class InspectedPackage:
    manifest: PluginManifest
    root_name: str


class PluginPackage:
    @staticmethod
    def inspect(path: str | Path) -> InspectedPackage:
        package = Path(path)
        if package.suffix != ".tpk": raise PluginManifestError("插件包扩展名必须是 .tpk")
        try: archive = tarfile.open(package, "r:*")
        except (OSError, tarfile.TarError) as exc: raise PluginManifestError(f"无法读取插件包: {exc}") from exc
        with archive:
            members = archive.getmembers()
            if not members: raise PluginManifestError("插件包为空")
            names = [PurePosixPath(member.name) for member in members]
            if any(p.is_absolute() or ".." in p.parts for p in names): raise PluginManifestError("插件包包含路径穿越")
            roots = {p.parts[0] for p in names if p.parts}
            if len(roots) != 1: raise PluginManifestError("插件包必须只有一个根目录")
            root = next(iter(roots))
            manifest_name = f"{root}/plugin.json"
            manifest_member = archive.getmember(manifest_name) if manifest_name in {m.name for m in members} else None
            if manifest_member is None or not manifest_member.isfile(): raise PluginManifestError("缺少根目录下的 plugin.json")
            stream = archive.extractfile(manifest_member)
            try: raw = json.load(stream) if stream else None
            except (json.JSONDecodeError, UnicodeDecodeError) as exc: raise PluginManifestError("plugin.json 不是有效 UTF-8 JSON") from exc
            manifest = PluginManifest.from_dict(raw)
            if manifest.plugin_id != root: raise PluginManifestError("manifest id 必须与包根目录一致")
            return InspectedPackage(manifest, root)


class PluginStore:
    """本地 .tpk 安装仓库；只操作 plugins 根目录下的插件。"""
    def __init__(self, root: str | Path):
        self.root = Path(root)
        self.backup_root = self.root / ".backups"

    def inspect(self, plugin_id: str) -> InspectedPackage:
        directory = self._plugin_dir(plugin_id)
        manifest_path = directory / "plugin.json"
        if not manifest_path.is_file(): raise PluginManifestError("插件未安装或缺少 plugin.json")
        try: raw = json.loads(manifest_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc: raise PluginManifestError("已安装插件 manifest 无效") from exc
        manifest = PluginManifest.from_dict(raw)
        if manifest.plugin_id != plugin_id: raise PluginManifestError("插件目录与 manifest id 不一致")
        return InspectedPackage(manifest, plugin_id)

    def install(self, package: str | Path) -> PluginManifest:
        inspected = PluginPackage.inspect(package)
        destination = self._plugin_dir(inspected.manifest.plugin_id)
        if destination.exists(): raise PluginManifestError("插件已安装，请使用升级流程")
        self._extract(package, destination, inspected.root_name)
        return inspected.manifest

    def upgrade(self, package: str | Path) -> PluginManifest:
        inspected = PluginPackage.inspect(package)
        destination = self._plugin_dir(inspected.manifest.plugin_id)
        if not destination.exists(): raise PluginManifestError("升级目标插件不存在")
        self.backup_root.mkdir(parents=True, exist_ok=True)
        backup = self.backup_root / inspected.manifest.plugin_id
        if backup.exists(): shutil.rmtree(backup)
        shutil.copytree(destination, backup)
        staging = self.root / ".staging" / inspected.manifest.plugin_id
        if staging.exists(): shutil.rmtree(staging)
        try:
            self._extract(package, staging, inspected.root_name)
            if destination.exists(): shutil.rmtree(destination)
            staging.rename(destination)
            return inspected.manifest
        except Exception:
            if destination.exists(): shutil.rmtree(destination)
            shutil.copytree(backup, destination)
            raise

    def rollback(self, plugin_id: str) -> bool:
        backup = self.backup_root / plugin_id
        destination = self._plugin_dir(plugin_id)
        if not backup.is_dir(): return False
        if destination.exists(): shutil.rmtree(destination)
        shutil.copytree(backup, destination)
        return True

    def uninstall(self, plugin_id: str) -> bool:
        destination = self._plugin_dir(plugin_id)
        if not destination.exists(): return False
        shutil.rmtree(destination)
        return True

    def _plugin_dir(self, plugin_id: str) -> Path:
        if not _ID.fullmatch(plugin_id): raise PluginManifestError("非法插件 id")
        return self.root / plugin_id

    def _extract(self, package: str | Path, destination: Path, root_name: str) -> None:
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists(): shutil.rmtree(destination)
        with tarfile.open(package, "r:*") as archive:
            base = destination.resolve()
            for member in archive.getmembers():
                relative = PurePosixPath(member.name).relative_to(root_name)
                target = (destination / Path(*relative.parts)).resolve()
                if target != base and base not in target.parents: raise PluginManifestError("插件包目标路径越界")
                archive.extract(member, destination.parent, filter="data")
        extracted = destination.parent / root_name
        if extracted != destination:
            if destination.exists(): shutil.rmtree(destination)
            extracted.rename(destination)
        self.inspect(destination.name)
