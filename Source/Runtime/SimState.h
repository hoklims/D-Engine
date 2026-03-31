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

// Budget thresholds for per-tick cost evaluation.
// max_tick_s covers full tick wall-clock time (cull, systems, command
// apply, stats, hash).  Purely evaluative -- no auto-throttle.
// Persists across bootstrap()/shutdown(); only status resets.
struct SimBudgetConfig {
    double   max_tick_s            = 0.004;   // total tick CPU (4ms @ 60Hz ~ 24% frame)
    double   max_system_s          = 0.002;   // single system ceiling
    uint32_t max_targeting_scanned = 5000;    // spatial scan work
    uint32_t max_melee_checks      = 2000;    // broadphase pair checks
    uint32_t max_lod_t0_count      = 200;     // full-fidelity agent cap
};

// Budget response policy -- controls automatic LOD degradation.
//
// Contract:
//   What CAN be degraded: LOD distance thresholds (t1/t2/t3_distance)
//     -> pushes agents to higher tiers -> gated systems do less work.
//   What MUST NOT be degraded: combat core (AttackTargets, ResolveDamage,
//     RemoveDead), physics (IntegrateVelocity, IntegratePosition),
//     target selection (SelectTargets), melee broadphase (MeleeBroadphase).
//   When: response computed after tick N evaluation, applied at tick N+1.
//     Never mid-tick.
//
// Persists across bootstrap()/shutdown() (like SimBudgetConfig).
struct SimBudgetResponseConfig {
    uint8_t  max_pressure       = 4;      // max degradation level (0 disables)
    float    shrink_per_level   = 0.20f;  // LOD threshold reduction per level
    uint32_t recovery_ticks     = 3;      // consecutive healthy ticks to reduce pressure by 1
    bool     enabled            = false;  // must be explicitly enabled
};

// Runtime state of the budget response -- resets on bootstrap/shutdown.
struct SimBudgetResponseState {
    uint8_t  pressure_level      = 0;     // 0 = no degradation, max = config.max_pressure
    uint32_t consecutive_healthy = 0;     // ticks since last violation
    bool     active              = false; // true when pressure_level > 0
    float    lod_distance_scale  = 1.0f;  // current multiplier on LOD thresholds
};

// Result of per-tick budget evaluation.  Read-only diagnostic.
struct SimBudgetStatus {
    bool     within_budget         = true;
    uint32_t violation_count       = 0;

    // Per-contract flags.
    bool     tick_over             = false;
    bool     system_over           = false;
    bool     targeting_over        = false;
    bool     melee_over            = false;
    bool     lod_t0_over           = false;

    // Diagnostics (tick_elapsed_s = full tick wall-clock).
    double   tick_elapsed_s        = 0.0;
    char     hottest_system[k_system_name_max] = {};
    double   hottest_system_s      = 0.0;
    uint32_t targeting_scanned     = 0;
    uint32_t melee_checks          = 0;
    uint32_t lod_t0_count          = 0;
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

    // Budget evaluation (this tick).
    SimBudgetStatus budget;

    // Budget response telemetry (this tick).
    uint8_t  budget_pressure_level  = 0;
    bool     budget_response_active = false;
    float    budget_lod_scale       = 1.0f;
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
// Tick flow (wall-clock timed end-to-end for budget evaluation):
//   1. Cull pre-dead agents
//   2. Clear command buffer, create WorldView
//   3. Run all systems in order (individually timed, receive WorldView&)
//   4. Apply all deferred commands at once (spawns first, then destroys)
//   5. Update crowd stats, increment tick_count, compute sim hash
//   6. Evaluate budget contracts (outside the measured region)
//
// Budget contract:
//   SimBudgetConfig persists across bootstrap()/shutdown().
//   Only SimBudgetStatus (per-tick result) resets on bootstrap/shutdown.
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

    void set_budget_config(const SimBudgetConfig& cfg);
    const SimBudgetStatus& budget_status() const;

    void set_budget_response_config(const SimBudgetResponseConfig& cfg);
    const SimBudgetResponseState& budget_response_state() const;

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
    SimBudgetConfig         budget_config_;
    SimBudgetStatus         budget_status_;
    SimBudgetResponseConfig budget_response_config_;
    SimBudgetResponseState  budget_response_state_;
    float                   lod_base_t1_ = 30.0f;
    float                   lod_base_t2_ = 60.0f;
    float                   lod_base_t3_ = 100.0f;

    void register_systems();
    void register_crowd_systems();
    void update_crowd_stats();
    void cull_pre_dead();
    void build_nav_fields();
    void evaluate_budget(double tick_wall_s);
    void apply_budget_response();
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
