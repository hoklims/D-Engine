#pragma once

#include "ECS/WorldView.h"
#include "Runtime/CommandBuffer.h"

#include <cstdint>

namespace de {

// Crowd pipeline systems -- run in this order before physics integration.

// Pick the nearest enemy as pursuit target.
uint32_t select_targets(WorldView& view, float dt, CommandBuffer& cmds);

// Compute normalised desired direction toward current target.
uint32_t compute_desired_movement(WorldView& view, float dt, CommandBuffer& cmds);

// Set velocity = desired_direction * move_speed (instant steering).
uint32_t apply_crowd_steering(WorldView& view, float dt, CommandBuffer& cmds);

}  // namespace de
