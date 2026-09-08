"""Model Plugin 对外契约：只描述模型资产，不暴露 RKNN 内部对象。"""
from __future__ import annotations
from dataclasses import dataclass, field
from typing import Any
import re

class ModelManifestError(ValueError): pass
_ID = re.compile(r"^[a-z0-9][a-z0-9._-]{0,63}$")

@dataclass(frozen=True)
class ModelManifest:
    model_id: str
    name: str
    version: str = "1.0.0"
    model_format: str = "rknn"
    framework: str = ""
    description: str = ""
    metadata: dict[str, Any] = field(default_factory=dict)
    repository_id: str = ""
    package_id: str = ""
    author: str = ""
    homepage: str = ""
    icon: str = ""
    screenshots: tuple[str, ...] = ()
    channel: str = "stable"
    license: str = ""
    download_url: str = ""
    checksum: str = ""
    signature: str = ""

    @property
    def input_width(self): return self.metadata.get("input_width")

    @property
    def input_height(self): return self.metadata.get("input_height")

    @classmethod
    def from_dict(cls, value: dict[str, Any]) -> "ModelManifest":
        if not isinstance(value, dict): raise ModelManifestError("model.json 必须是对象")
        for key in ("id", "name"):
            if not isinstance(value.get(key), str) or not value[key].strip(): raise ModelManifestError(f"缺少必填字段: {key}")
        if not _ID.fullmatch(value["id"]): raise ModelManifestError("非法模型 id")
        if value.get("format", "rknn") != "rknn": raise ModelManifestError("当前只支持 rknn 模型")
        version=value.get("version", "1.0.0")
        if not isinstance(version, str) or not re.fullmatch(r"\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?", version): raise ModelManifestError("非法模型版本")
        metadata={k:v for k,v in value.items() if k in {"input_width","input_height","input_format","quantization","classes","class_names"}}
        return cls(value["id"],value["name"],version,"rknn",value.get("framework", ""),value.get("description", ""),metadata,value.get("repository_id", ""),value.get("package_id", ""),value.get("author", ""),value.get("homepage", ""),value.get("icon", ""),tuple(value.get("screenshots", ())),value.get("channel", "stable"),value.get("license", ""),value.get("download_url", ""),value.get("checksum", ""),value.get("signature", ""))

    def to_dict(self):
        result={"id":self.model_id,"name":self.name,"version":self.version,"format":self.model_format,"framework":self.framework,"description":self.description}
        result.update(self.metadata)
        for key in ("repository_id","package_id","author","homepage","icon","license","download_url","checksum","signature","channel"):
            if getattr(self,key): result[key]=getattr(self,key)
        if self.screenshots: result["screenshots"]=list(self.screenshots)
        return result

@dataclass(frozen=True)
class ModelInfo:
    model_id: str
    version: str
    state: str
    path: str
    metadata: dict[str, Any] = field(default_factory=dict)

class ModelState:
    STAGING="staging"; VALIDATED="validated"; INSTALLED="installed"; ACTIVE="active"; FAILED="failed"
