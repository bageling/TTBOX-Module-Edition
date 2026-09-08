"""TTBOX EDID Builder — 根据配置生成完整 256 字节 EDID 二进制。

输出：Base Block (128B) + CTA-861 Extension (128B)。
"""
import struct
from .timing_db import DisplayTiming, TIMING_MAP, lookup_timing, mode_info

EDID_SIZE = 256
BLOCK_SIZE = 128


def _pnp_encode(vendor: str) -> bytes:
    """3 字母厂商码 → 2 字节 PnP ID。"""
    assert len(vendor) == 3 and vendor.isascii() and vendor.isalpha()
    v = vendor.upper()
    c1, c2, c3 = ord(v[0]) - 64, ord(v[1]) - 64, ord(v[2]) - 64
    val = (c1 << 10) | (c2 << 5) | c3
    return struct.pack('>H', val)


def _pnp_decode(data: bytes) -> str:
    val = struct.unpack('>H', data)[0]
    return chr(((val >> 10) & 0x1F) + 64) + chr(((val >> 5) & 0x1F) + 64) + chr((val & 0x1F) + 64)


def _checksum(data: bytes) -> int:
    """使 block 所有字节之和 mod 256 = 0。"""
    return (-sum(data)) & 0xFF


def _pack_dtd(t: DisplayTiming) -> bytes:
    """DisplayTiming → 18 字节 DTD。"""
    pc_10khz = t.pixel_clock_10khz
    # DTD 16bit 像素时钟上限 655.35MHz（HDMI 1.4 规范）；超过则拒绝生成而非静默溢出
    if pc_10khz > 0xFFFF:
        raise ValueError(
            f"pixel clock {t.pixel_clock} MHz 超出 DTD 16bit 上限 655.35MHz，"
            f"该模式无法写入 EDID DTD（HDMI 规范限制）")
    h_active = t.h_active & 0xFFF
    h_blank = t.h_blank & 0xFFF
    v_active = t.v_active & 0xFFF
    v_blank = t.v_blank & 0xFFF
    h_sync_offset = t.h_front_porch & 0x3FF
    h_sync_pulse = t.h_sync & 0x3FF
    v_sync_offset = t.v_front_porch & 0x3F
    v_sync_pulse = t.v_sync & 0x3F
    buf = bytearray(18)
    struct.pack_into('<H', buf, 0, pc_10khz)
    # 标准 EDID DTD 布局：byte2=ha_lo, byte3=hb_lo, byte4=ha_hi(7-4)|hb_hi(3-0)
    buf[2] = h_active & 0xFF
    buf[3] = h_blank & 0xFF
    buf[4] = (((h_active >> 8) & 0xF) << 4) | ((h_blank >> 8) & 0xF)
    # byte5=va_lo, byte6=vb_lo, byte7=va_hi(7-4)|vb_hi(3-0)
    buf[5] = v_active & 0xFF
    buf[6] = v_blank & 0xFF
    buf[7] = (((v_active >> 8) & 0xF) << 4) | ((v_blank >> 8) & 0xF)
    buf[8] = h_sync_offset & 0xFF
    buf[9] = h_sync_pulse & 0xFF
    buf[10] = ((v_sync_offset & 0xF) << 4) | (v_sync_pulse & 0xF)
    buf[11] = (((h_sync_offset >> 8) & 0x3) << 6) | (((h_sync_pulse >> 8) & 0x3) << 4) | (((v_sync_offset >> 4) & 0x3) << 2) | ((v_sync_pulse >> 4) & 0x3)
    buf[12] = 0xA6  # h_image_size lo（YU: 16:9 2560x1440 → 698x392mm 分两字节编码）
    buf[13] = 0x7E
    buf[14] = 0x21  # v_image_size lo
    buf[15] = 0x00
    buf[16] = 0
    # byte17 数字同步标志：bit4=数字信号，bit3=分离同步，bit1=Vsync正，bit0=Hsync正
    # 由时序参数 h_pol/v_pol 真实计算，不再写死（此前 0x1A 导致 1440p144 V极性与源不符反复断流）
    flags = 0x18  # 数字 + 分离同步
    if t.v_pol:
        flags |= 0x02
    if t.h_pol:
        flags |= 0x01
    buf[17] = flags | (0x80 if t.interlaced else 0)
    return bytes(buf)


def _monitor_name_dtd(name: str) -> bytes:
    """显示器名称 DTD (0xFC)，对齐 YU：名称 + \n + 2 空格填满 13 字节。"""
    buf = bytearray(18)
    buf[3] = 0xFC
    text = (name[:10].ljust(10) + '\n  ').encode('ascii', 'replace')
    buf[5:18] = text[:13]  # 13 字节内容填满（含尾部），byte17 不再覆盖
    return bytes(buf)


def _serial_dtd(serial_text: str) -> bytes:
    """序列号描述符 DTD (0xFF)，对齐 YU：11 字符 + \n + 空格 = 13 字节。"""
    buf = bytearray(18)
    buf[3] = 0xFF
    text = (serial_text[:11] + '\n ').encode('ascii', 'replace')
    buf[5:18] = text[:13]
    return bytes(buf)


def _range_limits_dtd_yu(min_v: int = 143, max_v: int = 146) -> bytes:
    """频率范围 DTD (0xFD)，逐字节对齐 YU 1440p144 成功版：
    00 00 00 fd 00 8f 92 d6 d9 3b 00 ...（byte10=0x00）
    minV=0x8f(143) maxV=0x92(146) minH=0xd6(214) maxH=0xd9(217)
    maxPC=0x3b(59)*10MHz=590MHz
    """
    buf = bytearray(18)
    buf[3] = 0xFD
    buf[4] = 0x00
    buf[5] = min_v & 0xFF
    buf[6] = max_v & 0xFF
    buf[7] = 214     # min horizontal kHz
    buf[8] = 217     # max horizontal kHz
    buf[9] = 59      # max pixel clock / 10MHz (590MHz)
    buf[10] = 0x0A   # 扩展标志（对齐 YU = 0x0A）
    return bytes(buf)


def _range_limits_dtd(max_clock_mhz: int) -> bytes:
    """兼容旧签名：默认走 YU 格式。"""
    return _range_limits_dtd_yu()


def yu_established() -> bytes:
    """Established timings 字节（25-34，共 10 字节），逐字节对齐 YU 成功版。"""
    return bytes.fromhex("cf74a3574cb02309484c")


def _hdmi_vsdb(max_tmds_mhz: int = 600) -> bytes:
    """HDMI Vendor Specific Data Block（CTA-861 扩展块内），字节格式对齐 YU 实测。
    根因修复 1：此前 EDID 无 HDMI VSDB → PC 显卡视为 DVI 设备 → 2K 被拒。
    根因修复 2：注册 ID 必须按 LSB-first 写（YU 实测 03 0c 00 = IEEE 0x000C03），
    此前写反序 00 0c 03 → PC 无法识别为 HDMI 设备。
    根因修复 3：586MHz > HDMI 1.4 上限 340MHz，必须置 HDMI_2_0 位(bit5) 声明
    SCDC 支持，否则 PC 按 HDMI 1.4 带宽拒绝 1440p144。
    布局（对齐 YU 实测 67 03 0c 00 10 00 00）：
      byte0 = tag3 | len7 = 0x67
      byte1-3 = 注册 ID LSB-first: 03 0C 00
      byte4-5 = 物理地址 A.B.C.D = 1.0.0.0 → 0x10 0x00
      byte6 = byte7 的 bit5(HDMI_2_0) 声明 → 0x00
      byte7 = 0x20 | max_tmds 位... YU: byte7 高位为版本/能力位
    YU 1440p144 实测: 67 03 0c 00 10 00 00 76 67 d8 5d c4 01 76 80
      即 HDMI1.4 VSDB(8B) + HF-VSDB(7B): 76 67 d8 5d c4 01 76 80
    """
    # 逐字节对齐 YU 成功实测布局（1440p144 协商成功版）：
    # VSDB1 (8B): 67 03 0c 00 10 00 00 76
    #   tag=3 len=7 | 注册ID 03 0C 00 (IEEE 0x000C03 LSB) | phy 10 00 | 00 | byte7=0x76
    #   byte7=0x76 → bit6/5/2 = HDMI_2_0 声明（586MHz 需要）
    # VSDB2 (8B): 67 d8 5d c4 01 76 80 00
    #   tag=3 len=7 | HF-VSDB 数据 d8 5d c4(=0xC45DD8 LSB 前缀) | 01 | 76(=592→600MHz?) | 80 | 00
    vsdb14 = bytes([0x67, 0x03, 0x0C, 0x00, 0x10, 0x00, 0x00, 0x76])
    hf = bytes([0x67, 0xD8, 0x5D, 0xC4, 0x01, 0x76, 0x80, 0x00])
    return vsdb14 + hf


def _build_cta_extension(dtds, native_index=0) -> bytes:
    """CTA-861 Extension Block (128B)，布局对齐 YU 成功版：
    - 数据块 = HDMI1.4 VSDB(8B) + HF-VSDB(8B)，dtd_start=20
    - 扩展块 DTD 区全空（时序只在基础块 DTD1 —— YU 实测如此，
      非标准模式混入扩展块反而让 PC 拒绝）
    """
    ext = bytearray(128)
    ext[0] = 0x02       # CTA-861
    ext[1] = 0x03       # revision
    vsdb = _hdmi_vsdb()
    ext[4:4 + len(vsdb)] = vsdb
    # dtd_start = 20（YU 同款：数据块后无 VIC 块、DTD 区留空）
    dtd_start = 4 + len(vsdb)
    ext[2] = dtd_start & 0xFF
    ext[3] = (dtd_start >> 8) & 0xFF
    # DTD 区留 0（对齐 YU；DTD0=0 表示"无额外 DTD"合法）
    ext[127] = _checksum(bytes(ext[:127]))
    return bytes(ext)


class EdidBuilder:
    """根据配置生成 256B EDID。config 字段：
    vendor(3字母), product_id(0xXXXX), serial(0xXXXXXXXX), name(≤13字符),
    native_mode(timing token), native_only(bool), profile(可选)。
    """

    def __init__(self, config: dict):
        self.cfg = config
        self.vendor = str(config.get('vendor', 'OPI')).strip().upper()[:3] or 'OPI'
        self.product_id = int(str(config.get('product_id', '0x3588')), 16) & 0xFFFF
        self.serial = int(str(config.get('serial', '0x20260414')), 16) & 0xFFFFFFFF
        self.name = str(config.get('name', 'TTBOX'))[:13]
        self.native = str(config.get('native_mode', '1080p60'))
        self.native_only = bool(config.get('native_only', False))
        self.profile = str(config.get('profile', 'boot-safe-full'))

    def build(self) -> bytes:
        bb = bytearray(128)
        bb[0:8] = bytes([0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00])
        bb[8:10] = _pnp_encode(self.vendor)
        struct.pack_into('<H', bb, 10, self.product_id)
        struct.pack_into('<I', bb, 12, self.serial)
        bb[16], bb[17] = 0x01, 0x22  # 制造周=1/年=2022（对齐 YU）
        bb[18], bb[19] = 1, 4        # EDID 1.4（HF-VSDB/HDMI2.0 规范要求 1.4，
                                     # 1.3 下 PC 忽略 HF-VSDB → 586MHz 被拒）
        bb[20] = 0xA2                # 数字输入: bit7=1, DVI bit3=1, bit5=1（对齐 YU）
        bb[21], bb[22] = 0, 0        # 屏幕尺寸: 0=投影/不确定（对齐 YU）
        bb[23] = 0x78                # Gamma 2.2
        # 对齐 YU 精确布局：byte24=feature(0x0A)，byte25-34=established timings
        bb[24] = 0x0A
        bb[25:35] = yu_established()  # CF 74 a3 57 4c b0 23 09 48 4c
        bb[35] = 0x00
        # standard timings (38-53)：全 0x01（对齐 YU：不广播标准时序兜底模式，
        # 防止 PC 从中挑 800x600 之类低分辨率 fallback）
        for idx in range(38, 54):
            bb[idx] = 0x01
        # 解析首选时序（内置/compat/动态 token）
        info = mode_info(self.native)
        if info is None:
            info = mode_info('1080p60')
        native_timing = lookup_timing(info[0]) if info[0] in TIMING_MAP else None
        if native_timing is None:
            # 动态模式：用 CVT-RB 数据临时构造
            from .timing_db import reduced_blanking_pixel_clock_khz
            w, h, r = info[1], info[2], info[3]
            pc = reduced_blanking_pixel_clock_khz(w, h, r) / 1000.0
            native_timing = DisplayTiming(w, h, float(r), pc,
                                          48, 32, 80, 3, 5, 77)
        dtds = [native_timing]
        # added_modes（配置里显式追加）
        for tok in (self.cfg.get('added_modes') or []):
            try:
                t = lookup_timing(tok)
                if t != native_timing and t not in dtds:
                    dtds.append(t)
            except KeyError:
                pass
        if not self.native_only and len(dtds) < 3:
            # 附加常用模式（去重）
            for tok in ('1080p60compat', '1440p60', '2160p60'):
                try:
                    t = lookup_timing(tok)
                    if t != native_timing and t not in dtds:
                        dtds.append(t)
                except KeyError:
                    pass
        bb[54:72] = _pack_dtd(dtds[0])                       # DTD1 首选（YU 同布局）
        # 描述符顺序对齐 YU：DTD2=名称、DTD3=序列号、DTD4=Range Limits
        bb[72:90] = _monitor_name_dtd(self.name)
        # 序列号文本格式对齐 YU：vendor + serial hex 大写（ZWXDF6D7185）
        serial_text = f"{self.vendor}{format(self.serial, '08X')}"[:13]
        bb[90:108] = _serial_dtd(serial_text)
        bb[108:126] = _range_limits_dtd(int(max((t.pixel_clock for t in dtds), default=600)))
        bb[126] = 1                                          # 1 个扩展块
        bb[127] = _checksum(bytes(bb[:127]))
        ext = _build_cta_extension(dtds, native_index=0)
        result = bytes(bb) + ext
        assert len(result) == EDID_SIZE
        return result


def build_from_config(config: dict) -> bytes:
    return EdidBuilder(config).build()


def edid_to_hex_text(edid: bytes) -> str:
    lines = []
    for i in range(0, len(edid), 16):
        lines.append(' '.join(f'{b:02x}' for b in edid[i:i+16]))
    return '\n'.join(lines)
