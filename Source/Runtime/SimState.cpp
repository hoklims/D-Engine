#include "Runtime/SimState.h"
#include "Runtime/CrowdComponents.h"
#include "Runtime/CrowdSystems.h"

#include <chrono>
#include <cstdio>
#include <cstring>

namespace de {

// -- Fixed systems -----------------------------------------------------------

static uint32_t integrate_velocity(WorldView& view, float dt, CommandBuffer&) {
    uint32_t count = 0;
    view.each<Velocity, Acceleration>(
        [dt, &count](EntityId, Velocity& v, Acceleration& a) {
            v.dx += a.ax * dt;
            v.dy += a.ay * dt;
            ++count;
        });
    return count;
}

static uint32_t integrate_position(WorldView& view, float dt, CommandBuffer&) {
    uint32_t count = 0;
    view.each<Position, Velocity>(
        [dt, &count](EntityId, Position& p, Velocity& v) {
            p.x += v.dx * dt;
            p.y += v.dy * dt;
            ++count;
        });
    return count;
}

// -- SimState ----------------------------------------------------------------

void SimState::bootstrap() {
    world = World{};
    tick_count_        = 0;
    system_count_      = 0;
    cmds_queued_last_  = 0;
    cmds_applied_last_ = 0;
    crowd_agent_count_  = 0;
    agents_with_target_ = 0;
    attacks_this_tick_  = 0;
    deaths_this_tick_   = 0;
    targeting_candidates_scanned_ = 0;
    separation_pairs_this_tick_  = 0;
    agents_engaged_              = 0;
    nav_queries_this_tick_       = 0;
    nav_failures_this_tick_      = 0;
    nav_blocked_cells_           = 0;
    nav_grid_active_             = false;
    set_battlefield_grids(nullptr, 0);
    for (auto& c : lod_tier_counts_) c = 0;
    lod_skipped_this_tick_       = 0;
    melee_bp_checks_             = 0;
    melee_pairs_this_tick_       = 0;
    melee_attacks_this_tick_     = 0;
    lod_config_                  = BehaviorLodConfig{};
    lod_base_t1_ = lod_config_.t1_distance;
    lod_base_t2_ = lod_config_.t2_distance;
    lod_base_t3_ = lod_config_.t3_distance;
    hash_history_.clear();
    // budget_config_ intentionally preserved across bootstrap.
    budget_status_ = SimBudgetStatus{};
    // budget_response_config_ intentionally preserved across bootstrap.
    budget_response_state_ = SimBudgetResponseState{};
    for (auto& c : team_counts_) c = 0;
    cmds_.clear();
    for (auto& s : last_stats_) s = {};

    register_systems();

    constexpr int count = 4;
    for (int i = 0; i < count; ++i) {
        EntityId e = world.create();
        world.set(e, Position{static_cast<float>(i) * 10.0f, 0.0f});
        world.set(e, Velocity{1.0f, 0.5f});
        world.set(e, Acceleration{0.0f, 0.0f});
    }
}

void SimState::bootstrap_crowd() {
    bootstrap_crowd(CrowdConfig{});
}

void SimState::bootstrap_crowd(const CrowdConfig& cfg) {
    world = World{};
    tick_count_        = 0;
    system_count_      = 0;
    cmds_queued_last_  = 0;
    cmds_applied_last_ = 0;
    crowd_agent_count_  = 0;
    agents_with_target_ = 0;
    attacks_this_tick_  = 0;
    deaths_this_tick_   = 0;
    targeting_candidates_scanned_ = 0;
    separation_pairs_this_tick_  = 0;
    agents_engaged_              = 0;
    nav_queries_this_tick_       = 0;
    nav_failures_this_tick_      = 0;
    nav_blocked_cells_           = 0;
    nav_grid_active_             = false;
    set_battlefield_grids(nullptr, 0);
    for (auto& c : lod_tier_counts_) c = 0;
    lod_skipped_this_tick_       = 0;
    melee_bp_checks_             = 0;
    melee_pairs_this_tick_       = 0;
    melee_attacks_this_tick_     = 0;
    lod_config_                  = BehaviorLodConfig{};
    lod_base_t1_ = lod_config_.t1_distance;
    lod_base_t2_ = lod_config_.t2_distance;
    lod_base_t3_ = lod_config_.t3_distance;
    hash_history_.clear();
    // budget_config_ intentionally preserved across bootstrap.
    budget_status_ = SimBudgetStatus{};
    // budget_response_config_ intentionally preserved across bootstrap.
    budget_response_state_ = SimBudgetResponseState{};
    for (auto& c : team_counts_) c = 0;
    cmds_.clear();
    for (auto& s : last_stats_) s = {};

    register_crowd_systems();

    // Derive battle center from team geometry (midpoint of the two spawns).
    float team0_x = -cfg.team_spacing;
    float team1_x =  cfg.team_spacing;
    float center_x = (team0_x + team1_x) * 0.5f;
    float center_y = static_cast<float>(cfg.agents_per_team - 1) * cfg.agent_spread * 0.5f;
    lod_config_.center_x = center_x;
    lod_config_.center_y = center_y;

    for (int team = 0; team < 2; ++team) {
        float base_x = (team == 0) ? team0_x : team1_x;
        for (int i = 0; i < cfg.agents_per_team; ++i) {
            EntityId e = world.create();
            world.set(e, CrowdAgent{});
            world.set(e, Team{static_cast<uint8_t>(team)});
            world.set(e, Position{base_x, static_cast<float>(i) * cfg.agent_spread});
            world.set(e, Velocity{0.0f, 0.0f});
            world.set(e, MoveSpeed{cfg.move_speed});
            world.set(e, Target{});
            world.set(e, DesiredDirection{});
            world.set(e, Health{cfg.health, cfg.health});
            world.set(e, AttackRange{cfg.attack_range});
            world.set(e, AttackDamage{cfg.attack_damage});
            world.set(e, AttackCooldown{0.0f, cfg.attack_interval});
            world.set(e, Separation{cfg.separation_radius, cfg.separation_strength});
            // Goal: advance toward the opposing team's spawn.
            float goal_x = (team == 0) ? team1_x : team0_x;
            world.set(e, BattleGoal{goal_x, 0.0f});
            world.set(e, EngageRadius{cfg.engage_radius});
            world.set(e, BehaviorLod{});
        }
    }

    update_crowd_stats();
}

void SimState::bootstrap_battlefield(const BattlefieldConfig& cfg) {
    // Validate grid configuration -- fail fast on nonsensical scenes.
    // Zero-area grids or non-positive cell sizes cannot produce a valid
    // flow field and indicate a configuration bug.
    if (cfg.grid_width <= 0 || cfg.grid_height <= 0 || cfg.grid_cell <= 0.0f) {
        // Fall back to plain crowd bootstrap without navigation.
        bootstrap_crowd(cfg.crowd);
        return;
    }

    // Reuse crowd bootstrap for agents (clears nav state too).
    bootstrap_crowd(cfg.crowd);

    // Sanitize obstacle pointer: ignore obstacle_count if pointer is null.
    const int safe_obstacle_count =
        (cfg.obstacles != nullptr) ? cfg.obstacle_count : 0;

    // Init per-team grids with identical geometry and obstacles.
    for (uint32_t t = 0; t < k_max_teams; ++t) {
        nav_grids_[t].init(cfg.grid_width, cfg.grid_height,
                           cfg.grid_cell, cfg.grid_ox, cfg.grid_oy);
        for (int i = 0; i < safe_obstacle_count; ++i) {
            nav_grids_[t].set_blocked(cfg.obstacles[i].cx, cfg.obstacles[i].cy);
        }
        nav_grid_ptrs_[t] = &nav_grids_[t];
    }
    nav_grid_active_   = true;
    nav_blocked_cells_ = nav_grids_[0].blocked_count();

    // Record per-team goals (team 0 goal = +spacing, team 1 goal = -spacing).
    nav_goals_x_[0] =  cfg.crowd.team_spacing;
    nav_goals_y_[0] =  0.0f;
    nav_goals_x_[1] = -cfg.crowd.team_spacing;
    nav_goals_y_[1] =  0.0f;
    for (uint32_t t = 2; t < k_max_teams; ++t) {
        nav_goals_x_[t] = 0.0f;
        nav_goals_y_[t] = 0.0f;
    }

    // Build initial flow fields and install pointers.
    build_nav_fields();
    set_battlefield_grids(nav_grid_ptrs_, k_max_teams);
}

void SimState::set_lod_center(float cx, float cy) {
    lod_config_.center_x = cx;
    lod_config_.center_y = cy;
}

void SimState::build_nav_fields() {
    if (!nav_grid_active_) return;
    for (uint32_t t = 0; t < k_max_teams; ++t) {
        nav_grids_[t].build_integration_field(nav_goals_x_[t], nav_goals_y_[t]);
    }
}

void SimState::tick(double step_dt) {
    // Wall-clock timer covers all simulation work (cull, systems, apply,
    // stats, hash).  evaluate_budget() runs outside the measured region.
    auto tick_start = std::chrono::high_resolution_clock::now();

    cull_pre_dead();

    float dt = static_cast<float>(step_dt);
    cmds_.clear();
    reset_crowd_tick_counters();
    set_crowd_tick_count(tick_count_);

    // Apply budget response from previous tick (decision at tick N-1
    // applied at tick N).  Adjusts LOD thresholds before systems run.
    if (budget_response_config_.enabled && budget_response_state_.active) {
        lod_config_.t1_distance = lod_base_t1_ * budget_response_state_.lod_distance_scale;
        lod_config_.t2_distance = lod_base_t2_ * budget_response_state_.lod_distance_scale;
        lod_config_.t3_distance = lod_base_t3_ * budget_response_state_.lod_distance_scale;
    } else {
        lod_config_.t1_distance = lod_base_t1_;
        lod_config_.t2_distance = lod_base_t2_;
        lod_config_.t3_distance = lod_base_t3_;
    }

    set_behavior_lod_config(&lod_config_);

    WorldView view(world);

    for (uint32_t i = 0; i < system_count_; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        uint32_t n = pipeline_[i].fn(view, dt, cmds_);
        auto t1 = std::chrono::high_resolution_clock::now();

        std::memcpy(last_stats_[i].name, pipeline_[i].name, k_system_name_max);
        last_stats_[i].elapsed_s =
            std::chrono::duration<double>(t1 - t0).count();
        last_stats_[i].entities_processed = n;
    }

    attacks_this_tick_              = crowd_attacks_this_tick();
    deaths_this_tick_               = crowd_deaths_queued_this_tick();
    targeting_candidates_scanned_   = crowd_candidates_scanned_this_tick();
    separation_pairs_this_tick_     = crowd_separation_pairs_this_tick();
    agents_engaged_                 = crowd_agents_engaged_this_tick();
    melee_bp_checks_                = melee_broadphase_checks_this_tick();
    melee_pairs_this_tick_          = melee_pairs_this_tick();
    melee_attacks_this_tick_        = melee_attacks_this_tick();
    nav_queries_this_tick_          = crowd_nav_queries_this_tick();
    nav_failures_this_tick_         = crowd_nav_failures_this_tick();
    cmds_queued_last_ = cmds_.pending();
    cmds_.apply(world);
    cmds_applied_last_ = cmds_.last_applied_count();

    update_crowd_stats();

    ++tick_count_;

    // Compute deterministic simulation hash on post-apply world state.
    // Uses tick_count_ (post-increment) so that snapshot().tick_count and
    // snapshot().sim_hash always refer to the same completed tick.
    uint64_t hash = compute_sim_hash(world, tick_count_);
    hash_history_.push(hash);

    auto tick_end = std::chrono::high_resolution_clock::now();
    double tick_wall_s = std::chrono::duration<double>(tick_end - tick_start).count();
    evaluate_budget(tick_wall_s);
    apply_budget_response();
}

void SimState::shutdown() {
    world = World{};
    tick_count_        = 0;
    system_count_      = 0;
    cmds_queued_last_  = 0;
    cmds_applied_last_ = 0;
    crowd_agent_count_  = 0;
    agents_with_target_ = 0;
    attacks_this_tick_  = 0;
    deaths_this_tick_   = 0;
    targeting_candidates_scanned_ = 0;
    separation_pairs_this_tick_  = 0;
    agents_engaged_              = 0;
    nav_queries_this_tick_       = 0;
    nav_failures_this_tick_      = 0;
    nav_blocked_cells_           = 0;
    nav_grid_active_             = false;
    set_battlefield_grids(nullptr, 0);
    for (auto& c : lod_tier_counts_) c = 0;
    lod_skipped_this_tick_       = 0;
    melee_bp_checks_             = 0;
    melee_pairs_this_tick_       = 0;
    melee_attacks_this_tick_     = 0;
    lod_config_                  = BehaviorLodConfig{};
    lod_base_t1_ = lod_config_.t1_distance;
    lod_base_t2_ = lod_config_.t2_distance;
    lod_base_t3_ = lod_config_.t3_distance;
    hash_history_.clear();
    // budget_config_ intentionally preserved across shutdown.
    budget_status_ = SimBudgetStatus{};
    // budget_response_config_ intentionally preserved across shutdown.
    budget_response_state_ = SimBudgetResponseState{};
    for (auto& c : team_counts_) c = 0;
    cmds_.clear();
    for (auto& s : last_stats_) s = {};
}

SimSnapshot SimState::snapshot() const {
    SimSnapshot snap;
    snap.entity_count  = world.entity_count();
    snap.tick_count    = tick_count_;
    snap.system_count  = system_count_;
    snap.cmds_queued   = cmds_queued_last_;
    snap.cmds_applied  = cmds_applied_last_;
    for (uint32_t i = 0; i < system_count_; ++i) {
        snap.systems[i] = last_stats_[i];
    }
    snap.sim_hash           = hash_history_.latest;
    snap.crowd_agent_count  = crowd_agent_count_;
    snap.agents_with_target = agents_with_target_;
    for (uint32_t i = 0; i < k_max_teams; ++i) {
        snap.team_counts[i] = team_counts_[i];
    }
    snap.attacks_this_tick              = attacks_this_tick_;
    snap.deaths_this_tick               = deaths_this_tick_;
    snap.targeting_candidates_scanned   = targeting_candidates_scanned_;
    snap.separation_pairs_this_tick     = separation_pairs_this_tick_;
    snap.agents_engaged                 = agents_engaged_;
    snap.nav_queries_this_tick          = nav_queries_this_tick_;
    snap.nav_failures_this_tick         = nav_failures_this_tick_;
    snap.nav_blocked_cells              = nav_blocked_cells_;
    for (int t = 0; t < 4; ++t) snap.lod_tier_counts[t] = lod_tier_counts_[t];
    snap.lod_skipped_this_tick          = lod_skipped_this_tick_;
    snap.melee_broadphase_checks        = melee_bp_checks_;
    snap.melee_pairs_this_tick          = melee_pairs_this_tick_;
    snap.melee_attacks_this_tick        = melee_attacks_this_tick_;
    snap.budget                         = budget_status_;
    snap.budget_pressure_level          = budget_response_state_.pressure_level;
    snap.budget_response_active         = budget_response_state_.active;
    snap.budget_lod_scale               = budget_response_state_.lod_distance_scale;
    return snap;
}

uint32_t SimState::system_count() const {
    return system_count_;
}

uint64_t SimState::sim_hash() const {
    return hash_history_.latest;
}

const SimHashHistory& SimState::hash_history() const {
    return hash_history_;
}

void SimState::register_systems() {
    add_system("IntegrateVelocity", integrate_velocity);
    add_system("IntegratePosition", integrate_position);
}

void SimState::register_crowd_systems() {
    add_system("SelectTargets",      select_targets);
    add_system("ClassifyLod",        classify_behavior_lod);
    add_system("ComputeBattleGoal", compute_battle_goal);
    add_system("ComputeDesiredMove", compute_desired_movement);
    add_system("ApplyCrowdSteer",    apply_crowd_steering);
    add_system("ApplySeparation",   apply_separation);
    add_system("MeleeBroadphase",   gather_melee_candidates);
    add_system("AttackTargets",      attack_targets);
    add_system("ResolveDamage",      resolve_damage);
    add_system("RemoveDead",         remove_dead);
    add_system("IntegrateVelocity",  integrate_velocity);
    add_system("IntegratePosition",  integrate_position);
}

void SimState::update_crowd_stats() {
    crowd_agent_count_  = 0;
    agents_with_target_ = 0;
    for (auto& c : team_counts_) c = 0;
    for (auto& c : lod_tier_counts_) c = 0;
    lod_skipped_this_tick_ = 0;

    world.each<CrowdAgent, Team, Target>(
        [&](EntityId, CrowdAgent&, Team& team, Target& tgt) {
            ++crowd_agent_count_;
            if (team.id < k_max_teams) ++team_counts_[team.id];
            if (tgt.has_target && world.alive(tgt.entity))
                ++agents_with_target_;
        });

    // LOD metrics: recount on the living world (post-apply) so the
    // snapshot is coherent with crowd_agent_count_ and team_counts_.
    world.each<CrowdAgent, BehaviorLod>(
        [&](EntityId, CrowdAgent&, BehaviorLod& lod) {
            if (lod.tier < k_lod_tier_count)
                ++lod_tier_counts_[lod.tier];
            if (lod.stride > 1 && (tick_count_ % lod.stride) != 0)
                ++lod_skipped_this_tick_;
        });
}

void SimState::cull_pre_dead() {
    // Destroy agents that entered this tick with HP <= 0.
    // Runs before any system so dead agents never act.
    // Uses direct World::destroy (not CommandBuffer) because the
    // CommandBuffer is only applied at end of tick.
    std::vector<EntityId> dead;
    world.each<CrowdAgent, Health>([&](EntityId id, CrowdAgent&, Health& hp) {
        if (hp.current <= 0.0f) dead.push_back(id);
    });
    for (auto id : dead) world.destroy(id);
}

void SimState::set_budget_config(const SimBudgetConfig& cfg) {
    budget_config_ = cfg;
}

const SimBudgetStatus& SimState::budget_status() const {
    return budget_status_;
}

void SimState::set_budget_response_config(const SimBudgetResponseConfig& cfg) {
    budget_response_config_ = cfg;
}

const SimBudgetResponseState& SimState::budget_response_state() const {
    return budget_response_state_;
}

void SimState::apply_budget_response() {
    if (!budget_response_config_.enabled) return;
    if (budget_response_config_.max_pressure == 0) return;

    auto& st = budget_response_state_;

    if (!budget_status_.within_budget) {
        // Over budget: increase pressure immediately (capped).
        st.consecutive_healthy = 0;
        if (st.pressure_level < budget_response_config_.max_pressure) {
            ++st.pressure_level;
        }
    } else {
        // Within budget: count consecutive healthy ticks.
        ++st.consecutive_healthy;
        if (st.pressure_level > 0 &&
            st.consecutive_healthy >= budget_response_config_.recovery_ticks) {
            --st.pressure_level;
            st.consecutive_healthy = 0;
        }
    }

    // Compute LOD distance scale from pressure level.
    st.lod_distance_scale = 1.0f -
        static_cast<float>(st.pressure_level) *
        budget_response_config_.shrink_per_level;
    // Clamp to a small positive floor to avoid zero/negative thresholds.
    if (st.lod_distance_scale < 0.05f) st.lod_distance_scale = 0.05f;

    st.active = (st.pressure_level > 0);
}

void SimState::evaluate_budget(double tick_wall_s) {
    budget_status_ = SimBudgetStatus{};

    // tick_elapsed_s = full tick wall-clock (cull + systems + apply +
    // stats + hash).  Hottest system uses per-system timings.
    budget_status_.tick_elapsed_s = tick_wall_s;

    double hottest_s = 0.0;
    uint32_t hottest_idx = 0;
    for (uint32_t i = 0; i < system_count_; ++i) {
        if (last_stats_[i].elapsed_s > hottest_s) {
            hottest_s   = last_stats_[i].elapsed_s;
            hottest_idx = i;
        }
    }

    budget_status_.hottest_system_s = hottest_s;
    if (system_count_ > 0)
        std::memcpy(budget_status_.hottest_system,
                    last_stats_[hottest_idx].name, k_system_name_max);

    budget_status_.targeting_scanned = targeting_candidates_scanned_;
    budget_status_.melee_checks      = melee_bp_checks_;
    budget_status_.lod_t0_count      = lod_tier_counts_[0];

    // Evaluate contracts (tick_wall_s = full tick, not just systems).
    if (tick_wall_s > budget_config_.max_tick_s) {
        budget_status_.tick_over = true;
        ++budget_status_.violation_count;
    }
    if (hottest_s > budget_config_.max_system_s) {
        budget_status_.system_over = true;
        ++budget_status_.violation_count;
    }
    if (targeting_candidates_scanned_ > budget_config_.max_targeting_scanned) {
        budget_status_.targeting_over = true;
        ++budget_status_.violation_count;
    }
    if (melee_bp_checks_ > budget_config_.max_melee_checks) {
        budget_status_.melee_over = true;
        ++budget_status_.violation_count;
    }
    if (lod_tier_counts_[0] > budget_config_.max_lod_t0_count) {
        budget_status_.lod_t0_over = true;
        ++budget_status_.violation_count;
    }

    budget_status_.within_budget = (budget_status_.violation_count == 0);
}

void SimState::add_system(const char* name, FixedSystemFn fn) {
    if (system_count_ >= k_max_sim_systems) return;
    auto& sys = pipeline_[system_count_];
    std::snprintf(sys.name, k_system_name_max, "%s", name);
    sys.fn = fn;
    ++system_count_;
}

}  // namespace de
