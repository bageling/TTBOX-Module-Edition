// test_hotkey_config.cpp — 热键完全可配置化验收。
//
// 核心原则：热键是用户配置项（RuntimeProfile.mouse.aim_hotkey / aim_hotkey2 /
// aim_hotkey_mode / enabled），不是程序常量。AimThread 主门与 AiboxHidOutput
// 保险门都必须实时读取配置快照，改配置即时生效，无需重启。
//
// 场景：
//   A  默认热键（右键 0x02）：按右键出移动，按左键不出
//   B  改为另一按键（左键 0x01）：同一物理按键语义随配置翻转
//   C  改为侧键（0x08）：侧键触发，右键失效
//   D  双热键 any：主或副任一按下即触发
//   E  双热键 all：必须同时按下才触发
//   F  运行中改配置（RuntimeConfig::update）：不重启，下一周期生效
//   G  未按配置热键时绝不输出鼠标移动（含 enabled=false 总开关）
//
// 说明：AiboxHidOutput 保险门在 Windows stub 下不写盘，但其实时配置判定
// 与 AimThread 主门共用同一 RuntimeConfig 快照，本测试在 AimThread send
// 边界观察最终输出行为。
#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "aim/AimThread.hpp"
#include "model/RuntimeProfile.hpp"
#include "output/IHidOutput.hpp"

using namespace ttbox::core::aim;

namespace {

struct Action {
    int16_t move_x = 0;
    int16_t move_y = 0;
    uint64_t frame = 0;
};

class RecordingHidOutput final : public ttbox::core::output::IHidOutput {
public:
    bool send(const ttbox::core::output::OutputAction& a) override {
        std::lock_guard<std::mutex> lk(mu_);
        actions_.push_back({a.move_x, a.move_y, a.frame_number});
        return true;
    }
    std::vector<Action> snapshot() {
        std::lock_guard<std::mutex> lk(mu_);
        return actions_;
    }
private:
    std::mutex mu_;
    std::vector<Action> actions_;
};

// 目标框：画面中心偏右上；Gate 放行时应产生正向 X 与负向 Y 移动。
ttbox::core::DetectionBox make_box() {
    ttbox::core::DetectionBox b;
    b.x1 = 560.0f; b.y1 = 180.0f; b.x2 = 640.0f; b.y2 = 300.0f;
    b.score = 0.9f;
    b.class_id = 0;
    return b;
}

struct TestCtx {
    AimTargetMailbox mailbox{1};
    std::shared_ptr<RecordingHidOutput> output = std::make_shared<RecordingHidOutput>();
    std::shared_ptr<ttbox::core::RuntimeProfile> profile = std::make_shared<ttbox::core::RuntimeProfile>();
    ttbox::core::RuntimeConfig config;
    std::atomic<uint16_t> buttons{0};
    AimThread thread;

    TestCtx() {
        // 与产品默认一致的基线（右键 0x02 / any / enabled=true 由各场景覆盖）。
        profile->mouse.enabled = true;
        profile->mouse.aim_hotkey = 0x02;
        profile->mouse.aim_hotkey2 = 0x00;
        profile->mouse.aim_hotkey_mode = 0;
        profile->mouse.kp_x = 1.0f;
        profile->mouse.kp_y = 1.0f;
        profile->mouse.lost_grace_ms = 78.0f;
        profile->mouse.aim_point.offset_x = 0.5f;
        profile->mouse.aim_point.offset_y = 0.5f;
        config.update(profile);
    }

    bool start() { return thread.start(&mailbox, output, 2000, &config, &buttons); }

    void feed(uint64_t frame, uint64_t ts_us, bool has_target) {
        AimTargetTask t;
        t.frame_number = frame;
        t.timestamp_us = ts_us;
        t.frame_width = 1280;
        t.frame_height = 720;
        t.has_target = has_target;
        if (has_target) {
            t.target = make_box();
            t.aim_point = {600.0f, 240.0f};
            t.detections.push_back(make_box());
        }
        mailbox.offer(0, t);
    }

    // 运行中改配置：模拟用户改热键 → RuntimeConfig::update（AimThread 不重启）。
    void reconfigure(uint8_t hk, uint8_t hk2, int mode, bool enabled) {
        auto p = std::make_shared<ttbox::core::RuntimeProfile>(*profile);
        p->mouse.aim_hotkey = hk;
        p->mouse.aim_hotkey2 = hk2;
        p->mouse.aim_hotkey_mode = mode;
        p->mouse.enabled = enabled;
        profile = p;
        config.update(p);
    }
};

int wait_frames(TestCtx& ctx, int ms = 30) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return static_cast<int>(ctx.output->snapshot().size());
}

bool any_move(const std::vector<Action>& acts) {
    for (auto& a : acts) if (a.move_x != 0 || a.move_y != 0) return true;
    return false;
}

bool all_zero(const std::vector<Action>& acts, size_t from = 0) {
    for (size_t i = from; i < acts.size(); ++i)
        if (acts[i].move_x != 0 || acts[i].move_y != 0) return false;
    return true;
}

}  // namespace

int main() {
    int fails = 0;
    auto check = [&fails](bool ok, const char* name) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
        if (!ok) ++fails;
    };

    // ---- A. 默认热键（右键 0x02，即配置默认值）----
    {
        TestCtx ctx;  // 默认: hk=0x02
        if (!ctx.start()) { std::printf("[FAIL] A start\n"); return 1; }
        ctx.buttons.store(0x02);          // 按右键 = 按配置热键
        ctx.feed(1, 1000, true);
        wait_frames(ctx);
        check(any_move(ctx.output->snapshot()), "A1 默认热键(右键)按下 -> 出移动");
        ctx.thread.stop();
    }
    {
        TestCtx ctx;
        if (!ctx.start()) { std::printf("[FAIL] A2 start\n"); return 1; }
        ctx.buttons.store(0x01);          // 按左键 ≠ 配置热键(右键)
        ctx.feed(1, 1000, true);
        wait_frames(ctx);
        ctx.thread.stop();
        check(all_zero(ctx.output->snapshot()), "A2 默认热键下按其它键 -> HID=0,0");
    }

    // ---- B. 改为另一按键（左键 0x01）----
    {
        TestCtx ctx;
        ctx.reconfigure(0x01, 0x00, 0, true);   // 主热键改为左键
        if (!ctx.start()) { std::printf("[FAIL] B start\n"); return 1; }
        ctx.buttons.store(0x01);
        ctx.feed(1, 1000, true);
        wait_frames(ctx);
        check(any_move(ctx.output->snapshot()), "B1 配置为左键+按左键 -> 出移动");
        ctx.thread.stop();
    }
    {
        TestCtx ctx;
        ctx.reconfigure(0x01, 0x00, 0, true);
        if (!ctx.start()) { std::printf("[FAIL] B2 start\n"); return 1; }
        ctx.buttons.store(0x02);          // 按右键 ≠ 新配置(左键)
        ctx.feed(1, 1000, true);
        wait_frames(ctx);
        ctx.thread.stop();
        check(all_zero(ctx.output->snapshot()), "B2 配置为左键+按右键 -> HID=0,0");
    }

    // ---- C. 改为侧键（0x08 = 侧1）----
    {
        TestCtx ctx;
        ctx.reconfigure(0x08, 0x00, 0, true);
        if (!ctx.start()) { std::printf("[FAIL] C start\n"); return 1; }
        ctx.buttons.store(0x08);          // 按侧键
        ctx.feed(1, 1000, true);
        wait_frames(ctx);
        check(any_move(ctx.output->snapshot()), "C1 配置为侧键+按侧键 -> 出移动");
        ctx.thread.stop();
    }
    {
        TestCtx ctx;
        ctx.reconfigure(0x08, 0x00, 0, true);
        if (!ctx.start()) { std::printf("[FAIL] C2 start\n"); return 1; }
        ctx.buttons.store(0x06);          // 左+中组合，不含侧键位
        ctx.feed(1, 1000, true);
        wait_frames(ctx);
        ctx.thread.stop();
        check(all_zero(ctx.output->snapshot()), "C2 配置为侧键+按其它键 -> HID=0,0");
    }

    // ---- D. 双热键 any：主(0x02) 或 副(0x08) 任一按下即触发 ----
    {
        TestCtx ctx;
        ctx.reconfigure(0x02, 0x08, 0, true);
        if (!ctx.start()) { std::printf("[FAIL] D start\n"); return 1; }
        ctx.buttons.store(0x08);          // 只按副热键
        ctx.feed(1, 1000, true);
        wait_frames(ctx);
        check(any_move(ctx.output->snapshot()), "D1 any模式+只按副热键 -> 出移动");
        ctx.thread.stop();
    }
    {
        TestCtx ctx;
        ctx.reconfigure(0x02, 0x08, 0, true);
        if (!ctx.start()) { std::printf("[FAIL] D2 start\n"); return 1; }
        ctx.buttons.store(0x02);          // 只按主热键
        ctx.feed(1, 1000, true);
        wait_frames(ctx);
        check(any_move(ctx.output->snapshot()), "D2 any模式+只按主热键 -> 出移动");
        ctx.thread.stop();
    }
    {
        TestCtx ctx;
        ctx.reconfigure(0x02, 0x08, 0, true);
        if (!ctx.start()) { std::printf("[FAIL] D3 start\n"); return 1; }
        ctx.buttons.store(0x01);          // 按无关键
        ctx.feed(1, 1000, true);
        wait_frames(ctx);
        ctx.thread.stop();
        check(all_zero(ctx.output->snapshot()), "D3 any模式+按无关键 -> HID=0,0");
    }

    // ---- E. 双热键 all：必须 0x02|0x08 = 0x0A 同时按下 ----
    {
        TestCtx ctx;
        ctx.reconfigure(0x02, 0x08, 1, true);   // mode=1 → all
        if (!ctx.start()) { std::printf("[FAIL] E start\n"); return 1; }
        ctx.buttons.store(0x0A);          // 两键同按
        ctx.feed(1, 1000, true);
        wait_frames(ctx);
        check(any_move(ctx.output->snapshot()), "E1 all模式+双键同按 -> 出移动");
        ctx.thread.stop();
    }
    {
        TestCtx ctx;
        ctx.reconfigure(0x02, 0x08, 1, true);
        if (!ctx.start()) { std::printf("[FAIL] E2 start\n"); return 1; }
        ctx.buttons.store(0x02);          // 只按主键 → all 模式不放行
        ctx.feed(1, 1000, true);
        wait_frames(ctx);
        ctx.thread.stop();
        check(all_zero(ctx.output->snapshot()), "E2 all模式+只按单键 -> HID=0,0");
    }

    // ---- F. 运行中改配置：不重启，下一周期生效 ----
    {
        TestCtx ctx;                      // 初始: 右键
        if (!ctx.start()) { std::printf("[FAIL] F start\n"); return 1; }
        ctx.buttons.store(0x02);          // 按着右键
        ctx.feed(1, 1000, true);
        wait_frames(ctx);
        check(any_move(ctx.output->snapshot()), "F1 改配置前(右键配置) -> 出移动");
        const size_t n_before = ctx.output->snapshot().size();
        ctx.reconfigure(0x08, 0x00, 0, true);   // 运行中把热键改成侧键（线程不重启）
        ctx.feed(2, 16000, true);         // 仍按着右键、目标仍在
        wait_frames(ctx);
        ctx.thread.stop();
        auto acts = ctx.output->snapshot();
        // 右键不再是配置热键 → 改配置之后的所有输出必须为 0,0
        check(all_zero(acts, n_before), "F2 运行中改热键(右键->侧键)后旧键失效 -> HID=0,0");
    }
    {
        TestCtx ctx;                      // 初始: 右键
        if (!ctx.start()) { std::printf("[FAIL] F3 start\n"); return 1; }
        ctx.buttons.store(0x02);
        ctx.reconfigure(0x02, 0x00, 0, false);  // 运行中关 mouse.enabled 总开关
        ctx.feed(1, 1000, true);          // 热键仍按着、目标仍在
        wait_frames(ctx);
        ctx.thread.stop();
        auto st = ctx.thread.status();
        check(all_zero(ctx.output->snapshot()), "F4 运行中关总开关 -> HID=0,0");
        check(st.gated_frames > 0, "F5 运行中关总开关 -> 记录 gated_frames>0");
    }

    // ---- G. 未按配置热键时绝不输出鼠标移动（混淆矩阵兜底）----
    {
        // 六种非配置键位组合（含 0、单键、多键混杂），配置为左键 0x01。
        const uint16_t wrong[] = {0x00, 0x02, 0x04, 0x08, 0x10, 0x06, 0x1E, 0xFE};
        bool all_ok = true;
        for (uint16_t bits : wrong) {
            TestCtx ctx;
            ctx.reconfigure(0x01, 0x00, 0, true);
            if (!ctx.start()) { all_ok = false; break; }
            ctx.buttons.store(bits);      // 全都不是配置热键
            ctx.feed(1, 1000, true);
            wait_frames(ctx);
            ctx.thread.stop();
            if (!all_zero(ctx.output->snapshot())) { all_ok = false; break; }
        }
        check(all_ok, "G 八种非配置键位组合 -> 全部 HID=0,0");
    }
    {
        // enabled=false 时即使按对热键也必须全零（总开关优先于热键）。
        TestCtx ctx;
        ctx.reconfigure(0x02, 0x00, 0, false);
        if (!ctx.start()) { std::printf("[FAIL] G2 start\n"); return 1; }
        ctx.buttons.store(0x02);
        ctx.feed(1, 1000, true);
        wait_frames(ctx);
        ctx.thread.stop();
        check(all_zero(ctx.output->snapshot()), "G2 enabled=false+按对热键 -> HID=0,0");
    }

    if (fails == 0) std::printf("test_hotkey_config: PASS\n");
    else std::printf("test_hotkey_config: %d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
