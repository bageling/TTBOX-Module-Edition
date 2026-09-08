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
    // 重启残留场景：旧周期帧号大 → 新周期从 0 重新计数，take_latest 会被旧任务卡死；
    // clear() 后新任务立即可取（修复 1~3 分钟检测框不更新）。
    AimTargetTask stale; stale.frame_number = 100000; stale.worker_id = 2;
    assert(mailbox.offer(2, stale));
    AimTargetTask fresh; fresh.frame_number = 1; fresh.worker_id = 0;
    assert(mailbox.offer(0, fresh));
    AimTargetTask got;
    assert(mailbox.take_latest(&got, 0));
    assert(got.frame_number == 100000);  // 修复前：残留任务把 last_frame 抬高
    mailbox.clear();
    assert(mailbox.take_latest(&got, 0));
    assert(got.frame_number == 1);       // 修复后：新周期任务立即可取
    assert(!mailbox.take_latest(&got, 1));
    std::cout << "test_aim_target_mailbox: PASS\n";
    return 0;
}
