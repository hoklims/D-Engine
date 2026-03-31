#pragma once

#include "ECS/WorldView.h"
#include "Runtime/CommandBuffer.h"

#include <cstdint>

namespace de {

// Crowd pipeline systems -- run in this order before physics integration.

// Pick the nearest enemy as pursuit target.
uint32_t select_targets(WorldView& view, float dt, CommandBuffer& cmds);

// Compute normalised desired direction toward current target.
// Zeroes direction when target is within attack range (stop to fight).
uint32_t compute_desired_movement(WorldView& view, float dt, CommandBuffer& cmds);

// Set velocity = desired_direction * move_speed (instant steering).
uint32_t apply_crowd_steering(WorldView& view, float dt, CommandBuffer& cmds);

// Tick cooldowns, deal damage when in range and ready.
uint32_t attack_targets(WorldView& view, float dt, CommandBuffer& cmds);

// Queue CommandBuffer::destroy for agents whose health <= 0.
uint32_t remove_dead(WorldView& view, float dt, CommandBuffer& cmds);

// Per-tick combat counters (reset at the start of each system call).
uint32_t crowd_attacks_this_tick();
uint32_t crowd_deaths_queued_this_tick();
void     reset_crowd_tick_counters();

}  // namespace de
