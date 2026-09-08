#!/bin/bash
# a9_stress_check.sh — 压力测试收尾检查：温度 / 降频 / gadget / fd / 线程
echo "===== 温度（thermal zones）====="
for z in /sys/class/thermal/thermal_zone*/; do
  t=$(cat $z/temp 2>/dev/null)
  type=$(cat $z/type 2>/dev/null)
  [ -n "$t" ] && echo "  $type: $((t/1000)).$((t%1000))°C"
done

echo "===== 频率（降频检查）====="
for p in 0 4 6; do
  g=$(cat /sys/devices/system/cpu/cpufreq/policy$p/scaling_governor 2>/dev/null)
  f=$(cat /sys/devices/system/cpu/cpufreq/policy$p/scaling_cur_freq 2>/dev/null)
  echo "  policy$p: governor=$g cur=$((f/1000))MHz"
done
echo "  NPU: $(cat /sys/class/devfreq/fdab0000.npu/cur_freq 2>/dev/null) Hz"

echo "===== HID Gadget 状态 ====="
cat /sys/class/udc/fc000000.usb/state 2>/dev/null
ls -la /dev/hidg* 2>/dev/null

echo "===== 进程/线程（残留检查）====="
ps -eLf 2>/dev/null | grep -E 'hid|worker|test_hid' | grep -v grep | head

echo "===== 系统负载 ====="
uptime
