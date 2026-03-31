#include "Runtime/FixedStep.h"
#include "Runtime/FrameInfo.h"

#include <cstdio>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::printf("FAIL: %s\n", name);
    }
}

// Simulate N frames by feeding scripted deltas into FixedStep
// and maintaining FrameInfo exactly as Engine does.
struct FakeLoop {
    de::FixedStep fs;
    de::FrameInfo info = {};

    void init(double hz, double max_delta, uint32_t max_steps) {
        fs.init(hz, max_delta, max_steps);
    }

    void frame(double raw_delta) {
        ++info.frame_index;
        info.raw_frame_delta = raw_delta;

        de::FixedStepResult r = fs.consume(raw_delta);
        info.clamped_frame_delta = r.clamped_delta;
        info.steps_this_frame = r.steps_taken;
        info.step_cap_hit = r.step_cap_hit;
        info.presentation_alpha = r.alpha;

        for (uint32_t i = 0; i < r.steps_taken; ++i) {
            ++info.sim_tick_index;
        }
    }
};

static void test_frame_index_increments() {
    FakeLoop loop;
    loop.init(60.0, 0.25, 8);

    loop.frame(1.0 / 60.0);
    check(loop.info.frame_index == 1, "frame_index == 1 after 1 frame");

    loop.frame(1.0 / 60.0);
    check(loop.info.frame_index == 2, "frame_index == 2 after 2 frames");

    loop.frame(1.0 / 60.0);
    check(loop.info.frame_index == 3, "frame_index == 3 after 3 frames");
}

static void test_tick_index_increments() {
    FakeLoop loop;
    loop.init(60.0, 0.25, 8);

    // One frame with exactly 1 step.
    loop.frame(1.0 / 60.0);
    check(loop.info.sim_tick_index == 1, "tick_index == 1 after 1 step");

    // One frame with 2 steps.
    loop.frame(2.0 / 60.0);
    check(loop.info.sim_tick_index == 3, "tick_index == 3 after 1+2 steps");
}

static void test_clamped_delta_reported() {
    FakeLoop loop;
    loop.init(60.0, 0.25, 8);

    // Normal delta: clamped == raw.
    loop.frame(0.01);
    check(loop.info.clamped_frame_delta == 0.01, "clamped == raw for small delta");

    // Huge spike: clamped to 0.25.
    loop.frame(5.0);
    check(loop.info.clamped_frame_delta == 0.25, "clamped to max_frame_delta for spike");
    check(loop.info.raw_frame_delta == 5.0, "raw_frame_delta preserved");
}

static void test_step_cap_hit_flag() {
    FakeLoop loop;
    loop.init(60.0, 0.25, 4);  // low cap

    // Normal frame: no cap.
    loop.frame(1.0 / 60.0);
    check(!loop.info.step_cap_hit, "cap not hit on normal frame");

    // Spike that needs > 4 steps: cap hit.
    loop.frame(0.25);
    check(loop.info.step_cap_hit, "cap hit on overloaded frame");
    check(loop.info.steps_this_frame == 4, "steps == max when cap hit");
}

static void test_alpha_coherence() {
    FakeLoop loop;
    loop.init(60.0, 0.25, 8);

    // Feed 1.5 steps worth.
    loop.frame(1.5 / 60.0);
    check(loop.info.presentation_alpha >= 0.0, "alpha >= 0");
    check(loop.info.presentation_alpha < 1.0, "alpha < 1");
    check(loop.info.presentation_alpha > 0.4 && loop.info.presentation_alpha < 0.6,
          "alpha ~0.5 for 1.5 step-widths");
}

static void test_zero_delta_frame() {
    FakeLoop loop;
    loop.init(60.0, 0.25, 8);

    loop.frame(0.0);
    check(loop.info.frame_index == 1, "frame_index increments even on zero delta");
    check(loop.info.steps_this_frame == 0, "zero steps on zero delta");
    check(loop.info.sim_tick_index == 0, "tick_index unchanged on zero delta");
    check(loop.info.presentation_alpha == 0.0, "alpha == 0 on zero delta");
}

int main() {
    test_frame_index_increments();
    test_tick_index_increments();
    test_clamped_delta_reported();
    test_step_cap_hit_flag();
    test_alpha_coherence();
    test_zero_delta_frame();

    std::printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
