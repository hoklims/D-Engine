#include "Runtime/SimState.h"
#include "Runtime/SimHash.h"
#include "Runtime/CrowdComponents.h"

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

// =================================================================
//  Identical runs produce identical hash sequences
// =================================================================

static void test_deterministic_identical_runs() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 5;

    de::SimState sim1;
    sim1.bootstrap_crowd(cfg);
    auto seq1 = de::run_and_collect(sim1, 50, 1.0 / 60.0);

    de::SimState sim2;
    sim2.bootstrap_crowd(cfg);
    auto seq2 = de::run_and_collect(sim2, 50, 1.0 / 60.0);

    check(seq1 == seq2, "identical_runs: same hash sequence");
    check(seq1.first_divergence(seq2) == -1, "identical_runs: no divergence");
}

// =================================================================
//  Divergence detected when a component is mutated mid-run
// =================================================================

static void test_divergence_detection() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 5;

    de::SimState sim1;
    sim1.bootstrap_crowd(cfg);

    de::SimState sim2;
    sim2.bootstrap_crowd(cfg);

    // Run 10 ticks identically.
    de::HashSequence seq1, seq2;
    for (int i = 0; i < 10; ++i) {
        sim1.tick(1.0 / 60.0);
        sim2.tick(1.0 / 60.0);
        seq1.hashes.push_back(sim1.sim_hash());
        seq2.hashes.push_back(sim2.sim_hash());
    }

    check(seq1.first_divergence(seq2) == -1,
          "divergence: first 10 ticks identical");

    // Mutate health in sim2 -- small perturbation.
    bool mutated = false;
    sim2.world.each<de::CrowdAgent, de::Health>(
        [&](de::EntityId, de::CrowdAgent&, de::Health& hp) {
            if (!mutated) { hp.current += 1.0f; mutated = true; }
        });
    check(mutated, "divergence: mutation applied");

    // Run one more tick -- hashes must diverge.
    sim1.tick(1.0 / 60.0);
    sim2.tick(1.0 / 60.0);
    seq1.hashes.push_back(sim1.sim_hash());
    seq2.hashes.push_back(sim2.sim_hash());

    check(seq1.hashes.back() != seq2.hashes.back(),
          "divergence: hash differs after mutation");
    check(seq1.first_divergence(seq2) == 10,
          "divergence: detected at tick 10");
}

// =================================================================
//  Hash evolves each tick (simulation is not static)
// =================================================================

static void test_hash_changes_per_tick() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 5;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    auto seq = de::run_and_collect(sim, 20, 1.0 / 60.0);

    bool all_different = true;
    for (int i = 1; i < 20; ++i) {
        if (seq.hashes[static_cast<size_t>(i)] ==
            seq.hashes[static_cast<size_t>(i - 1)]) {
            all_different = false;
            break;
        }
    }
    check(all_different, "hash_changes: every tick produces a unique hash");
}

// =================================================================
//  Hash is deterministic even through combat deaths
// =================================================================

static void test_deterministic_through_deaths() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 3;
    cfg.health          = 10.0f;
    cfg.attack_damage   = 100.0f;   // one-shot kills
    cfg.attack_interval = 0.5f;
    cfg.team_spacing    = 2.0f;     // close enough to engage fast

    de::SimState sim1;
    sim1.bootstrap_crowd(cfg);
    auto seq1 = de::run_and_collect(sim1, 120, 1.0 / 60.0);

    de::SimState sim2;
    sim2.bootstrap_crowd(cfg);
    auto seq2 = de::run_and_collect(sim2, 120, 1.0 / 60.0);

    check(seq1 == seq2, "deaths: identical sequences despite combat");

    // Verify deaths actually occurred.
    check(sim1.world.entity_count() < 6,
          "deaths: agents died during simulation");
}

// =================================================================
//  Hash reflects entity destruction (fewer agents -> different hash)
// =================================================================

static void test_hash_reflects_death_count() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 3;
    cfg.health          = 10.0f;
    cfg.attack_damage   = 100.0f;
    cfg.attack_interval = 0.5f;
    cfg.team_spacing    = 2.0f;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    // Capture hash before any deaths.
    sim.tick(1.0 / 60.0);
    uint64_t hash_before = sim.sim_hash();

    // Run until deaths happen.
    for (int i = 0; i < 200; ++i) sim.tick(1.0 / 60.0);

    uint64_t hash_after = sim.sim_hash();

    check(hash_before != hash_after,
          "death_count: hash changed after deaths");
}

// =================================================================
//  Ring buffer wraps correctly
// =================================================================

static void test_hash_history_ring_buffer() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 3;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    // Run more ticks than the ring buffer holds (64).
    for (int i = 0; i < 100; ++i) sim.tick(1.0 / 60.0);

    const auto& hist = sim.hash_history();
    check(hist.count == 100, "ring: count tracks total ticks");
    check(hist.latest != 0,  "ring: latest hash is non-zero");

    // Evicted ticks return 0.
    check(hist.at(0) == 0,   "ring: evicted tick returns 0");

    // Recent ticks are available.
    check(hist.at(99) != 0,  "ring: latest tick available");
    check(hist.at(99) == hist.latest, "ring: at(99) == latest");

    // Boundary: tick 36 should be evicted (100 - 64 = 36).
    check(hist.at(35) == 0,  "ring: tick 35 evicted");
    check(hist.at(36) != 0,  "ring: tick 36 available");
}

// =================================================================
//  HashSequence::first_divergence edge cases
// =================================================================

static void test_first_divergence_length_mismatch() {
    de::HashSequence a;
    a.hashes = {1, 2, 3};

    de::HashSequence b;
    b.hashes = {1, 2, 3, 4};

    check(a.first_divergence(b) == 3,
          "divergence_len: shorter sequence detected");

    de::HashSequence c;
    c.hashes = {1, 2, 3};
    check(a.first_divergence(c) == -1,
          "divergence_len: identical sequences return -1");
}

// =================================================================
//  sim_hash appears in SimSnapshot
// =================================================================

static void test_snapshot_contains_hash() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 3;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);
    sim.tick(1.0 / 60.0);

    auto snap = sim.snapshot();
    check(snap.sim_hash != 0, "snapshot: sim_hash is populated");
    check(snap.sim_hash == sim.sim_hash(),
          "snapshot: sim_hash matches accessor");
}

// =================================================================
//  Bootstrap clears hash history
// =================================================================

static void test_bootstrap_clears_history() {
    de::CrowdConfig cfg{};
    cfg.agents_per_team = 3;

    de::SimState sim;
    sim.bootstrap_crowd(cfg);

    for (int i = 0; i < 10; ++i) sim.tick(1.0 / 60.0);
    check(sim.hash_history().count == 10, "clear: 10 hashes before reset");

    // Re-bootstrap should clear.
    sim.bootstrap_crowd(cfg);
    check(sim.hash_history().count == 0, "clear: history reset after bootstrap");
    check(sim.sim_hash() == 0,           "clear: latest hash reset");
}

// =================================================================

int main() {
    test_deterministic_identical_runs();
    test_divergence_detection();
    test_hash_changes_per_tick();
    test_deterministic_through_deaths();
    test_hash_reflects_death_count();
    test_hash_history_ring_buffer();
    test_first_divergence_length_mismatch();
    test_snapshot_contains_hash();
    test_bootstrap_clears_history();

    std::printf("\nSimHashTest: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
