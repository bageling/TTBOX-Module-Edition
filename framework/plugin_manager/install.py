"""统一安装来源解析。"""
from __future__ import annotations
import shutil
import urllib.request
from pathlib import Path
from .models import InstallSource


def resolve_package(request, repository=None, download_dir: str | Path | None = None) -> Path:
    source = request.source.value if isinstance(request.source, InstallSource) else str(request.source)
    if source == InstallSource.LOCAL_FILE.value:
        if not request.path: raise ValueError("local_file 需要 path")
        path = Path(request.path)
        if not path.is_file(): raise FileNotFoundError(path)
        return path
    if source == InstallSource.REPOSITORY.value:
        if repository is None or not request.plugin_id or not request.version: raise ValueError("repository 需要 repository、plugin_id、version")
        return Path(repository.download_package(request.plugin_id, request.version))
    if source == InstallSource.URL.value:
        if not request.url: raise ValueError("url 需要 url")
        target_dir = Path(download_dir or Path.cwd() / ".downloads")
        target_dir.mkdir(parents=True, exist_ok=True)
        target = target_dir / Path(request.url.split("?", 1)[0]).name
        if request.url.startswith("file://"):
            return Path(request.url[7:])
        urllib.request.urlretrieve(request.url, target)
        return target
    raise ValueError(f"未知安装来源: {source}")
