from __future__ import annotations
from enum import Enum
from .config import ConfigService
from .plugin_manager import PluginManager
from .service import ServiceManager
from .security import SecurityPolicy
class FrameworkState(str, Enum): STOPPED="stopped"; RUNNING="running"; FAILED="failed"
class FrameworkRuntime:
    def __init__(self, config, services, plugins, security, core_service="core"):
        self.config=config; self.services=services; self.plugins=plugins; self.security=security; self.core_service=core_service; self.state=FrameworkState.STOPPED
    def start(self):
        if self.state == FrameworkState.RUNNING: return True
        if not self.services.start(self.core_service): self.state=FrameworkState.FAILED; return False
        self.state=FrameworkState.RUNNING; return True
    def stop(self):
        if self.state == FrameworkState.STOPPED: return True
        for plugin_id in tuple(self.plugins._items):
            if self.plugins.status(plugin_id).state.value == "running": self.plugins.stop(plugin_id)
        ok=self.services.stop(self.core_service); self.state=FrameworkState.STOPPED if ok else FrameworkState.FAILED; return ok
