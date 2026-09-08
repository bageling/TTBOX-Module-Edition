#include "test_util.hpp"
#include "mouse/PersonalMotion.hpp"

using namespace ttbox::core::aim;

TEST(personal_motion_disabled_is_identity) {
    PersonalMotion motion;
    PersonalMotionConfig cfg;
    cfg.enabled = false;
    cfg.knots = {0.2f, 0.8f};
    CHECK_EQ(motion.scale(80.0f, cfg), 1.0f);
}

TEST(personal_motion_interpolates_knots_by_error) {
    PersonalMotion motion;
    PersonalMotionConfig cfg;
    cfg.enabled = true;
    cfg.curve_blend = 1.0f;
    cfg.knots = {0.5f, 1.0f};
    CHECK_EQ(motion.scale(0.0f, cfg), 0.5f);
    CHECK_EQ(motion.scale(128.0f, cfg), 1.0f);
}

TEST(personal_motion_blend_keeps_default_component) {
    PersonalMotion motion;
    PersonalMotionConfig cfg;
    cfg.enabled = true;
    cfg.curve_blend = 0.5f;
    cfg.knots = {0.4f};
    CHECK_EQ(motion.scale(0.0f, cfg), 0.7f);
}

TEST(personal_motion_invalid_model_is_identity) {
    PersonalMotion motion;
    PersonalMotionConfig cfg;
    cfg.enabled = true;
    cfg.curve_blend = 1.0f;
    cfg.knots = {0.0f, 1.5f};
    CHECK_EQ(motion.scale(32.0f, cfg), 1.0f);
}

int main() {
    return ttbox_test::run_all();
}
