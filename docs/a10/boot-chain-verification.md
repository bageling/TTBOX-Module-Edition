# Boot Chain 验证（boot-chain-verification.md）

## 1. 启动链构成（RK3588 / Ubuntu 24.04.1 / 6.1.0-1025-rockchip）

```
ROM (RK3588)
  → IDB loader（GPT 前 16.8MB 内，dd 原样复制 ✅）
  → U-Boot u-boot.itb @ 8MiB（BL31 内嵌，dd 原样复制 ✅）
  → extlinux（rootfs /boot/extlinux/extlinux.conf）
  → vmlinuz-6.1.0-1025-rockchip ✅
  → DTB rk3588-orangepi-5-plus.dtb ✅
  → DT overlay rk3588-hdmirx.dtbo ✅
  → initrd.img-6.1.0-1025-rockchip ✅
  → root=UUID=<根分区>  ← 这里断裂（IMG 内为旧 UUID）
  → /etc/fstab UUID=<根分区>  ← 这里断裂
  → systemd → DRM/显示/HID/Web
```

## 2. 各级验证状态（GOLDEN vs IMG）

| 环节 | 文件 | GOLDEN | IMG | 状态 |
|---|---|---|---|---|
| IDB loader | 前 16.8MB | ✓ | dd 原样 | ✅ 一致 |
| U-Boot+BL31 | u-boot.itb @8MiB | ✓ | dd 原样 | ✅ 一致 |
| 分区表 | GPT p1/p2 | boot,esp | p1 ✓ / p2 flag 丢失 | ⚠️ 已修复 |
| extlinux | extlinux.conf | root=c3855003 | root=c3855003（旧） | ❌ 致命 |
| Kernel | vmlinuz | ✓ | ✓ 同字节 | ✅ |
| initrd | initrd.img | ✓ | ✓ 同字节 | ✅ |
| DTB | rk3588-orangepi-5-plus.dtb | ✓ | ✓ | ✅ |
| DTBO | rk3588-hdmirx.dtbo | ✓ | ✓ (421B) | ✅ |
| firmware | /lib/firmware/... | ✓ | ✓ 完整 | ✅ |
| fstab | /etc/fstab | UUID=c3855003 | UUID=c3855003（旧） | ❌ 致命 |

## 3. 断裂点精确定位

- **断裂点 A**：`extlinux.conf append root=UUID=c3855003-b298-4ef0-bb40-149bed57fd8c`
  - 新 rootfs 实际 UUID = `b782e19b-2294-4d60-b4f2-b5e0e493029e`
  - 内核 initramfs 阶段 `Gave up waiting for root device` → 无法启动
- **断裂点 B**：`/etc/fstab` 根分区 UUID 同样为旧值
  - 即使 kernel 参数被绕过，systemd 也无法挂载根

## 4. 未受影响的部分（已确认原样）

- U-Boot / BL31：`dd if=/dev/mmcblk1 bs=1M count=21` 逐字节复制，未替换
- Kernel / DTB / DTBO / firmware：rsync 完整复制，字节一致
- HDMI RX：rk3588-hdmirx.dtbo 在镜像中存在
- 显示输出：rk3588-orangepi-5-plus.dtb（含 vop/drm 节点）在镜像中存在

## 5. 修复后启动链预期

```
extlinux root=UUID=b782e19b-...  ✅（构建脚本 sed 修正）
fstab    UUID=b782e19b-...       ✅
→ initramfs 挂载根成功
→ systemd 启动 → DRM 初始化 → 第一/第二屏输出
→ HDMI RX / HID / Web 服务
```
