#include "Render/RenderFrame.h"
#include "ECS/World.h"
#include "Runtime/Components.h"
#include "Runtime/CrowdComponents.h"

namespace de {

uint32_t extract_render_frame(World& world, uint64_t tick, uint64_t frame_id, RenderFrame& out) {
    out = RenderFrame{};
    out.sim_tick = tick;
    out.frame_id = frame_id;

    uint32_t idx = 0;
    world.each<CrowdAgent, Position, Team, Health, BehaviorLod>(
        [&](EntityId, CrowdAgent&, Position& pos, Team& team,
            Health& hp, BehaviorLod& lod) {
            if (idx < k_max_render_agents) {
                auto& item     = out.agents[idx];
                item.x         = pos.x;
                item.y         = pos.y;
                item.team_id   = team.id;
                item.lod_tier  = lod.tier;
                item.health_pct = (hp.max > 0.0f) ? (hp.current / hp.max) : 0.0f;
            }
            ++idx;
        });

    out.agent_count     = idx;
    out.extracted_count = (idx < k_max_render_agents) ? idx : k_max_render_agents;
    return out.extracted_count;
}

}  // namespace de
