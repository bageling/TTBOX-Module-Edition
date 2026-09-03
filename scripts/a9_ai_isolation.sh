#!/bin/bash
# a9_ai_isolation.sh — HID 高回报率负载 与 AI Pipeline 并发隔离测试
# 对比：无 HID / HID 8000Hz 绑 CPU0（NPU IRQ 所在核，最坏情况）/ CPU7 / 默认
cd /home/ubuntu/ttbox2/ttbox/core/build
MODEL=/home/ubuntu/ttbox2/models/huangwa.rknn

run_ai() {
  ./test_worker_hw --model $MODEL --adapter --workers 3 --cores 4,5,6 \
    --buffers 8 --frames 1000 --inw 320 --inh 320 2>&1 | \
    grep -E 'capture FPS|总吞吐|错误|poll_timeout' | tr '\n' ' | '
  echo ""
}

echo "===== [基线] 无 HID 负载 ====="
run_ai

echo "===== [HID 8000Hz @ CPU0=NPU IRQ] 与 AI 并发 ====="
./test_hid_load_sim --rate 8000 --cpu 0 --duration 9 >/tmp/hid_cpu0.log 2>&1 &
HID_PID=$!
sleep 1
run_ai
wait $HID_PID
grep '目标 rate' /tmp/hid_cpu0.log

echo "===== [HID 8000Hz @ CPU7] 与 AI 并发 ====="
./test_hid_load_sim --rate 8000 --cpu 7 --duration 9 >/tmp/hid_cpu7.log 2>&1 &
HID_PID=$!
sleep 1
run_ai
wait $HID_PID
grep '目标 rate' /tmp/hid_cpu7.log

echo "===== [HID 8000Hz @ 默认调度] 与 AI 并发 ====="
./test_hid_load_sim --rate 8000 --duration 9 >/tmp/hid_def.log 2>&1 &
HID_PID=$!
sleep 1
run_ai
wait $HID_PID
grep '目标 rate' /tmp/hid_def.log

echo ""
echo "===== NPU IRQ 分布（AI+HID 并发后）====="
grep -iE 'fdab9000.iommu|fdab0000.npu' /proc/interrupts
