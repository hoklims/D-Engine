#include "Runtime/SimState.h"

#include <cmath>
#include <cstdio>
#include <cstring>

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

// =================================================================
//  Bootstrap
// =================================================================

static void test_bootstrap() {
    de::SimState sim;
    sim.bootstrap();

    check(sim.world.entity_count() == 4, "bootstrap creates 4 entities");

    de::SimSnapshot snap = sim.snapshot();
    check(snap.entity_count == 4, "snapshot entity_count == 4");
    check(snap.tick_count == 0, "snapshot tick_count == 0 before tick");
    check(snap.system_count == 2, "snapshot system_count == 2");
}

// =================================================================
//  Single tick (acceleration=0, so velocity unchanged -- backward compat)
// =================================================================

static void test_single_tick() {
    de::SimState sim;
    sim.bootstrap();

    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.tick_count == 1, "tick_count == 1 after one tick");

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

// =================================================================
//  Multiple ticks (acceleration=0)
// =================================================================

static void test_multi_tick() {
    de::SimState sim;
    sim.bootstrap();

    constexpr int ticks = 10;
    for (int i = 0; i < ticks; ++i) {
        sim.tick(1.0);
    }

    de::SimSnapshot snap = sim.snapshot();
    check(snap.tick_count == 10, "multi_tick: tick_count == 10");

    float sum_x = 0.0f;
    float sum_y = 0.0f;
    sim.world.each<de::Position>([&](de::EntityId, de::Position& p) {
        sum_x += p.x;
        sum_y += p.y;
    });
    check(approx(sum_x, 100.0f), "multi_tick: sum_x == 100");
    check(approx(sum_y, 20.0f), "multi_tick: sum_y == 20");
}

// =================================================================
//  Fractional dt
// =================================================================

static void test_fractional_dt() {
    de::SimState sim;
    sim.bootstrap();

    constexpr double dt = 1.0 / 60.0;
    for (int i = 0; i < 60; ++i) {
        sim.tick(dt);
    }

    float sum_y = 0.0f;
    sim.world.each<de::Position>([&](de::EntityId, de::Position& p) {
        sum_y += p.y;
    });
    check(approx(sum_y, 2.0f, 0.01f), "fractional_dt: sum_y ~= 2.0");
}

// =================================================================
//  Shutdown
// =================================================================

static void test_shutdown() {
    de::SimState sim;
    sim.bootstrap();
    sim.tick(1.0);
    sim.tick(1.0);

    sim.shutdown();

    de::SimSnapshot snap = sim.snapshot();
    check(snap.entity_count == 0, "shutdown: entity_count == 0");
    check(snap.tick_count == 0, "shutdown: tick_count == 0");
    check(snap.system_count == 0, "shutdown: system_count == 0");

    sim.bootstrap();
    check(sim.world.entity_count() == 4, "shutdown: re-bootstrap works");
    check(sim.system_count() == 2, "shutdown: pipeline restored after re-bootstrap");
}

// =================================================================
//  Snapshot reflects live state
// =================================================================

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

// =================================================================
//  Double bootstrap is idempotent
// =================================================================

static void test_double_bootstrap() {
    de::SimState sim;
    sim.bootstrap();
    sim.bootstrap();

    check(sim.world.entity_count() == 4,
          "double_bootstrap: still 4 entities, not 8");

    de::SimSnapshot snap = sim.snapshot();
    check(snap.tick_count == 0,
          "double_bootstrap: tick_count reset to 0");
    check(snap.system_count == 2,
          "double_bootstrap: system_count still 2, not 4");
}

// =================================================================
//  Bootstrap after ticks
// =================================================================

static void test_bootstrap_after_ticks() {
    de::SimState sim;
    sim.bootstrap();
    sim.tick(1.0);
    sim.tick(1.0);

    sim.bootstrap();

    de::SimSnapshot snap = sim.snapshot();
    check(snap.entity_count == 4,
          "bootstrap_after_ticks: entity_count == 4");
    check(snap.tick_count == 0,
          "bootstrap_after_ticks: tick_count == 0");

    float sum_x = 0.0f;
    sim.world.each<de::Position>([&](de::EntityId, de::Position& p) {
        sum_x += p.x;
    });
    check(approx(sum_x, 60.0f),
          "bootstrap_after_ticks: positions are fresh (sum_x == 60)");
}

// =================================================================
//  Bootstrap after shutdown
// =================================================================

static void test_bootstrap_after_shutdown() {
    de::SimState sim;
    sim.bootstrap();
    sim.tick(1.0);
    sim.shutdown();
    sim.bootstrap();

    de::SimSnapshot snap = sim.snapshot();
    check(snap.entity_count == 4,
          "bootstrap_after_shutdown: entity_count == 4");
    check(snap.tick_count == 0,
          "bootstrap_after_shutdown: tick_count == 0");
}

// =================================================================
//  Snapshot coherence after re-bootstrap
// =================================================================

static void test_snapshot_coherence_re_bootstrap() {
    de::SimState sim;
    sim.bootstrap();
    sim.tick(1.0);
    sim.tick(1.0);
    sim.tick(1.0);

    de::SimSnapshot before = sim.snapshot();
    check(before.tick_count == 3,
          "snapshot_coherence: 3 ticks before re-bootstrap");

    sim.bootstrap();

    de::SimSnapshot after = sim.snapshot();
    check(after.entity_count == 4,
          "snapshot_coherence: 4 entities after re-bootstrap");
    check(after.tick_count == 0,
          "snapshot_coherence: tick_count 0 after re-bootstrap");

    sim.tick(1.0);
    de::SimSnapshot post_tick = sim.snapshot();
    check(post_tick.tick_count == 1,
          "snapshot_coherence: tick_count 1 after one fresh tick");
}

// =================================================================
//  System order matters: velocity integrated before position
// =================================================================

static void test_system_order_matters() {
    de::SimState sim;
    sim.bootstrap();

    // Entity with non-zero acceleration to prove order.
    de::EntityId e = sim.world.create();
    sim.world.set(e, de::Position{0.0f, 0.0f});
    sim.world.set(e, de::Velocity{0.0f, 0.0f});
    sim.world.set(e, de::Acceleration{2.0f, 0.0f});

    sim.tick(1.0);

    // Correct order (vel first, then pos):
    //   v.dx = 0 + 2*1 = 2
    //   p.x  = 0 + 2*1 = 2
    //
    // Wrong order (pos first, then vel) would give p.x = 0.
    check(approx(sim.world.get<de::Velocity>(e)->dx, 2.0f),
          "order: velocity == 2.0");
    check(approx(sim.world.get<de::Position>(e)->x, 2.0f),
          "order: position uses UPDATED velocity (proves vel-before-pos)");
}

// =================================================================
//  Acceleration accumulates over multiple ticks
// =================================================================

static void test_acceleration_multi_tick() {
    de::SimState sim;
    sim.bootstrap();

    de::EntityId e = sim.world.create();
    sim.world.set(e, de::Position{0.0f, 0.0f});
    sim.world.set(e, de::Velocity{0.0f, 0.0f});
    sim.world.set(e, de::Acceleration{1.0f, 0.0f});

    // Tick 1: v=1, p=0+1=1
    // Tick 2: v=2, p=1+2=3
    // Tick 3: v=3, p=3+3=6
    sim.tick(1.0);
    sim.tick(1.0);
    sim.tick(1.0);

    check(approx(sim.world.get<de::Velocity>(e)->dx, 3.0f),
          "accel_multi: v.dx == 3 after 3 ticks");
    check(approx(sim.world.get<de::Position>(e)->x, 6.0f),
          "accel_multi: p.x == 6 after 3 ticks (1+2+3)");
}

// =================================================================
//  Pipeline telemetry: names, timing, entity counts
// =================================================================

static void test_pipeline_telemetry() {
    de::SimState sim;
    sim.bootstrap();
    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.system_count == 2, "telemetry: 2 systems");

    check(std::strcmp(snap.systems[0].name, "IntegrateVelocity") == 0,
          "telemetry: system 0 is IntegrateVelocity");
    check(snap.systems[0].elapsed_s >= 0.0,
          "telemetry: system 0 elapsed >= 0");
    check(snap.systems[0].entities_processed == 4,
          "telemetry: IntegrateVelocity processed 4 entities");

    check(std::strcmp(snap.systems[1].name, "IntegratePosition") == 0,
          "telemetry: system 1 is IntegratePosition");
    check(snap.systems[1].elapsed_s >= 0.0,
          "telemetry: system 1 elapsed >= 0");
    check(snap.systems[1].entities_processed == 4,
          "telemetry: IntegratePosition processed 4 entities");
}

// =================================================================
//  Telemetry: entity counts differ between systems
// =================================================================

static void test_telemetry_entity_count_varies() {
    de::SimState sim;
    sim.bootstrap();

    // Entity with Position+Velocity but NO Acceleration.
    de::EntityId e = sim.world.create();
    sim.world.set(e, de::Position{0.0f, 0.0f});
    sim.world.set(e, de::Velocity{1.0f, 0.0f});

    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    // IntegrateVelocity needs Velocity+Acceleration: 4 bootstrap entities only.
    check(snap.systems[0].entities_processed == 4,
          "vary_count: IntegrateVelocity processes 4 (not 5)");
    // IntegratePosition needs Position+Velocity: all 5.
    check(snap.systems[1].entities_processed == 5,
          "vary_count: IntegratePosition processes 5");
}

// =================================================================
//  main
// =================================================================

int main() {
    test_bootstrap();
    test_single_tick();
    test_multi_tick();
    test_fractional_dt();
    test_shutdown();
    test_snapshot_updates();

    // Bootstrap lifecycle
    test_double_bootstrap();
    test_bootstrap_after_ticks();
    test_bootstrap_after_shutdown();
    test_snapshot_coherence_re_bootstrap();

    // Pipeline order and acceleration
    test_system_order_matters();
    test_acceleration_multi_tick();

    // Telemetry
    test_pipeline_telemetry();
    test_telemetry_entity_count_varies();

    std::printf("\nRuntimeEcsTest results: %d passed, %d failed\n",
                g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
