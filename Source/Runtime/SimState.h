#pragma once

#include "ECS/World.h"
#include "ECS/WorldView.h"
#include "Runtime/CommandBuffer.h"
#include "Runtime/Components.h"
#include "Runtime/BattlefieldGrid.h"
#include "Runtime/CrowdComponents.h"
#include "Runtime/SimHash.h"

#include <cstdint>

namespace de {

static constexpr uint32_t k_max_sim_systems  = 16;
static constexpr uint32_t k_system_name_max  = 32;
static constexpr uint32_t k_max_teams        = 4;

// Per-system telemetry from the last tick.
struct SystemStats {
    char         name[k_system_name_max] = {};
    double       elapsed_s               = 0.0;
    uint32_t     entities_processed      = 0;
};

// Debug snapshot of the simulation pipeline.
struct SimSnapshot {
    uint32_t    entity_count  = 0;
    uint64_t    tick_count    = 0;
    uint32_t    system_count  = 0;
    uint32_t    cmds_queued   = 0;
    uint32_t    cmds_applied  = 0;
    SystemStats systems[k_max_sim_systems] = {};

    // Simulation hash (computed at end of tick, with tick_count post-increment).
    // sim_hash and tick_count always refer to the same completed tick.
    uint64_t    sim_hash           = 0;

    // Crowd metrics.
    uint32_t    crowd_agent_count  = 0;
    uint32_t    agents_with_target = 0;
    uint32_t    team_counts[k_max_teams] = {};

    // Combat metrics (this tick).
    uint32_t    attacks_this_tick  = 0;
    uint32_t    deaths_this_tick   = 0;

    // Targeting telemetry (this tick).
    uint32_t    targeting_candidates_scanned = 0;

    // Separation telemetry (this tick).
    uint32_t    separation_pairs_this_tick = 0;

    // Battle goal telemetry (this tick).
    uint32_t    agents_engaged = 0;

    // Navigation telemetry (this tick).
    uint32_t    nav_queries_this_tick  = 0;
    uint32_t    nav_failures_this_tick = 0;
    uint32_t    nav_blocked_cells      = 0;

    // Behavior LOD telemetry (this tick).
    uint32_t    lod_tier_counts[4]     = {};
    uint32_t    lod_skipped_this_tick  = 0;

    // Melee broadphase telemetry (this tick).
    uint32_t    melee_broadphase_checks = 0;
    uint32_t    melee_pairs_this_tick   = 0;
    uint32_t    melee_attacks_this_tick = 0;
};

// Signature for a fixed-step simulation system.
//
// Systems receive a WorldView (read/write component data only, no
// structural mutation) the timestep, and a CommandBuffer for deferred
// structural mutations.  Returns the number of entities processed.
//
// Contract enforced at compile time: WorldView does not expose
// create/destroy/set/remove.  World also asserts in Debug if a
// structural mutation is attempted during each() iteration.
using FixedSystemFn = uint32_t(*)(WorldView& view, float dt, CommandBuffer& cmds);

// A named system in the fixed-step pipeline.
// Name is owned (copied at registration) -- no dangling pointer risk.
struct FixedSystem {
    char          name[k_system_name_max] = {};
    FixedSystemFn fn                      = nullptr;
};

// Bootstrap configuration for crowd scenes.
struct CrowdConfig {
    int   agents_per_team = 10;
    float team_spacing    = 20.0f;
    float agent_spread    = 2.0f;
    float move_speed      = 3.0f;
    float health          = 100.0f;
    float attack_range    = 2.0f;
    float attack_damage   = 10.0f;
    float attack_interval     = 1.0f;
    float separation_radius   = 0.8f;
    float separation_strength = 5.0f;
    float engage_radius       = 15.0f;
};

// Obstacle definition for battlefield scenes.
struct ObstacleDef {
    int cx, cy;   // cell coordinates
};

// Configuration for a battlefield scene with navigation grid.
struct BattlefieldConfig {
    CrowdConfig       crowd       = {};
    int               grid_width  = 60;
    int               grid_height = 40;
    float             grid_cell   = 1.0f;
    float             grid_ox     = -30.0f;  // origin x (world)
    float             grid_oy     = -20.0f;  // origin y (world)
    const ObstacleDef* obstacles  = nullptr;
    int               obstacle_count = 0;
};

// Owns a World and runs an ordered pipeline of fixed systems each tick.
//
// Lifecycle contract:
//   bootstrap() is idempotent -- always resets to virgin state first
//   (world cleared, tick_count zeroed, pipeline re-registered).
//   Safe to call multiple times or after tick()/shutdown().
//
// Tick flow:
//   1. Clear command buffer
//   2. Create a WorldView over the world
//   3. Run all systems in order (they receive WorldView&, queue deferred commands)
//   4. Apply all deferred commands at once (spawns first, then destroys)
//   5. Increment tick_count
struct SimState {
    World world;

    void bootstrap();
    void bootstrap_crowd();
    void bootstrap_crowd(const CrowdConfig& cfg);
    void bootstrap_battlefield(const BattlefieldConfig& cfg);
    void tick(double step_dt);
    void shutdown();

    void add_system(const char* name, FixedSystemFn fn);

    void set_lod_center(float cx, float cy);

    SimSnapshot snapshot() const;
    uint32_t    system_count() const;

    uint64_t              sim_hash() const;
    const SimHashHistory& hash_history() const;

private:
    uint64_t      tick_count_            = 0;
    uint32_t      system_count_          = 0;
    uint32_t      cmds_queued_last_      = 0;
    uint32_t      cmds_applied_last_     = 0;
    FixedSystem   pipeline_[k_max_sim_systems]  = {};
    SystemStats   last_stats_[k_max_sim_systems] = {};
    CommandBuffer cmds_;

    uint32_t      crowd_agent_count_   = 0;
    uint32_t      agents_with_target_  = 0;
    uint32_t      team_counts_[k_max_teams] = {};
    uint32_t      attacks_this_tick_   = 0;
    uint32_t      deaths_this_tick_    = 0;
    uint32_t      targeting_candidates_scanned_ = 0;
    uint32_t      separation_pairs_this_tick_  = 0;
    uint32_t      agents_engaged_             = 0;
    uint32_t      nav_queries_this_tick_      = 0;
    uint32_t      nav_failures_this_tick_     = 0;
    uint32_t      nav_blocked_cells_          = 0;
    BattlefieldGrid nav_grids_[k_max_teams]   = {};
    const BattlefieldGrid* nav_grid_ptrs_[k_max_teams] = {};
    float         nav_goals_x_[k_max_teams]   = {};
    float         nav_goals_y_[k_max_teams]   = {};
    bool          nav_grid_active_            = false;
    uint32_t      lod_tier_counts_[4]         = {};
    uint32_t      lod_skipped_this_tick_      = 0;
    uint32_t      melee_bp_checks_           = 0;
    uint32_t      melee_pairs_this_tick_     = 0;
    uint32_t      melee_attacks_this_tick_   = 0;
    BehaviorLodConfig lod_config_;
    SimHashHistory    hash_history_;

    void register_systems();
    void register_crowd_systems();
    void update_crowd_stats();
    void cull_pre_dead();
    void build_nav_fields();
};

// Run a bootstrapped SimState for N ticks, collecting the hash after each.
inline HashSequence run_and_collect(SimState& sim, uint32_t ticks, double dt) {
    HashSequence seq;
    seq.hashes.reserve(ticks);
    for (uint32_t i = 0; i < ticks; ++i) {
        sim.tick(dt);
        seq.hashes.push_back(sim.sim_hash());
    }
    return seq;
}

}  // namespace de
