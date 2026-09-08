"""Model Plugin 管理层适配器；文件管理复用现有 Platform ModelManager 语义，推理仍由 Core ModelAdapter/RKNNEngine负责。"""
from __future__ import annotations
import hashlib, json, shutil, tempfile
from pathlib import Path
from .model_contract import ModelManifest, ModelManifestError, ModelInfo, ModelState

class ModelPluginService:
    def __init__(self, root, loader=None):
        self.root=Path(root).resolve(); self.staging=self.root/'staging'; self.installed=self.root/'installed'; self.active_dir=self.root/'active'; self.active_file=self.active_dir/'active.json'; self.loader=loader
        for path in (self.staging,self.installed,self.active_dir): path.mkdir(parents=True,exist_ok=True)
    def _id(self, model_id):
        if not isinstance(model_id,str) or not model_id or Path(model_id).name != model_id or any(c in model_id for c in '/\\') or model_id in ('.','..'): raise ModelManifestError("非法模型 id")
        return model_id
    def _manifest(self, model_id):
        data=json.loads((self.installed/model_id/'model.json').read_text(encoding='utf-8')); return ModelManifest.from_dict(data)
    def upload(self, model_id, source, manifest):
        mid=self._id(model_id); src=Path(source).resolve(); m=manifest if isinstance(manifest,ModelManifest) else ModelManifest.from_dict(manifest)
        if m.model_id != mid: raise ModelManifestError("模型 id 不一致")
        if not src.is_file(): raise FileNotFoundError(src)
        dest=self.staging/mid; shutil.rmtree(dest,ignore_errors=True); dest.mkdir(parents=True); shutil.copy2(src,dest/'model.rknn'); (dest/'model.json').write_text(json.dumps(m.to_dict(),ensure_ascii=False,indent=2),encoding='utf-8')
        return ModelInfo(mid,m.version,ModelState.STAGING,str(dest),m.metadata)
    def validate(self, model_id):
        mid=self._id(model_id); d=self.staging/mid; f=d/'model.rknn'; m=ModelManifest.from_dict(json.loads((d/'model.json').read_text(encoding='utf-8')))
        if not f.is_file() or f.stat().st_size < 1: raise ValueError("模型文件为空或不存在")
        if self.loader and not self.loader(f,m): raise ValueError("RKNN 模型加载校验失败")
        metadata=dict(m.metadata); metadata['sha256']=self._digest(f); (d/'validation.json').write_text(json.dumps({'ok':True,'metadata':metadata},ensure_ascii=False),encoding='utf-8')
        return ModelInfo(mid,m.version,ModelState.VALIDATED,str(d),metadata)
    def install(self, model_id):
        mid=self._id(model_id); src=self.staging/mid; marker=src/'validation.json'; f=src/'model.rknn'
        if not marker.is_file() or not f.is_file(): raise ValueError("模型必须先验证")
        dest=self.installed/mid; temp=Path(tempfile.mkdtemp(prefix=f'.{mid}.',dir=self.installed))
        try: shutil.copytree(src,temp/'payload',dirs_exist_ok=True); shutil.move(str(temp/'payload'),str(dest))
        finally: shutil.rmtree(temp,ignore_errors=True)
        return ModelInfo(mid,self._manifest_from_dir(dest).version,ModelState.INSTALLED,str(dest),self._manifest_from_dir(dest).metadata)
    def activate(self, model_id):
        mid=self._id(model_id); d=self.installed/mid; f=d/'model.rknn'; m=self._manifest_from_dir(d)
        if not f.is_file(): raise ValueError("模型未安装")
        if self.loader and not self.loader(f,m): raise ValueError("模型加载失败，保持旧 active")
        old=self.active().model_id if self.active() else None; payload={'model_id':mid,'version':m.version}
        tmp=self.active_file.with_suffix('.tmp'); tmp.write_text(json.dumps(payload,ensure_ascii=False,indent=2),encoding='utf-8'); tmp.replace(self.active_file)
        if old and old != mid: (self.active_dir/'previous.json').write_text(json.dumps({'model_id':old},ensure_ascii=False),encoding='utf-8')
        return ModelInfo(mid,m.version,ModelState.ACTIVE,str(d),m.metadata)
    def rollback(self):
        previous=self.active_dir/'previous.json'
        if not previous.is_file(): raise ValueError("没有可回滚模型")
        mid=json.loads(previous.read_text(encoding='utf-8'))['model_id']; current=self.active().model_id; result=self.activate(mid); (self.active_dir/'previous.json').write_text(json.dumps({'model_id':current}),encoding='utf-8'); return result
    def active(self):
        if not self.active_file.is_file(): return None
        data=json.loads(self.active_file.read_text(encoding='utf-8')); mid=self._id(data['model_id']); m=self._manifest_from_dir(self.installed/mid); return ModelInfo(mid,m.version,ModelState.ACTIVE,str(self.installed/mid),m.metadata)
    def list(self):
        active=self.active(); result=[]
        for d in sorted(self.installed.iterdir()):
            if d.is_dir() and (d/'model.rknn').is_file():
                m=self._manifest_from_dir(d); result.append(ModelInfo(d.name, m.version, ModelState.ACTIVE if active and active.model_id==d.name else ModelState.INSTALLED, str(d),m.metadata))
        return result
    def delete(self, model_id):
        mid=self._id(model_id)
        if self.active() and self.active().model_id==mid: raise ValueError("禁止删除 active 模型")
        dest=self.installed/mid
        if not dest.is_dir(): raise ValueError("模型未安装")
        shutil.rmtree(dest); return True
    def _manifest_from_dir(self,d): return ModelManifest.from_dict(json.loads((d/'model.json').read_text(encoding='utf-8')))
    @staticmethod
    def _digest(path):
        h=hashlib.sha256();
        with open(path,'rb') as f:
            for block in iter(lambda:f.read(1024*1024),b''): h.update(block)
        return h.hexdigest()
