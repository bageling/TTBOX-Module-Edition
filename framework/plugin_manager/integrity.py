"""包完整性：SHA256 校验与 Ed25519 签名校验（复用 Release 管理端的签名风格）。"""
from __future__ import annotations

import base64
import hashlib
from pathlib import Path

from .models import IntegrityError


def sha256_file(path: str | Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_integrity(package: str | Path, expected_sha256: str | None) -> bool:
    """校验包 SHA256；未提供期望值时返回 True（跳过校验）。"""
    actual = sha256_file(package)
    if expected_sha256:
        normalized = expected_sha256.strip().lower()
        if actual != normalized:
            raise IntegrityError(
                f"SHA256 不匹配: 期望 {normalized}，实际 {actual}"
            )
    return True


def verify_signature(package: str | Path, signature: str | None, public_key_path: str | Path | None) -> bool:
    """Ed25519 签名校验：签名对象为包 SHA256 的 Base64 文本。

    与 release/manager/server.py 的 verify_package_signature 风格一致。
    缺少 cryptography 或未提供签名/公钥时返回 False，不阻断流程。
    """
    if not signature or not public_key_path:
        return False
    key_path = Path(public_key_path)
    if not key_path.is_file():
        return False
    try:
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
        from cryptography.exceptions import InvalidSignature

        key = serialization.load_pem_public_key(key_path.read_bytes())
        if not isinstance(key, Ed25519PublicKey):
            return False
        message = sha256_file(package).encode("ascii")
        raw_signature = base64.b64decode(signature)
        try:
            key.verify(raw_signature, message)
            return True
        except InvalidSignature:
            return False
    except ImportError:
        return False
    except Exception:
        return False