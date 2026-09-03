"""Framework 服务生命周期管理。"""
from __future__ import annotations
from dataclasses import dataclass
from enum import Enum
from typing import Callable
class ServiceState(str, Enum): STOPPED="stopped"; RUNNING="running"; FAILED="failed"
@dataclass(frozen=True)
class ServiceStatus:
    state: ServiceState
@dataclass
class ServiceSpec:
    name: str
    start: Callable[[], None] | None = None
    stop: Callable[[], None] | None = None
class ServiceManager:
    def __init__(self, specs: dict[str, ServiceSpec] | None = None): self._specs = specs or {}; self._states = {k: ServiceState.STOPPED for k in self._specs}
    def status(self, name: str) -> ServiceStatus: return ServiceStatus(self._states[name])
    def start(self, name: str) -> bool:
        try:
            if self._specs[name].start: self._specs[name].start()
            self._states[name] = ServiceState.RUNNING; return True
        except Exception: self._states[name] = ServiceState.FAILED; return False
    def stop(self, name: str) -> bool:
        try:
            if self._specs[name].stop: self._specs[name].stop()
            self._states[name] = ServiceState.STOPPED; return True
        except Exception: self._states[name] = ServiceState.FAILED; return False
    def restart(self, name: str) -> bool: return self.stop(name) and self.start(name)
    def enable(self, name: str) -> bool: return name in self._specs
    def disable(self, name: str) -> bool: return name in self._specs
