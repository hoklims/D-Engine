#include "Render/RenderFrame.h"
#include "ECS/World.h"
#include "Runtime/Components.h"
#include "Runtime/CrowdComponents.h"

#include <cmath>

namespace de {

uint32_t extract_render_frame(World& world, uint64_t tick, uint64_t frame_id, RenderFrame& out) {
    out = RenderFrame{};
    out.sim_tick = tick;
    out.frame_id = frame_id;

    uint32_t idx = 0;
    world.each<CrowdAgent, Position, Velocity, DesiredDirection, Target,
               Team, Health, BehaviorLod>(
        [&](EntityId self, CrowdAgent&, Position& pos, Velocity& vel,
            DesiredDirection& dd, Target& tgt, Team& team,
            Health& hp, BehaviorLod& lod) {
            if (idx < k_max_render_agents) {
                auto& item      = out.agents[idx];
                item.x          = pos.x;
                item.y          = pos.y;
                item.team_id    = team.id;
                item.lod_tier   = lod.tier;
                item.health_pct = (hp.max > 0.0f) ? (hp.current / hp.max) : 0.0f;

                // Valid pursuit target: flag set, entity alive, not self,
                // and belongs to a different team.
                bool valid = false;
                if (tgt.has_target && tgt.entity != self
                    && world.alive(tgt.entity)) {
                    const Team* tt = world.get<Team>(tgt.entity);
                    valid = tt && tt->id != team.id;
                }
                item.has_target = valid;

                // Direction: velocity > desired direction > (0,1).
                float vlen2 = vel.dx * vel.dx + vel.dy * vel.dy;
                float dlen2 = dd.dx * dd.dx + dd.dy * dd.dy;

                if (vlen2 > 0.0001f) {
                    float inv  = 1.0f / std::sqrt(vlen2);
                    item.dir_x = vel.dx * inv;
                    item.dir_y = vel.dy * inv;
                } else if (dlen2 > 0.0001f) {
                    float inv  = 1.0f / std::sqrt(dlen2);
                    item.dir_x = dd.dx * inv;
                    item.dir_y = dd.dy * inv;
                } else {
                    item.dir_x = 0.0f;
                    item.dir_y = 1.0f;
                }
            }
            ++idx;
        });

    out.agent_count     = idx;
    out.extracted_count = (idx < k_max_render_agents) ? idx : k_max_render_agents;
    return out.extracted_count;
}

}  // namespace de
