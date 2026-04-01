#include "Runtime/CrowdSystems.h"
#include "Runtime/Components.h"
#include "Runtime/CrowdComponents.h"
#include "Runtime/SpatialGrid.h"

#include <cfloat>
#include <cmath>
#include <unordered_set>
#include <vector>

namespace de {

// -- Per-tick combat state (file scope) --------------------------------------

struct HitEvent {
    EntityId target;
    float    damage;
};

// Broadphase output: attacker->defender pairs validated by spatial query.
struct MeleePair {
    EntityId attacker;
    EntityId defender;
};

static std::vector<HitEvent>  s_hit_buffer;
static std::vector<MeleePair> s_melee_pairs;
static SpatialGrid s_grid;
static const BattlefieldGrid* const* s_nav_grids = nullptr;
static uint32_t s_nav_grid_count         = 0;
static uint32_t s_attacks_this_tick      = 0;
static uint32_t s_deaths_this_tick       = 0;
static uint32_t s_candidates_scanned     = 0;
static uint32_t s_separation_pairs       = 0;
static uint32_t s_agents_engaged         = 0;
static uint32_t s_nav_queries            = 0;
static uint32_t s_nav_failures           = 0;
static uint32_t s_lod_counts[k_lod_tier_count] = {};
static uint32_t s_lod_skipped            = 0;
static const BehaviorLodConfig* s_lod_cfg = nullptr;
static BehaviorLodConfig s_lod_default;
static uint64_t s_crowd_tick             = 0;

// Melee broadphase telemetry.
// Local avoidance telemetry.
static uint32_t s_avoidance_neighbors    = 0;
static uint32_t s_avoidance_adjusted     = 0;

static uint32_t s_melee_bp_checks        = 0;
static uint32_t s_melee_pairs_count      = 0;
static uint32_t s_melee_attacks          = 0;

// Broadphase validation set: O(1) lookup for (attacker, defender) pairs.
static std::unordered_set<uint64_t> s_melee_valid_set;

static uint64_t make_pair_key(uint32_t a_idx, uint32_t d_idx) {
    return (static_cast<uint64_t>(a_idx) << 32) | static_cast<uint32_t>(d_idx);
}

uint32_t crowd_attacks_this_tick()            { return s_attacks_this_tick; }
uint32_t crowd_deaths_queued_this_tick()      { return s_deaths_this_tick; }
uint32_t crowd_candidates_scanned_this_tick() { return s_candidates_scanned; }
uint32_t crowd_separation_pairs_this_tick()   { return s_separation_pairs; }
uint32_t crowd_agents_engaged_this_tick()     { return s_agents_engaged; }
uint32_t crowd_nav_queries_this_tick()        { return s_nav_queries; }
uint32_t crowd_nav_failures_this_tick()       { return s_nav_failures; }
uint32_t crowd_lod_tier_count(uint8_t tier)   { return (tier < k_lod_tier_count) ? s_lod_counts[tier] : 0; }
uint32_t crowd_lod_skipped_this_tick()        { return s_lod_skipped; }

uint32_t avoidance_neighbors_this_tick()      { return s_avoidance_neighbors; }
uint32_t avoidance_adjusted_this_tick()       { return s_avoidance_adjusted; }

uint32_t melee_broadphase_checks_this_tick()  { return s_melee_bp_checks; }
uint32_t melee_pairs_this_tick()              { return s_melee_pairs_count; }
uint32_t melee_attacks_this_tick()            { return s_melee_attacks; }

void set_behavior_lod_config(const BehaviorLodConfig* cfg) { s_lod_cfg = cfg; }
void set_crowd_tick_count(uint64_t tick) { s_crowd_tick = tick; }

void set_battlefield_grids(const BattlefieldGrid* const* grids, uint32_t count) {
    s_nav_grids      = grids;
    s_nav_grid_count = count;
}
uint32_t get_battlefield_grid_count() { return s_nav_grid_count; }

void     reset_crowd_tick_counters() {
    s_attacks_this_tick  = 0;
    s_deaths_this_tick   = 0;
    s_candidates_scanned = 0;
    s_separation_pairs   = 0;
    s_agents_engaged     = 0;
    s_nav_queries        = 0;
    s_nav_failures       = 0;
    for (auto& c : s_lod_counts) c = 0;
    s_lod_skipped        = 0;
    s_avoidance_neighbors = 0;
    s_avoidance_adjusted  = 0;
    s_melee_bp_checks    = 0;
    s_melee_pairs_count  = 0;
    s_melee_attacks      = 0;
    s_hit_buffer.clear();
    s_melee_pairs.clear();
    s_melee_valid_set.clear();
}

// -----------------------------------------------------------------------
//  LOD helpers
// -----------------------------------------------------------------------

// Returns true if this agent should be SKIPPED by a gated system
// on the current tick.
static bool lod_should_skip(const BehaviorLod& lod) {
    if (lod.stride <= 1) return false;
    return (s_crowd_tick % lod.stride) != 0;
}

// -----------------------------------------------------------------------
//  classify_behavior_lod
// -----------------------------------------------------------------------
// Assign a LOD tier to each crowd agent based on engagement state and
// distance to the explicit battle center (cfg.center_x/y).
// Engaged agents are always T0.  Must run BEFORE any gated system.

uint32_t classify_behavior_lod(WorldView& view, float /*dt*/,
                               CommandBuffer& /*cmds*/) {
    const BehaviorLodConfig& cfg = s_lod_cfg ? *s_lod_cfg : s_lod_default;
    for (auto& c : s_lod_counts) c = 0;

    uint32_t count = 0;
    view.each<CrowdAgent, Position, Target, EngageRadius, BehaviorLod>(
        [&](EntityId, CrowdAgent&, Position& pos,
            Target& tgt, EngageRadius& engage, BehaviorLod& lod) {
            ++count;

            // Engaged agents are always T0 (full fidelity).
            bool is_engaged = false;
            if (tgt.has_target && view.alive(tgt.entity)) {
                const auto* tp = view.get<Position>(tgt.entity);
                if (tp) {
                    float dx = tp->x - pos.x;
                    float dy = tp->y - pos.y;
                    float d2 = dx * dx + dy * dy;
                    if (d2 <= engage.radius * engage.radius) {
                        is_engaged = true;
                    }
                }
            }

            if (is_engaged) {
                lod.tier   = 0;
                lod.stride = k_lod_strides[0];
            } else {
                // Distance to explicit battle center.
                float cx = pos.x - cfg.center_x;
                float cy = pos.y - cfg.center_y;
                float dist = std::sqrt(cx * cx + cy * cy);
                if (dist < cfg.t1_distance) {
                    lod.tier   = 0;
                    lod.stride = k_lod_strides[0];
                } else if (dist < cfg.t2_distance) {
                    lod.tier   = 1;
                    lod.stride = k_lod_strides[1];
                } else if (dist < cfg.t3_distance) {
                    lod.tier   = 2;
                    lod.stride = k_lod_strides[2];
                } else {
                    lod.tier   = 3;
                    lod.stride = k_lod_strides[3];
                }
            }
            ++s_lod_counts[lod.tier];
            if (lod_should_skip(lod)) ++s_lod_skipped;
        });
    return count;
}

// -----------------------------------------------------------------------
//  select_targets
// -----------------------------------------------------------------------
// For each crowd agent, find the nearest living enemy (different team).
// Keeps the current target if it is still alive and still an enemy.

uint32_t select_targets(WorldView& view, float /*dt*/, CommandBuffer& /*cmds*/) {
    // Rebuild spatial grid from current positions.
    s_grid.clear();
    s_candidates_scanned = 0;
    view.each<CrowdAgent, Team, Position>(
        [](EntityId id, CrowdAgent&, Team& t, Position& p) {
            s_grid.insert(id, t.id, p.x, p.y);
        });

    uint32_t count = 0;
    view.each<CrowdAgent, Team, Position, Target>(
        [&](EntityId self, CrowdAgent&, Team& my_team,
            Position& my_pos, Target& tgt) {
            ++count;

            // Keep current target if still valid.
            if (tgt.has_target && view.alive(tgt.entity)) {
                const auto* et = view.get<Team>(tgt.entity);
                if (et && et->id != my_team.id) return;
            }

            // Find nearest enemy via spatial grid.
            uint32_t checked = 0;
            auto result = s_grid.find_nearest_enemy(
                my_pos.x, my_pos.y, my_team.id, self, checked);
            s_candidates_scanned += checked;
            tgt.entity     = result.id;
            tgt.has_target = result.found;
        });
    return count;
}

// -----------------------------------------------------------------------
//  compute_battle_goal
// -----------------------------------------------------------------------
// Strategic layer: set desired direction toward the team's rally point.
// Runs before compute_desired_movement so pursuit can override.

uint32_t compute_battle_goal(WorldView& view, float /*dt*/,
                             CommandBuffer& /*cmds*/) {
    uint32_t count = 0;
    view.each<CrowdAgent, Team, Position, BattleGoal, DesiredDirection, BehaviorLod>(
        [&](EntityId, CrowdAgent&, Team& team, Position& pos,
            BattleGoal& goal, DesiredDirection& dir, BehaviorLod& lod) {
            // LOD gating: skip non-T0 agents on off-ticks.
            // Their DesiredDirection from the last update is preserved.
            if (lod_should_skip(lod)) { return; }
            ++count;

            // When navigation grids are installed, the flow field is
            // authoritative.  If sample_flow fails (out of grid, blocked,
            // unreachable), the agent gets a zero direction -- it does NOT
            // fall back to direct line, which would silently bypass obstacles.
            if (s_nav_grids && team.id < s_nav_grid_count) {
                const BattlefieldGrid* grid = s_nav_grids[team.id];
                if (grid) {
                    float fx = 0.0f;
                    float fy = 0.0f;
                    ++s_nav_queries;
                    if (grid->sample_flow(pos.x, pos.y, fx, fy)) {
                        float len = std::sqrt(fx * fx + fy * fy);
                        if (len > 1e-6f) {
                            dir.dx = fx / len;
                            dir.dy = fy / len;
                            return;
                        }
                    }
                    // Fail-safe: zero direction (hold position).
                    ++s_nav_failures;
                    dir.dx = 0.0f;
                    dir.dy = 0.0f;
                    return;
                }
            }

            // No grid installed -- direct line toward goal.
            float dx  = goal.x - pos.x;
            float dy  = goal.y - pos.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len > 1e-6f) {
                dir.dx = dx / len;
                dir.dy = dy / len;
            } else {
                dir.dx = 0.0f;
                dir.dy = 0.0f;
            }
        });
    return count;
}

// -----------------------------------------------------------------------
//  compute_desired_movement
// -----------------------------------------------------------------------
// Tactical override: if the nearest enemy is within EngageRadius, switch
// from battle-goal direction to local pursuit.  If in attack range, stop.
// If no enemy is close enough, the battle-goal direction is preserved.

uint32_t compute_desired_movement(WorldView& view, float /*dt*/,
                                  CommandBuffer& /*cmds*/) {
    s_agents_engaged = 0;
    uint32_t count = 0;
    view.each<CrowdAgent, Position, Target, DesiredDirection,
              AttackRange, EngageRadius>(
        [&](EntityId, CrowdAgent&, Position& pos,
            Target& tgt, DesiredDirection& dir,
            AttackRange& atk_range, EngageRadius& engage) {
            ++count;

            if (!tgt.has_target || !view.alive(tgt.entity)) return;

            const auto* tp = view.get<Position>(tgt.entity);
            if (!tp) return;

            float dx  = tp->x - pos.x;
            float dy  = tp->y - pos.y;
            float len = std::sqrt(dx * dx + dy * dy);

            // Outside engage radius -- keep battle-goal direction.
            if (len > engage.radius) return;

            ++s_agents_engaged;

            // In attack range -- stop moving, fight instead.
            if (len <= atk_range.range) {
                dir.dx = 0.0f;
                dir.dy = 0.0f;
                return;
            }

            // Within engage range -- pursue target.
            if (len > 1e-6f) {
                dir.dx = dx / len;
                dir.dy = dy / len;
            }
        });
    return count;
}

// -----------------------------------------------------------------------
//  apply_crowd_steering
// -----------------------------------------------------------------------
// Instant steering: velocity = direction * max_speed.
// No smoothing, no inertia -- deliberately simple.

uint32_t apply_crowd_steering(WorldView& view, float /*dt*/,
                              CommandBuffer& /*cmds*/) {
    uint32_t count = 0;
    view.each<CrowdAgent, Velocity, DesiredDirection, MoveSpeed>(
        [&](EntityId, CrowdAgent&, Velocity& vel,
            DesiredDirection& dir, MoveSpeed& spd) {
            ++count;
            vel.dx = dir.dx * spd.max;
            vel.dy = dir.dy * spd.max;
        });
    return count;
}

// -----------------------------------------------------------------------
//  apply_local_avoidance
// -----------------------------------------------------------------------
// Anticipatory collision avoidance using time-to-closest-approach (TTC).
// For each agent, scan nearby agents (within LocalAvoidance.radius).
// If two agents are closing (relative velocity converging), compute
// TTC.  If TTC is within the prediction horizon, apply a lateral
// dodge force perpendicular to the approach direction.
//
// Side selection is deterministic: lower EntityId dodges left (perp
// = (-dy, +dx)), higher dodges right (perp = (+dy, -dx)).  This
// guarantees symmetric avoidance -- both agents dodge in compatible
// directions.
//
// Runs after ApplyCrowdSteer (velocity set) and before ApplySeparation
// (positional overlap repair).  LOD gated.

uint32_t apply_local_avoidance(WorldView& view, float /*dt*/,
                               CommandBuffer& /*cmds*/) {
    s_avoidance_neighbors = 0;
    s_avoidance_adjusted  = 0;
    uint32_t count = 0;

    view.each<CrowdAgent, Position, Velocity, LocalAvoidance, MoveSpeed, BehaviorLod>(
        [&](EntityId self, CrowdAgent&, Position& pos,
            Velocity& vel, LocalAvoidance& avoid, MoveSpeed& spd,
            BehaviorLod& lod) {
            if (lod_should_skip(lod)) { return; }
            ++count;

            float steer_x = 0.0f;
            float steer_y = 0.0f;
            bool  adjusted = false;

            s_grid.for_each_nearby(
                pos.x, pos.y, avoid.radius, self,
                [&](const SpatialGrid::Entry& e, float d2) {
                    ++s_avoidance_neighbors;

                    float dist = std::sqrt(d2);
                    if (dist < 1e-6f) return;  // exact overlap handled by separation

                    // Relative position and velocity.
                    float rel_px = e.x - pos.x;
                    float rel_py = e.y - pos.y;

                    // We need neighbor velocity.  Look it up.
                    const auto* nv = view.get<Velocity>(e.id);
                    if (!nv) return;

                    float rel_vx = vel.dx - nv->dx;
                    float rel_vy = vel.dy - nv->dy;

                    // Closing speed along the approach axis.
                    // Positive means converging.
                    float closing = (rel_px * rel_vx + rel_py * rel_vy) / dist;
                    if (closing <= 0.0f) return;  // diverging, no avoidance needed

                    // Time to closest approach (simplified linear prediction).
                    float ttc = dist / closing;
                    if (ttc > avoid.horizon) return;  // too far in the future

                    // Urgency ramps linearly from 0 (at horizon) to 1 (at contact).
                    float urgency = 1.0f - ttc / avoid.horizon;

                    // Lateral dodge direction: perpendicular to approach axis.
                    // Deterministic side: lower EntityId goes left, higher goes right.
                    float ax = rel_px / dist;
                    float ay = rel_py / dist;
                    float perp_x, perp_y;
                    if (self.index < e.id.index) {
                        perp_x = -ay;
                        perp_y =  ax;
                    } else {
                        perp_x =  ay;
                        perp_y = -ax;
                    }

                    steer_x += perp_x * urgency * avoid.strength;
                    steer_y += perp_y * urgency * avoid.strength;
                    adjusted = true;
                });

            if (adjusted) {
                vel.dx += steer_x;
                vel.dy += steer_y;

                // Reclamp to max speed.
                float speed2 = vel.dx * vel.dx + vel.dy * vel.dy;
                float max2   = spd.max * spd.max;
                if (speed2 > max2) {
                    float scale = spd.max / std::sqrt(speed2);
                    vel.dx *= scale;
                    vel.dy *= scale;
                }
                ++s_avoidance_adjusted;
            }
        });
    return count;
}

// -----------------------------------------------------------------------
//  gather_melee_candidates  (broadphase)
// -----------------------------------------------------------------------
// For each crowd agent, query the spatial grid for enemies within
// AttackRange.  Builds a deduplicated buffer of (attacker, defender)
// pairs.  This is the melee broadphase -- it bounds the work that
// attack_targets has to do by pre-filtering spatially.
//
// Uses the grid already built by select_targets (same tick).

uint32_t gather_melee_candidates(WorldView& view, float /*dt*/,
                                 CommandBuffer& /*cmds*/) {
    s_melee_pairs.clear();
    s_melee_valid_set.clear();
    s_melee_bp_checks   = 0;
    s_melee_pairs_count = 0;

    uint32_t count = 0;
    view.each<CrowdAgent, Team, Position, AttackRange>(
        [&](EntityId self, CrowdAgent&, Team& my_team,
            Position& pos, AttackRange& range) {
            ++count;

            s_melee_bp_checks += s_grid.for_each_nearby(
                pos.x, pos.y, range.range, self,
                [&](const SpatialGrid::Entry& e, float /*d2*/) {
                    if (e.team == my_team.id) return;  // ally, skip
                    s_melee_pairs.push_back({self, e.id});
                    s_melee_valid_set.insert(
                        make_pair_key(self.index, e.id.index));
                });
        });

    s_melee_pairs_count = static_cast<uint32_t>(s_melee_pairs.size());
    return count;
}

// -----------------------------------------------------------------------
//  attack_targets
// -----------------------------------------------------------------------
// Single pass over agents (not pairs).  For each agent:
//   1) Tick cooldown (unconditional, deterministic).
//   2) If cooldown ready AND Target.entity is in the broadphase set,
//      emit exactly one HitEvent toward Target.entity.
//
// Guarantees:
//   - At most one hit per attacker per tick (iterate agents, not pairs).
//   - Attack target is always Target.entity (set by select_targets),
//     never an arbitrary broadphase neighbor.
//   - interval <= 0 cannot cause multi-hit (one iteration per agent).
//   - Does NOT modify Health -- that is resolve_damage's job.

uint32_t attack_targets(WorldView& view, float dt, CommandBuffer& /*cmds*/) {
    s_attacks_this_tick = 0;
    s_melee_attacks     = 0;
    s_hit_buffer.clear();

    uint32_t count = 0;
    view.each<CrowdAgent, Target, AttackCooldown, AttackDamage>(
        [&](EntityId self, CrowdAgent&, Target& tgt,
            AttackCooldown& cd, AttackDamage& dmg) {
            ++count;
            cd.remaining -= dt;

            if (!tgt.has_target || !view.alive(tgt.entity)) return;
            if (cd.remaining > 0.0f) return;

            // Broadphase gate: Target.entity must be confirmed in melee
            // range by gather_melee_candidates.
            auto key = make_pair_key(self.index, tgt.entity.index);
            if (s_melee_valid_set.find(key) == s_melee_valid_set.end())
                return;

            s_hit_buffer.push_back({tgt.entity, dmg.damage});
            cd.remaining = cd.interval;
            ++s_attacks_this_tick;
            ++s_melee_attacks;
        });
    return count;
}

// -----------------------------------------------------------------------
//  resolve_damage
// -----------------------------------------------------------------------
// Consume all hit events and apply damage to Health components.
// Runs after attack_targets so all attacks are collected before any
// HP is modified -- guarantees simultaneous resolution.

uint32_t resolve_damage(WorldView& view, float /*dt*/, CommandBuffer& /*cmds*/) {
    uint32_t count = 0;
    for (const auto& hit : s_hit_buffer) {
        auto* hp = view.get<Health>(hit.target);
        if (hp) {
            hp->current -= hit.damage;
            ++count;
        }
    }
    return count;
}

// -----------------------------------------------------------------------
//  remove_dead
// -----------------------------------------------------------------------
// Queue deferred destruction for agents whose health dropped to zero.

uint32_t remove_dead(WorldView& view, float /*dt*/, CommandBuffer& cmds) {
    s_deaths_this_tick = 0;
    uint32_t count = 0;
    view.each<CrowdAgent, Health>(
        [&](EntityId id, CrowdAgent&, Health& hp) {
            ++count;
            if (hp.current <= 0.0f) {
                cmds.destroy(id);
                ++s_deaths_this_tick;
            }
        });
    return count;
}

// -----------------------------------------------------------------------
//  apply_separation
// -----------------------------------------------------------------------
// Soft local repulsion using the spatial grid built by select_targets.
// For each agent, find neighbors within personal-space radius and add a
// push velocity proportional to overlap.  This runs after ApplyCrowdSteer
// so pursuit velocity is already set; separation nudges agents apart
// without overriding pursuit intent.

uint32_t apply_separation(WorldView& view, float /*dt*/,
                          CommandBuffer& /*cmds*/) {
    s_separation_pairs = 0;
    uint32_t count = 0;
    view.each<CrowdAgent, Position, Velocity, Separation, MoveSpeed, BehaviorLod>(
        [&](EntityId self, CrowdAgent&, Position& pos,
            Velocity& vel, Separation& sep, MoveSpeed& spd, BehaviorLod& lod) {
            // LOD gating: skip separation for non-T0 agents on off-ticks.
            if (lod_should_skip(lod)) { return; }
            ++count;
            float push_x = 0.0f;
            float push_y = 0.0f;

            s_separation_pairs += s_grid.for_each_nearby(
                pos.x, pos.y, sep.radius, self,
                [&](const SpatialGrid::Entry& e, float d2) {
                    float dist = std::sqrt(d2);
                    float overlap = 1.0f - dist / sep.radius;
                    float dx = pos.x - e.x;
                    float dy = pos.y - e.y;
                    if (dist > 1e-6f) {
                        dx /= dist;
                        dy /= dist;
                    } else {
                        // Exact overlap: deterministic antisymmetric
                        // tiebreak on EntityId.  Lower index pushes +X,
                        // higher index pushes -X.  Guarantees the pair
                        // receives opposite impulses.
                        dx = (self.index < e.id.index) ? 1.0f : -1.0f;
                        dy = 0.0f;
                    }
                    push_x += dx * overlap * sep.strength;
                    push_y += dy * overlap * sep.strength;
                });

            vel.dx += push_x;
            vel.dy += push_y;

            // Reclamp to MoveSpeed.max so separation never violates
            // the speed contract established by ApplyCrowdSteer.
            float speed2 = vel.dx * vel.dx + vel.dy * vel.dy;
            float max2   = spd.max * spd.max;
            if (speed2 > max2) {
                float scale = spd.max / std::sqrt(speed2);
                vel.dx *= scale;
                vel.dy *= scale;
            }
        });
    return count;
}

}  // namespace de
