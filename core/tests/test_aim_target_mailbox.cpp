// test_aim_target_mailbox.cpp — AimTargetMailbox 基础行为测试
#include <cassert>
#include <iostream>
#include "pipeline/AimTargetMailbox.hpp"
using namespace ttbox::core::aim;
int main() {
    AimTargetMailbox mailbox(3);
    AimTargetTask old_task; old_task.frame_number = 10; old_task.worker_id = 0;
    AimTargetTask new_task; new_task.frame_number = 12; new_task.worker_id = 1;
    assert(mailbox.offer(0, old_task));
    assert(mailbox.offer(1, new_task));
    AimTargetTask out;
    assert(mailbox.take_latest(&out, 0));
    assert(out.frame_number == 12);
    assert(out.worker_id == 1);
    assert(!mailbox.take_latest(&out, 12));
    assert(!mailbox.offer(3, old_task));
    std::cout << "test_aim_target_mailbox: PASS\n";
    return 0;
}
