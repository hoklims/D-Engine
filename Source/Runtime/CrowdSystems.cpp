#include "Runtime/CrowdSystems.h"
#include "Runtime/Components.h"
#include "Runtime/CrowdComponents.h"

#include <cfloat>
#include <cmath>

namespace de {

// -- Per-tick combat counters (file scope) -----------------------------------

static uint32_t s_attacks_this_tick = 0;
static uint32_t s_deaths_this_tick  = 0;

uint32_t crowd_attacks_this_tick()       { return s_attacks_this_tick; }
uint32_t crowd_deaths_queued_this_tick() { return s_deaths_this_tick; }
void     reset_crowd_tick_counters()     { s_attacks_this_tick = 0; s_deaths_this_tick = 0; }

// -----------------------------------------------------------------------
//  select_targets
// -----------------------------------------------------------------------
// For each crowd agent, find the nearest living enemy (different team).
// Keeps the current target if it is still alive and still an enemy.

uint32_t select_targets(WorldView& view, float /*dt*/, CommandBuffer& /*cmds*/) {
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

            // Find nearest enemy.
            float    best_d2 = FLT_MAX;
            EntityId best    = {};
            bool     found   = false;

            view.each<CrowdAgent, Team, Position>(
                [&](EntityId other, CrowdAgent&, Team& ot, Position& op) {
                    if (other.index == self.index &&
                        other.generation == self.generation) return;
                    if (ot.id == my_team.id) return;
                    float dx = op.x - my_pos.x;
                    float dy = op.y - my_pos.y;
                    float d2 = dx * dx + dy * dy;
                    if (d2 < best_d2) {
                        best_d2 = d2;
                        best    = other;
                        found   = true;
                    }
                });

            tgt.entity     = best;
            tgt.has_target = found;
        });
    return count;
}

// -----------------------------------------------------------------------
//  compute_desired_movement
// -----------------------------------------------------------------------
// Turns a target into a normalised direction vector.

uint32_t compute_desired_movement(WorldView& view, float /*dt*/,
                                  CommandBuffer& /*cmds*/) {
    uint32_t count = 0;
    view.each<CrowdAgent, Position, Target, DesiredDirection, AttackRange>(
        [&](EntityId, CrowdAgent&, Position& pos,
            Target& tgt, DesiredDirection& dir, AttackRange& atk_range) {
            ++count;

            if (!tgt.has_target || !view.alive(tgt.entity)) {
                dir.dx = 0.0f;
                dir.dy = 0.0f;
                return;
            }

            const auto* tp = view.get<Position>(tgt.entity);
            if (!tp) {
                dir.dx = 0.0f;
                dir.dy = 0.0f;
                return;
            }

            float dx  = tp->x - pos.x;
            float dy  = tp->y - pos.y;
            float len = std::sqrt(dx * dx + dy * dy);

            // In attack range -- stop moving, fight instead.
            if (len <= atk_range.range) {
                dir.dx = 0.0f;
                dir.dy = 0.0f;
                return;
            }

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
//  attack_targets
// -----------------------------------------------------------------------
// Tick cooldowns.  When in range and cooldown ready, deal damage.

uint32_t attack_targets(WorldView& view, float dt, CommandBuffer& /*cmds*/) {
    s_attacks_this_tick = 0;
    uint32_t count = 0;
    view.each<CrowdAgent, Position, Target, AttackRange,
              AttackDamage, AttackCooldown>(
        [&](EntityId, CrowdAgent&, Position& pos, Target& tgt,
            AttackRange& range, AttackDamage& dmg, AttackCooldown& cd) {
            ++count;
            cd.remaining -= dt;

            if (!tgt.has_target || !view.alive(tgt.entity)) return;

            const auto* tp = view.get<Position>(tgt.entity);
            if (!tp) return;

            float dx   = tp->x - pos.x;
            float dy   = tp->y - pos.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > range.range) return;
            if (cd.remaining > 0.0f) return;

            auto* target_hp = view.get<Health>(tgt.entity);
            if (target_hp) {
                target_hp->current -= dmg.damage;
                ++s_attacks_this_tick;
            }
            cd.remaining = cd.interval;
        });
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

}  // namespace de
