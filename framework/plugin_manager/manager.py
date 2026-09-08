"""统一 Plugin Manager：兼容第2步运行时接口，并编排标准、注册表、来源与生命周期。"""
from __future__ import annotations
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable
from .models import *
from .registry import PluginRegistry
from .discovery import PluginDiscovery
from .standard import PluginManifest, PluginStore
from .repository import PluginRepository
from .lifecycle import BuiltinPluginRuntime, ProcessPluginRuntime, HealthChecker, PluginRuntime
from .transaction import TransactionLog
from .integrity import verify_integrity
from .install import resolve_package

@dataclass(frozen=True)
class PluginSpec:
    plugin_id: str
    start: Callable[[], None] | None = None
    stop: Callable[[], None] | None = None

@dataclass(frozen=True)
class PluginStatus:
    state: PluginState
    health: PluginHealth = PluginHealth.UNKNOWN
    version: str = ""
    error: str = ""

class PluginManager:
    def __init__(self, plugins_root=None, state_file=None, security=None, services=None, config=None, repository: PluginRepository | None = None, runtime_factory=None):
        self._items: dict[str, PluginSpec] = {}
        self._states: dict[str, PluginState] = {}
        self._runtimes: dict[str, PluginRuntime] = {}
        self.root = Path(plugins_root) if plugins_root else None
        self.store = PluginStore(self.root) if self.root else None
        self.registry = PluginRegistry(state_file or (self.root / ".registry.json" if self.root else None))
        self.registry.load()
        self.discovery = PluginDiscovery(self.root) if self.root else None
        self.security = security
        self.services = services
        self.config = config
        self.repository = repository
        self.runtime_factory = runtime_factory
        self.health_checker = HealthChecker()
        self.transactions = TransactionLog(self.root / ".transaction.json" if self.root else None)
        self.last_recovery = self.transactions.read() if self.transactions.recoverable() else None

    # Backward-compatible builtin lifecycle API.
    def register(self, spec: PluginSpec) -> bool:
        if spec.plugin_id in self._items or self.registry.has(spec.plugin_id): return False
        self._items[spec.plugin_id] = spec; self._states[spec.plugin_id] = PluginState.DISABLED; return True
    def query(self, plugin_id): return self._items.get(plugin_id)
    def status(self, plugin_id):
        record = self.registry.get(plugin_id)
        if record: return PluginStatus(record.state, record.health, record.version, record.error)
        return PluginStatus(self._states[plugin_id])
    def enable(self, plugin_id):
        if plugin_id in self._items: self._states[plugin_id] = PluginState.ENABLED; return True
        record = self.registry.require(plugin_id); record.enabled=True; record.state=PluginState.ENABLED; self.registry.save(); return True
    def disable(self, plugin_id):
        if plugin_id in self._items:
            if self._states.get(plugin_id) == PluginState.RUNNING: self.stop(plugin_id)
            self._states[plugin_id]=PluginState.DISABLED; return True
        record=self.registry.require(plugin_id)
        if record.state == PluginState.RUNNING: self.stop(plugin_id)
        record.enabled=False; record.state=PluginState.DISABLED; self.registry.save(); return True
    def start(self, plugin_id):
        if plugin_id in self._items and not self.registry.has(plugin_id):
            state = self._states.get(plugin_id, PluginState.ENABLED)
            if state == PluginState.DISABLED: return False
            try:
                if self._items[plugin_id].start: self._items[plugin_id].start()
                self._states[plugin_id]=PluginState.RUNNING; return True
            except Exception: self._states[plugin_id]=PluginState.FAILED; return False
        record=self.registry.require(plugin_id); self._check_dependencies(plugin_id, set())
        if not record.enabled: return False
        try:
            runtime=self._runtime(plugin_id, record); runtime.start(); record.state=PluginState.RUNNING; record.health=PluginHealth.HEALTHY; record.error=""; self.registry.save(); return True
        except Exception as exc: record.state=PluginState.FAILED; record.error=str(exc); record.health=PluginHealth.FAILED; self.registry.save(); return False
    def stop(self, plugin_id):
        if plugin_id in self._items and not self.registry.has(plugin_id):
            try:
                if self._items[plugin_id].stop: self._items[plugin_id].stop()
                self._states[plugin_id]=PluginState.ENABLED; return True
            except Exception: self._states[plugin_id]=PluginState.FAILED; return False
        record=self.registry.require(plugin_id)
        try:
            runtime=self._runtimes.get(plugin_id)
            if runtime: runtime.stop()
            record.state=PluginState.STOPPED; record.health=PluginHealth.UNKNOWN; self.registry.save(); return True
        except Exception as exc: record.state=PluginState.FAILED; record.error=str(exc); self.registry.save(); return False
    def restart(self, plugin_id): return self.stop(plugin_id) and self.start(plugin_id)

    def discover(self):
        if not self.discovery: return []
        scanned = self.discovery.scan()
        scanned_ids = {record.plugin_id for record in scanned}
        for plugin_id in list(self.registry.ids()):
            if plugin_id not in scanned_ids:
                self.registry.remove(plugin_id)
        for record in scanned: self.registry.add(record)
        self.registry.save(); return self.registry.list()
    def load(self): self.registry.load(); return self.registry.list()
    def save(self): self.registry.save()
    def get_installed_plugins(self): return self.registry.list()
    def get_plugin(self, plugin_id): return self.registry.get(plugin_id)
    def get_plugin_status(self, plugin_id): return self.status(plugin_id)
    def start_all(self):
        return all(self.start(r.plugin_id) for r in self.registry.list() if r.enabled and r.autostart)

    def install(self, request):
        package=resolve_package(request, self.repository, self.root / ".downloads" if self.root else None)
        verify_integrity(package, getattr(request, "expected_sha256", None))
        inspected=__import__("framework.plugin_manager.standard", fromlist=["PluginPackage"]).PluginPackage.inspect(package)
        if self.registry.has(inspected.manifest.plugin_id) and not getattr(request, "force", False): raise PluginAlreadyInstalledError(inspected.manifest.plugin_id)
        self._check_dependencies_for_manifest(inspected.manifest)
        self.transactions.begin("install", inspected.manifest.plugin_id, new_version=inspected.manifest.version)
        try:
            self.store.install(package)
            record=self._record(inspected.manifest)
            self.registry.add(record); self._declare_permissions(record); self.registry.save(); self.transactions.commit(); return record
        except Exception as exc:
            self.transactions.fail(exc); raise
    def uninstall(self, plugin_id):
        record=self.registry.require(plugin_id); self._require_permission(plugin_id, "manage_plugin")
        if self._dependents(plugin_id): raise PluginManagerError(f"插件仍被依赖: {', '.join(self._dependents(plugin_id))}")
        self.stop(plugin_id); self.transactions.begin("uninstall", plugin_id)
        self.store.uninstall(plugin_id); self.registry.remove(plugin_id); self.registry.save(); self.transactions.commit(); return True
    def upgrade(self, request):
        package=resolve_package(request, self.repository, self.root / ".downloads" if self.root else None)
        inspected=__import__("framework.plugin_manager.standard", fromlist=["PluginPackage"]).PluginPackage.inspect(package); old=self.registry.require(inspected.manifest.plugin_id); was_running=old.state==PluginState.RUNNING
        self._check_dependencies_for_manifest(inspected.manifest); self.transactions.begin("upgrade", old.plugin_id, old.version, inspected.manifest.version)
        try:
            if was_running: self.stop(old.plugin_id)
            self.store.upgrade(package); new=self._record(inspected.manifest, old); self.registry.update(new); self._declare_permissions(new); self.registry.save()
            if was_running and not self.start(new.plugin_id): raise UpgradeError("新版本启动或健康检查失败")
            self.transactions.commit(); return new
        except Exception as exc:
            self.store.rollback(old.plugin_id); restored=self._record(__import__("framework.plugin_manager.standard", fromlist=["PluginPackage"]).PluginPackage.inspect(Path(old.path) / "plugin.json").manifest, old); self.registry.update(restored); self.registry.save(); self.transactions.fail(exc)
            if was_running: self.start(old.plugin_id)
            raise
    def rollback(self, plugin_id):
        old=self.registry.require(plugin_id)
        if not self.store.rollback(plugin_id): return False
        import json
        manifest_data=json.loads((Path(old.path) / "plugin.json").read_text(encoding="utf-8"))
        from .standard import PluginManifest
        restored=self._record(PluginManifest.from_dict(manifest_data), old)
        self.registry.update(restored); self.registry.save(); return restored

    def search_plugins(self, query): return self.repository.search_plugins(query) if self.repository else []
    def get_market_plugin(self, plugin_id): return self.repository.get_plugin(plugin_id) if self.repository else None
    def get_market_versions(self, plugin_id): return self.repository.get_versions(plugin_id) if self.repository else []
    def verify_integrity(self, package, expected_sha256=None): return verify_integrity(package, expected_sha256)
    def verify_signature(self, package, signature=None, public_key_path=None):
        from .integrity import verify_signature
        return verify_signature(package, signature, public_key_path)

    def _record(self, manifest, old=None):
        now=time.strftime("%Y-%m-%dT%H:%M:%S")
        return PluginRecord(manifest.plugin_id, manifest.version, manifest.plugin_type, str(self.root / manifest.plugin_id), manifest.enabled if old is None else old.enabled, PluginState.ENABLED if manifest.enabled else PluginState.INSTALLED, manifest.autostart, PluginHealth.UNKNOWN, manifest.permissions, manifest.dependencies, manifest.api_version, manifest.entry, old.installed_at if old else now, now)
    def _runtime(self, plugin_id, record):
        if plugin_id in self._runtimes: return self._runtimes[plugin_id]
        import json
        from .standard import PluginManifest
        spec=self._items.get(plugin_id)
        manifest=PluginManifest.from_dict(json.loads((Path(record.path) / "plugin.json").read_text(encoding="utf-8")))
        runtime=self.runtime_factory(record, spec) if self.runtime_factory else (BuiltinPluginRuntime(spec.start if spec else None, spec.stop if spec else None) if record.plugin_type=="builtin" else ProcessPluginRuntime(Path(record.path) / manifest.entry, record.path))
        self._runtimes[plugin_id]=runtime; return runtime
    def _declare_permissions(self, record):
        if self.security: self.security.declare(record.plugin_id, set(record.permissions))
    def _require_permission(self, plugin_id, permission):
        if self.security and not self.security.check(plugin_id, permission): raise PermissionError(f"插件缺少权限: {permission}")
    def _check_dependencies_for_manifest(self, manifest):
        for dependency in manifest.dependencies:
            dep,_=parse_dependency(dependency)
            if not self.registry.has(dep) and dep not in self._items: raise DependencyMissingError(f"缺少依赖: {dep}")
    def _check_dependencies(self, plugin_id, visiting):
        if plugin_id in visiting: raise DependencyCycleError(plugin_id)
        visiting.add(plugin_id); record=self.registry.require(plugin_id)
        for dependency in record.dependencies:
            dep,_=parse_dependency(dependency)
            if not self.registry.has(dep): raise DependencyMissingError(dep)
            self._check_dependencies(dep, visiting.copy())
    def _dependents(self, plugin_id): return [r.plugin_id for r in self.registry.list() if any(parse_dependency(d)[0]==plugin_id for d in r.dependencies)]
