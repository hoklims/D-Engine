#include "Runtime/CrowdSystems.h"
#include "Runtime/Components.h"
#include "Runtime/CrowdComponents.h"

#include <cfloat>
#include <cmath>

namespace de {

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
    view.each<CrowdAgent, Position, Target, DesiredDirection>(
        [&](EntityId, CrowdAgent&, Position& pos,
            Target& tgt, DesiredDirection& dir) {
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

}  // namespace de
