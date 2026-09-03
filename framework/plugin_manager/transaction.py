"""Plugin Manager 操作事务日志。"""
from __future__ import annotations
import json
import time
from pathlib import Path
from enum import Enum

class TransactionState(str, Enum): IDLE="idle"; RUNNING="running"; COMMITTED="committed"; FAILED="failed"
class TransactionLog:
    def __init__(self, path: str | Path | None): self.path=Path(path) if path else None
    def begin(self, operation, plugin_id, old_version="", new_version=""):
        self._write({"operation":operation,"plugin_id":plugin_id,"old_version":old_version,"new_version":new_version,"state":TransactionState.RUNNING.value,"started_at":time.time()})
    def commit(self): self._write({"state":TransactionState.COMMITTED.value,"committed_at":time.time()})
    def fail(self, error): self._write({"state":TransactionState.FAILED.value,"error":str(error),"failed_at":time.time()})
    def recoverable(self):
        data=self.read(); return bool(data and data.get("state") == TransactionState.RUNNING.value)
    def read(self):
        if not self.path or not self.path.is_file(): return None
        try: return json.loads(self.path.read_text(encoding="utf-8"))
        except (OSError,json.JSONDecodeError): return None
    def clear(self):
        if self.path and self.path.exists(): self.path.unlink()
    def _write(self, data):
        if self.path:
            self.path.parent.mkdir(parents=True, exist_ok=True); self.path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
