#include "Runtime/SimState.h"

#include <cmath>
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

static bool approx(float a, float b, float eps = 1e-5f) {
    return std::fabsf(a - b) < eps;
}

// -----------------------------------------------------------------
//  Bootstrap creates expected entities
// -----------------------------------------------------------------
static void test_bootstrap() {
    de::SimState sim;
    sim.bootstrap();

    check(sim.world.entity_count() == 4, "bootstrap creates 4 entities");

    de::SimSnapshot snap = sim.snapshot();
    check(snap.entity_count == 4, "snapshot entity_count == 4");
    check(snap.tick_count == 0, "snapshot tick_count == 0 before tick");
}

// -----------------------------------------------------------------
//  Single tick evolves components
// -----------------------------------------------------------------
static void test_single_tick() {
    de::SimState sim;
    sim.bootstrap();

    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.tick_count == 1, "tick_count == 1 after one tick");

    // After bootstrap, entities have positions (0,0), (10,0), (20,0), (30,0)
    // and velocity (1, 0.5).  After dt=1: positions become (1, 0.5), (11, 0.5), etc.
    int count = 0;
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    sim.world.each<de::Position, de::Velocity>(
        [&](de::EntityId, de::Position& p, de::Velocity&) {
            sum_x += p.x;
            sum_y += p.y;
            ++count;
        });

    check(count == 4, "single_tick: 4 entities iterated");
    // sum_x = (0+1) + (10+1) + (20+1) + (30+1) = 64
    check(approx(sum_x, 64.0f), "single_tick: sum_x == 64");
    // sum_y = 4 * 0.5 = 2
    check(approx(sum_y, 2.0f), "single_tick: sum_y == 2");
}

// -----------------------------------------------------------------
//  Multiple ticks accumulate correctly
// -----------------------------------------------------------------
static void test_multi_tick() {
    de::SimState sim;
    sim.bootstrap();

    constexpr int ticks = 10;
    for (int i = 0; i < ticks; ++i) {
        sim.tick(1.0);
    }

    de::SimSnapshot snap = sim.snapshot();
    check(snap.tick_count == 10, "multi_tick: tick_count == 10");

    // After 10 ticks at dt=1:
    // entity 0: x = 0 + 10*1 = 10, y = 0 + 10*0.5 = 5
    // entity 1: x = 10 + 10 = 20
    // entity 2: x = 20 + 10 = 30
    // entity 3: x = 30 + 10 = 40
    float sum_x = 0.0f;
    float sum_y = 0.0f;
    sim.world.each<de::Position>([&](de::EntityId, de::Position& p) {
        sum_x += p.x;
        sum_y += p.y;
    });
    // sum_x = 10 + 20 + 30 + 40 = 100
    check(approx(sum_x, 100.0f), "multi_tick: sum_x == 100");
    // sum_y = 4 * 5 = 20
    check(approx(sum_y, 20.0f), "multi_tick: sum_y == 20");
}

// -----------------------------------------------------------------
//  Fractional dt works correctly
// -----------------------------------------------------------------
static void test_fractional_dt() {
    de::SimState sim;
    sim.bootstrap();

    // 60 ticks at dt = 1/60
    constexpr double dt = 1.0 / 60.0;
    for (int i = 0; i < 60; ++i) {
        sim.tick(dt);
    }

    // entity 0: x = 0 + 60*(1/60)*1 = 1.0, y = 0 + 60*(1/60)*0.5 = 0.5
    float sum_y = 0.0f;
    sim.world.each<de::Position>([&](de::EntityId, de::Position& p) {
        sum_y += p.y;
    });
    // sum_y = 4 * 0.5 = 2.0 (approximately, float accumulation)
    check(approx(sum_y, 2.0f, 0.01f), "fractional_dt: sum_y ~= 2.0");
}

// -----------------------------------------------------------------
//  Shutdown leaves clean state
// -----------------------------------------------------------------
static void test_shutdown() {
    de::SimState sim;
    sim.bootstrap();
    sim.tick(1.0);
    sim.tick(1.0);

    sim.shutdown();

    de::SimSnapshot snap = sim.snapshot();
    check(snap.entity_count == 0, "shutdown: entity_count == 0");
    check(snap.tick_count == 0, "shutdown: tick_count == 0");

    // World is reusable after shutdown
    sim.bootstrap();
    check(sim.world.entity_count() == 4, "shutdown: re-bootstrap works");
}

// -----------------------------------------------------------------
//  Snapshot reflects live state
// -----------------------------------------------------------------
static void test_snapshot_updates() {
    de::SimState sim;

    de::SimSnapshot s0 = sim.snapshot();
    check(s0.entity_count == 0, "snapshot: empty before bootstrap");

    sim.bootstrap();
    de::SimSnapshot s1 = sim.snapshot();
    check(s1.entity_count == 4, "snapshot: 4 after bootstrap");
    check(s1.tick_count == 0, "snapshot: tick 0 after bootstrap");

    sim.tick(1.0);
    de::SimSnapshot s2 = sim.snapshot();
    check(s2.tick_count == 1, "snapshot: tick 1 after first tick");
}

// -----------------------------------------------------------------
//  main
// -----------------------------------------------------------------
int main() {
    test_bootstrap();
    test_single_tick();
    test_multi_tick();
    test_fractional_dt();
    test_shutdown();
    test_snapshot_updates();

    std::printf("\nRuntimeEcsTest results: %d passed, %d failed\n",
                g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
