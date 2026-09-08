"""TTBOX EDID Validator — 验证 EDID 二进制是否符合规范。"""

from .builder import _pnp_decode, _checksum


def validate_edid(edid: bytes) -> list:
    errors = []
    if len(edid) != 256:
        errors.append(f"长度: 期望 256 字节, 实际 {len(edid)}")
        return errors
    if edid[0:8] != bytes([0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00]):
        errors.append(f"Header 错误: {edid[0:8].hex()}")
    if sum(edid[:128]) % 256 != 0:
        errors.append(f"Base checksum 错误")
    if sum(edid[128:]) % 256 != 0:
        errors.append(f"Extension checksum 错误")
    if edid[18] < 1:
        errors.append(f"EDID 版本号异常: {edid[18]}.{edid[19]}")
    if not (edid[20] & 0x80):
        errors.append("不是数字信号输入")
    vendor = _pnp_decode(edid[8:10])
    if not vendor.isalpha() or len(vendor) != 3:
        errors.append(f"厂商代码异常: {vendor}")
    if edid[126] == 0:
        errors.append("无扩展块 (CTA-861 缺失)")
    if edid[126] > 0 and edid[128] != 0x02:
        errors.append(f"扩展块类型不是 CTA-861: 0x{edid[128]:02x}")
    pc = edid[54] | edid[55] << 8
    if pc > 60000:
        errors.append(f"DTD1 像素时钟异常: {pc*10/1000:.2f} MHz")
    return errors


def verify_edid(edid: bytes):
    errors = validate_edid(edid)
    return len(errors) == 0, errors
