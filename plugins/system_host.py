"""System/Hardware Plugin 的 Framework 适配面。

这里仅组合已有系统适配器；不复制 PluginManager、ConfigService 或 ServiceManager。
"""
from __future__ import annotations
from dataclasses import dataclass
from pathlib import Path
from .system_common import FanService, WifiService, NetworkService, MonitorService, SystemService, LogService, UpgradeService

@dataclass
class SystemPluginHost:
    fan: FanService
    wifi: WifiService
    network: NetworkService
    monitor: MonitorService
    system: SystemService
    log: LogService
    upgrade: UpgradeService

    @classmethod
    def create(cls, log_path="/var/log/ttbox/ttbox.log", sys_root="/sys", proc_root="/proc"):
        return cls(FanService(sys_root), WifiService(), NetworkService(), MonitorService(proc_root, sys_root), SystemService(), LogService(log_path), UpgradeService())

    def service(self, plugin_id):
        try: return getattr(self, plugin_id)
        except AttributeError as exc: raise KeyError(plugin_id) from exc

    def status(self):
        return {plugin_id: self.service(plugin_id).status() for plugin_id in ("fan", "wifi", "network", "monitor", "system", "log", "upgrade")}
