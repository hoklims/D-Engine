#include "Runtime/FixedStep.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

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

static void test_basic_accumulation() {
    de::FixedStep fs;
    fs.init(60.0, 0.25, 8);

    double dt = 1.0 / 60.0;

    // Feed exactly one step worth of delta.
    de::FixedStepResult r = fs.consume(dt);
    check(r.steps_taken == 1, "one step from one dt");
    check(r.alpha < 0.01, "alpha near zero after exact step");

    // Feed two steps worth of delta.
    r = fs.consume(dt * 2.0);
    check(r.steps_taken == 2, "two steps from 2*dt");
}

static void test_sub_step_accumulation() {
    de::FixedStep fs;
    fs.init(60.0, 0.25, 8);

    double half_dt = (1.0 / 60.0) * 0.5;

    // Two half-steps should accumulate to one full step.
    de::FixedStepResult r1 = fs.consume(half_dt);
    check(r1.steps_taken == 0, "no step from half dt");

    de::FixedStepResult r2 = fs.consume(half_dt);
    check(r2.steps_taken == 1, "one step after second half dt");
}

static void test_delta_clamp() {
    de::FixedStep fs;
    fs.init(60.0, 0.25, 8);

    // Feed a huge spike (2 seconds). Should be clamped to 0.25s.
    // 0.25 / (1/60) = 15 steps, but capped at 8.
    de::FixedStepResult r = fs.consume(2.0);
    check(r.steps_taken == 8, "clamped spike capped at max_steps");
}

static void test_max_steps_cap() {
    de::FixedStep fs;
    fs.init(60.0, 0.25, 4);  // low cap: 4 steps

    // 0.25s / (1/60) = 15 steps requested, but capped at 4.
    de::FixedStepResult r = fs.consume(0.25);
    check(r.steps_taken == 4, "steps capped at max_steps_per_frame");

    // Accumulator should have been drained to prevent spiral.
    // Next frame with a normal delta should behave normally.
    r = fs.consume(1.0 / 60.0);
    check(r.steps_taken <= 2, "no spiral after drain");
}

static void test_alpha_range() {
    de::FixedStep fs;
    fs.init(60.0, 0.25, 8);

    // Feed 1.5 steps worth of delta.
    double dt = (1.0 / 60.0) * 1.5;
    de::FixedStepResult r = fs.consume(dt);
    check(r.steps_taken == 1, "one step from 1.5*dt");
    check(r.alpha > 0.4 && r.alpha < 0.6, "alpha ~0.5 from 1.5*dt");

    // Alpha must always be in [0, 1).
    check(r.alpha >= 0.0 && r.alpha < 1.0, "alpha in [0,1)");
}

static void test_init_invalid_sim_rate() {
    de::FixedStep fs;
    check(!fs.init(0.0, 0.25, 8), "init rejects sim_rate_hz = 0");
    check(!fs.init(-30.0, 0.25, 8), "init rejects sim_rate_hz < 0");
}

static void test_init_invalid_max_frame_delta() {
    de::FixedStep fs;
    check(!fs.init(60.0, -1.0, 8), "init rejects max_frame_delta < 0");
    check(fs.init(60.0, 0.0, 8), "init accepts max_frame_delta = 0");
}

static void test_init_invalid_max_steps() {
    de::FixedStep fs;
    check(!fs.init(60.0, 0.25, 0), "init rejects max_steps = 0");
    check(fs.init(60.0, 0.25, 1), "init accepts max_steps = 1");
}

static void test_negative_delta() {
    de::FixedStep fs;
    fs.init(60.0, 0.25, 8);

    de::FixedStepResult r = fs.consume(-0.5);
    check(r.steps_taken == 0, "no steps from negative delta");
    check(r.alpha >= 0.0, "alpha >= 0 after negative delta");
    check(fs.accumulator() >= 0.0, "accumulator >= 0 after negative delta");
}

static void test_alpha_never_negative() {
    de::FixedStep fs;
    fs.init(60.0, 0.25, 8);

    // Feed a tiny positive then a zero.
    fs.consume(0.001);
    de::FixedStepResult r = fs.consume(0.0);
    check(r.alpha >= 0.0 && r.alpha < 1.0, "alpha in [0,1) after zero delta");
}

static void test_reset_clears_accumulator() {
    de::FixedStep fs;
    fs.init(60.0, 0.25, 8);

    // Accumulate a partial step (no tick produced yet).
    double half_dt = (1.0 / 60.0) * 0.5;
    fs.consume(half_dt);
    check(fs.accumulator() > 0.0, "reset: accumulator > 0 before reset");

    fs.reset();
    check(fs.accumulator() == 0.0, "reset: accumulator == 0 after reset");

    // After reset, a half-step should NOT produce a tick (no residual).
    de::FixedStepResult r = fs.consume(half_dt);
    check(r.steps_taken == 0, "reset: half dt after reset produces 0 steps");
}

int main() {
    test_basic_accumulation();
    test_sub_step_accumulation();
    test_delta_clamp();
    test_max_steps_cap();
    test_alpha_range();
    test_init_invalid_sim_rate();
    test_init_invalid_max_frame_delta();
    test_init_invalid_max_steps();
    test_negative_delta();
    test_alpha_never_negative();
    test_reset_clears_accumulator();

    std::printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
