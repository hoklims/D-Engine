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
    nav_blocked_cells_           = 0;
    nav_grid_active_             = false;
    set_battlefield_grids(nullptr, 0);
    for (auto& c : team_counts_) c = 0;
    cmds_.clear();
    for (auto& s : last_stats_) s = {};

    register_crowd_systems();

    for (int team = 0; team < 2; ++team) {
        float base_x = (team == 0) ? -cfg.team_spacing : cfg.team_spacing;
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
            float goal_x = (team == 0) ? cfg.team_spacing : -cfg.team_spacing;
            world.set(e, BattleGoal{goal_x, 0.0f});
            world.set(e, EngageRadius{cfg.engage_radius});
        }
    }

    update_crowd_stats();
}

void SimState::bootstrap_battlefield(const BattlefieldConfig& cfg) {
    // Reuse crowd bootstrap for agents (clears nav state too).
    bootstrap_crowd(cfg.crowd);

    // Init per-team grids with identical geometry and obstacles.
    for (uint32_t t = 0; t < k_max_teams; ++t) {
        nav_grids_[t].init(cfg.grid_width, cfg.grid_height,
                           cfg.grid_cell, cfg.grid_ox, cfg.grid_oy);
        for (int i = 0; i < cfg.obstacle_count; ++i) {
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

void SimState::build_nav_fields() {
    if (!nav_grid_active_) return;
    for (uint32_t t = 0; t < k_max_teams; ++t) {
        nav_grids_[t].build_integration_field(nav_goals_x_[t], nav_goals_y_[t]);
    }
}

void SimState::tick(double step_dt) {
    cull_pre_dead();

    float dt = static_cast<float>(step_dt);
    cmds_.clear();
    reset_crowd_tick_counters();

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
    nav_queries_this_tick_          = crowd_nav_queries_this_tick();

    cmds_queued_last_ = cmds_.pending();
    cmds_.apply(world);
    cmds_applied_last_ = cmds_.last_applied_count();

    update_crowd_stats();

    ++tick_count_;
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
    nav_blocked_cells_           = 0;
    nav_grid_active_             = false;
    set_battlefield_grids(nullptr, 0);
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
    snap.nav_blocked_cells              = nav_blocked_cells_;
    return snap;
}

uint32_t SimState::system_count() const {
    return system_count_;
}

void SimState::register_systems() {
    add_system("IntegrateVelocity", integrate_velocity);
    add_system("IntegratePosition", integrate_position);
}

void SimState::register_crowd_systems() {
    add_system("SelectTargets",      select_targets);
    add_system("ComputeBattleGoal", compute_battle_goal);
    add_system("ComputeDesiredMove", compute_desired_movement);
    add_system("ApplyCrowdSteer",    apply_crowd_steering);
    add_system("ApplySeparation",   apply_separation);
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

    world.each<CrowdAgent, Team, Target>(
        [&](EntityId, CrowdAgent&, Team& team, Target& tgt) {
            ++crowd_agent_count_;
            if (team.id < k_max_teams) ++team_counts_[team.id];
            if (tgt.has_target && world.alive(tgt.entity))
                ++agents_with_target_;
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

void SimState::add_system(const char* name, FixedSystemFn fn) {
    if (system_count_ >= k_max_sim_systems) return;
    auto& sys = pipeline_[system_count_];
    std::snprintf(sys.name, k_system_name_max, "%s", name);
    sys.fn = fn;
    ++system_count_;
}

}  // namespace de
