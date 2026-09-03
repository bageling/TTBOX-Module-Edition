"""Plugin Manager 数据模型：状态、记录、安装请求、市场目录与异常。"""
from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Any


class PluginState(str, Enum):
    INSTALLED = "installed"
    DISABLED = "disabled"
    ENABLED = "enabled"
    RUNNING = "running"
    STOPPED = "stopped"
    FAILED = "failed"
    UNINSTALLED = "uninstalled"
    INVALID = "invalid"


class PluginHealth(str, Enum):
    UNKNOWN = "unknown"
    HEALTHY = "healthy"
    DEGRADED = "degraded"
    FAILED = "failed"


class InstallSource(str, Enum):
    LOCAL_FILE = "local_file"
    URL = "url"
    REPOSITORY = "repository"


@dataclass
class PluginRecord:
    plugin_id: str
    version: str
    plugin_type: str
    path: str
    enabled: bool = False
    state: PluginState = PluginState.INSTALLED
    autostart: bool = False
    health: PluginHealth = PluginHealth.UNKNOWN
    permissions: tuple[str, ...] = ()
    dependencies: tuple[str, ...] = ()
    api_version: str = "1"
    entry: str = ""
    installed_at: str = ""
    updated_at: str = ""
    error: str = ""

    def to_dict(self) -> dict[str, Any]:
        return {
            "plugin_id": self.plugin_id,
            "version": self.version,
            "plugin_type": self.plugin_type,
            "path": self.path,
            "enabled": self.enabled,
            "state": self.state.value,
            "autostart": self.autostart,
            "health": self.health.value,
            "permissions": list(self.permissions),
            "dependencies": list(self.dependencies),
            "api_version": self.api_version,
            "entry": self.entry,
            "installed_at": self.installed_at,
            "updated_at": self.updated_at,
            "error": self.error,
        }

    @classmethod
    def from_dict(cls, value: dict[str, Any]) -> "PluginRecord":
        return cls(
            plugin_id=value["plugin_id"],
            version=value.get("version", ""),
            plugin_type=value.get("plugin_type", "process"),
            path=value.get("path", ""),
            enabled=bool(value.get("enabled", False)),
            state=PluginState(value.get("state", "installed")),
            autostart=bool(value.get("autostart", False)),
            health=PluginHealth(value.get("health", "unknown")),
            permissions=tuple(value.get("permissions", ())),
            dependencies=tuple(value.get("dependencies", ())),
            api_version=value.get("api_version", "1"),
            entry=value.get("entry", ""),
            installed_at=value.get("installed_at", ""),
            updated_at=value.get("updated_at", ""),
            error=value.get("error", ""),
        )


@dataclass
class InstallRequest:
    source: InstallSource
    path: str | None = None
    url: str | None = None
    plugin_id: str | None = None
    version: str | None = None
    expected_sha256: str | None = None
    signature: str | None = None
    force: bool = False


@dataclass
class CatalogEntry:
    plugin_id: str
    name: str = ""
    description: str = ""
    author: str = ""
    publisher: str = ""
    version: str = ""
    versions: tuple[str, ...] = ()
    api_version: str = "1"
    plugin_type: str = "process"
    icon: str = ""
    homepage: str = ""
    repository: str = ""
    download_url: str = ""
    package_size: int = 0
    sha256: str = ""
    permissions: tuple[str, ...] = ()
    dependencies: tuple[str, ...] = ()
    compatible_core: str = ""
    compatible_framework: str = ""
    release_time: str = ""
    changelog: str = ""

    def to_dict(self) -> dict[str, Any]:
        return {
            "plugin_id": self.plugin_id,
            "name": self.name,
            "description": self.description,
            "author": self.author,
            "publisher": self.publisher,
            "version": self.version,
            "versions": list(self.versions),
            "api_version": self.api_version,
            "type": self.plugin_type,
            "icon": self.icon,
            "homepage": self.homepage,
            "repository": self.repository,
            "download_url": self.download_url,
            "package_size": self.package_size,
            "sha256": self.sha256,
            "permissions": list(self.permissions),
            "dependencies": list(self.dependencies),
            "compatible_core": self.compatible_core,
            "compatible_framework": self.compatible_framework,
            "release_time": self.release_time,
            "changelog": self.changelog,
        }

    @classmethod
    def from_dict(cls, value: dict[str, Any]) -> "CatalogEntry":
        return cls(
            plugin_id=value["plugin_id"],
            name=value.get("name", ""),
            description=value.get("description", ""),
            author=value.get("author", ""),
            publisher=value.get("publisher", ""),
            version=value.get("version", ""),
            versions=tuple(value.get("versions", ())),
            api_version=value.get("api_version", "1"),
            plugin_type=value.get("type", "process"),
            icon=value.get("icon", ""),
            homepage=value.get("homepage", ""),
            repository=value.get("repository", ""),
            download_url=value.get("download_url", ""),
            package_size=value.get("package_size", 0),
            sha256=value.get("sha256", ""),
            permissions=tuple(value.get("permissions", ())),
            dependencies=tuple(value.get("dependencies", ())),
            compatible_core=value.get("compatible_core", ""),
            compatible_framework=value.get("compatible_framework", ""),
            release_time=value.get("release_time", ""),
            changelog=value.get("changelog", ""),
        )


class PluginManagerError(Exception):
    """Plugin Manager 通用错误。"""


class PluginNotFoundError(PluginManagerError):
    """插件不存在。"""


class PluginAlreadyInstalledError(PluginManagerError):
    """插件已安装。"""


class DependencyMissingError(PluginManagerError):
    """依赖缺失。"""


class DependencyCycleError(PluginManagerError):
    """依赖循环。"""


class InstallError(PluginManagerError):
    """安装失败。"""


class UpgradeError(PluginManagerError):
    """升级失败。"""


class IntegrityError(PluginManagerError):
    """完整性校验失败。"""


def parse_dependency(dependency: str) -> tuple[str, str | None]:
    """把依赖字符串解析为 (plugin_id, version_constraint)。支持 'id' 或 'id>=1.0.0'。"""
    dependency = dependency.strip()
    for operator in (">=", "<=", "==", ">", "<"):
        if operator in dependency:
            plugin_id, _, version = dependency.partition(operator)
            return plugin_id.strip(), operator + version.strip()
    return dependency, None
