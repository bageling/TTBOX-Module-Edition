# AIBox IMG 回归诊断报告（img-regression-report.md）

> 阶段：A10 镜像回归定位（暂停 A10 后续开发）
> 日期：2026-08-14
> 现象：烧录 AIBox-v0.0.1-orangepi5plus.img 后无法显示第二块屏幕

---

## ROOT CAUSE（最终结论，非猜测）

**镜像制作过程中，mkfs 创建的新 rootfs 分区生成了全新 UUID，但镜像内的
`/boot/extlinux/extlinux.conf` 和 `/etc/fstab` 仍引用 GOLDEN 系统的旧 root UUID
（`c3855003-b298-4ef0-bb40-149bed57fd8c`），与新 rootfs 实际 UUID
（`b782e19b-2294-4d60-b4f2-b5e0e493029e`）不匹配。**

内核启动时按 `root=UUID=c3855003...` 查找根分区 → 找不到 → initramfs 阶段根挂载失败
→ 系统完全无法启动 → DRM/显示/桌面/Web 全部不初始化 → 任何屏幕（含第二屏）均无输出。

**"第二屏失效"的真实原因不是显示/HDMI 配置被破坏，而是系统根本没有启动。**

---

## 一、哪个文件/分区/制作步骤导致失效

| 项 | 结论 |
|---|---|
| 导致失效的文件 | `/boot/extlinux/extlinux.conf`（`append root=UUID=`）<br>`/etc/fstab`（根分区 UUID） |
| 导致失效的分区 | 分区 2（rootfs）：mkfs.ext4 后 UUID 改变，未同步到启动配置 |
| 导致失效的制作步骤 | `a10_build_rootfs.sh` 的"mkfs.ext4 → rsync"之间，缺少"读取新 UUID → 更新 extlinux.conf/fstab"步骤 |

## 二、逐项排查结论（排除项）

| 被排查项 | 结论 |
|---|---|
| U-Boot | ✅ 未破坏（dd 前 21MB 逐字节复制，u-boot.itb @8MiB 原样） |
| BL31 | ✅ 未破坏（内嵌于 u-boot.itb，原样） |
| Kernel | ✅ 未破坏（vmlinuz-6.1.0-1025-rockchip 53.7MB 字节一致） |
| DTB | ✅ 未破坏（rk3588-orangepi-5-plus.dtb 存在） |
| DT overlay | ✅ 未破坏（rk3588-hdmirx.dtbo 421B 存在） |
| HDMI RX | ✅ 文件层面完整（依赖 dtbo + 驱动，均无缺失） |
| DRM | ✅ 文件层面完整（vop/drm 节点在主 DTB） |
| Display 配置 | ✅ 文件层面无差异（/boot 与 firmware 完整） |
| 文件系统配置 | ❌ **/boot/extlinux/extlinux.conf + /etc/fstab UUID 错误（根因）** |

## 三、GOLDEN vs IMG 证据

- GOLDEN root UUID（当前 GOLDEN 系统实际值，blkid）：`c3855003-b298-4ef0-bb40-149bed57fd8c`
- IMG rootfs UUID（从桌面 img 解包 rootfs 分区，读 ext4 superblock）：`b782e19b-2294-4d60-b4f2-b5e0e493029e`
- IMG 内 extlinux.conf / fstab：仍为 `c3855003-...`（7-Zip 解包逐字读取）
- 详见 `golden-vs-img-diff.txt`、`boot-chain-verification.md`

## 四、修复方案（用户第 8 条：GOLDEN Boot + AIBox RootFS）

不替换任何 Rockchip 固件（U-Boot/BL31/Kernel/DTB/DTBO/Firmware 原样），仅修正 RootFS 配置：

1. 重新构建镜像（`a10_build_v2.sh`）
2. rsync 完成后读取新 rootfs UUID（`blkid -s UUID -o value ${LOOP}p2`）
3. `sed` 更新镜像内：
   - `/boot/extlinux/extlinux.conf`：`root=UUID=<新UUID>`
   - `/etc/fstab`：`UUID=<新UUID>`
4. 恢复 p2 分区属性（boot,esp，与原 GOLDEN 表一致）
5. 其余保持原流程（清理/账号/首启）

## 五、重新烧录验证清单（修复后）

- [ ] 第一屏（HDMI 输出 0）PASS
- [ ] 第二屏（HDMI 输出 1）PASS
- [ ] HDMI RX PASS
- [ ] 1080p240 PASS
- [ ] RGA PASS
- [ ] RKNPU PASS
- [ ] RKNN PASS
- [ ] 黄瓦 320 INT8 ≈240FPS
- [ ] 冷启动（全新 TF 卡）PASS

全部恢复后方可继续 A10。
