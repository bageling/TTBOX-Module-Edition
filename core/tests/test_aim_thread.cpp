// test_aim_thread.cpp — AimThread 生命周期与最新任务消费测试。
#include <cassert>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "aim/AimThread.hpp"
using namespace ttbox::core::aim;
int main() {
    AimTargetMailbox mailbox(1);
    auto output = std::make_shared<ttbox::core::output::NullHidOutput>();
    AimThread thread;
    assert(thread.start(&mailbox, output, 1000));
    AimTargetTask task; task.frame_number = 7; task.timestamp_us = 123;
    assert(mailbox.offer(0, task));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    [[maybe_unused]] auto status = thread.status();
    thread.stop();
    assert(status.has_task && status.last_frame == 7);
    assert(status.has_target == false);
    assert(status.move_x == 0 && status.move_y == 0);
    std::cout << "test_aim_thread: PASS\n";
}
