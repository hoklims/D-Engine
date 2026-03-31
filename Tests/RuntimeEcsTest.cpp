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
    check(approx(sum_x, 64.0f), "single_tick: sum_x == 64");
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

    de::EntityId e = sim.world.create();
    sim.world.set(e, de::Position{0.0f, 0.0f});
    sim.world.set(e, de::Velocity{0.0f, 0.0f});
    sim.world.set(e, de::Acceleration{2.0f, 0.0f});

    sim.tick(1.0);

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

    de::EntityId e = sim.world.create();
    sim.world.set(e, de::Position{0.0f, 0.0f});
    sim.world.set(e, de::Velocity{1.0f, 0.0f});

    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.systems[0].entities_processed == 4,
          "vary_count: IntegrateVelocity processes 4 (not 5)");
    check(snap.systems[1].entities_processed == 5,
          "vary_count: IntegratePosition processes 5");
}

// =================================================================
//  Test systems for deferred command testing
// =================================================================

// Queues destroy for every entity with Position.x > 25.
static uint32_t destroy_high_x(de::World& world, float, de::CommandBuffer& cmds) {
    uint32_t count = 0;
    world.each<de::Position>([&](de::EntityId id, de::Position& p) {
        ++count;
        if (p.x > 25.0f) {
            cmds.destroy(id);
        }
    });
    return count;
}

// Spawns one entity at Position(100, 0).
static uint32_t spawn_one(de::World& world, float, de::CommandBuffer& cmds) {
    uint32_t count = 0;
    world.each<de::Position>([&](de::EntityId, de::Position&) { ++count; });
    cmds.spawn([](de::World& w, de::EntityId e) {
        w.set(e, de::Position{100.0f, 0.0f});
        w.set(e, de::Velocity{0.0f, 0.0f});
        w.set(e, de::Acceleration{0.0f, 0.0f});
    });
    return count;
}

// Counts entities and stores in a global for external verification.
static uint32_t g_count_seen = 0;

static uint32_t count_only(de::World& world, float, de::CommandBuffer&) {
    uint32_t count = 0;
    world.each<de::Position>([&](de::EntityId, de::Position&) { ++count; });
    g_count_seen = count;
    return count;
}

// =================================================================
//  Deferred destroy: entity survives iteration, dies after tick
// =================================================================

static void test_deferred_destroy() {
    de::SimState sim;
    sim.bootstrap();  // pos.x = 0, 10, 20, 30
    sim.add_system("DestroyHighX", destroy_high_x);

    sim.tick(1.0);
    // After integration: pos.x = 1, 11, 21, 31
    // DestroyHighX queues destroy for x=31.  Apply removes it.

    check(sim.world.entity_count() == 3,
          "deferred_destroy: 3 entities remain");

    bool found_31 = false;
    sim.world.each<de::Position>([&](de::EntityId, de::Position& p) {
        if (approx(p.x, 31.0f)) found_31 = true;
    });
    check(!found_31, "deferred_destroy: entity at x~31 is gone");
}

// =================================================================
//  Deferred spawn: entity appears after tick
// =================================================================

static void test_deferred_spawn() {
    de::SimState sim;
    sim.bootstrap();
    sim.add_system("SpawnOne", spawn_one);

    sim.tick(1.0);

    check(sim.world.entity_count() == 5,
          "deferred_spawn: 5 entities after spawn");

    bool found_100 = false;
    sim.world.each<de::Position>([&](de::EntityId, de::Position& p) {
        if (approx(p.x, 100.0f)) found_100 = true;
    });
    check(found_100, "deferred_spawn: spawned entity at x=100 exists");
}

// =================================================================
//  No invalidation: entities queued for destroy still visible in later systems
// =================================================================

static void test_no_invalidation_during_iteration() {
    de::SimState sim;
    sim.bootstrap();
    sim.add_system("DestroyHighX", destroy_high_x);
    sim.add_system("CountAfter", count_only);

    g_count_seen = 0;
    sim.tick(1.0);

    // CountAfter runs AFTER DestroyHighX.
    // Destroy is deferred, so CountAfter still sees all 4 entities.
    check(g_count_seen == 4,
          "no_invalidation: later system still sees 4 (destroy not applied yet)");
    check(sim.world.entity_count() == 3,
          "no_invalidation: 3 entities after apply");
}

// =================================================================
//  Deterministic apply at end of tick
// =================================================================

static void test_deterministic_apply() {
    de::SimState sim;
    sim.bootstrap();
    sim.add_system("SpawnOne", spawn_one);
    sim.add_system("CountAfter", count_only);

    g_count_seen = 0;
    sim.tick(1.0);

    // SpawnOne queues a spawn.  CountAfter runs after, should see
    // the original 4 (spawn not applied until end of tick).
    check(g_count_seen == 4,
          "deterministic: CountAfter sees 4 (spawn not applied yet)");
    check(sim.world.entity_count() == 5,
          "deterministic: 5 entities after apply");
}

// =================================================================
//  Snapshot reports command counts
// =================================================================

static void test_snapshot_cmd_stats() {
    de::SimState sim;
    sim.bootstrap();
    sim.add_system("DestroyHighX", destroy_high_x);
    sim.add_system("SpawnOne", spawn_one);

    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    // DestroyHighX queues 1 destroy, SpawnOne queues 1 spawn = 2 total
    check(snap.cmds_queued == 2, "cmd_stats: 2 commands queued");
    check(snap.cmds_applied == 2, "cmd_stats: 2 commands applied");
    // 4 - 1 + 1 = 4
    check(snap.entity_count == 4, "cmd_stats: entity_count == 4");
}

// =================================================================
//  No commands when systems don't use CommandBuffer
// =================================================================

static void test_no_commands_baseline() {
    de::SimState sim;
    sim.bootstrap();

    sim.tick(1.0);

    de::SimSnapshot snap = sim.snapshot();
    check(snap.cmds_queued == 0, "baseline: 0 commands queued");
    check(snap.cmds_applied == 0, "baseline: 0 commands applied");
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

    // Deferred commands
    test_deferred_destroy();
    test_deferred_spawn();
    test_no_invalidation_during_iteration();
    test_deterministic_apply();
    test_snapshot_cmd_stats();
    test_no_commands_baseline();

    std::printf("\nRuntimeEcsTest results: %d passed, %d failed\n",
                g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
