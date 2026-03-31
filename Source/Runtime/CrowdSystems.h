#pragma once

#include "ECS/WorldView.h"
#include "Runtime/CommandBuffer.h"

#include <cstdint>

namespace de {

// Crowd combat contract -- alive-at-tick-start + simultaneous damage
//
// Tick entry:
//   SimState::tick() calls cull_pre_dead() BEFORE any system runs.
//   Agents with Health <= 0 at tick start are destroyed immediately
//   (direct World::destroy, not CommandBuffer).  They never act.
//
// Pipeline order:
//   SelectTargets > ComputeDesiredMove > ApplyCrowdSteer >
//   AttackTargets > ResolveDamage > RemoveDead >
//   IntegrateVelocity > IntegratePosition
//
// AttackTargets produces hit events into a buffer without modifying HP.
// ResolveDamage consumes the buffer and applies all damage at once.
// This guarantees that the result is independent of iteration order:
// every agent alive at the start of the tick gets to act, and all
// damage is applied simultaneously before death checks.
//
// Deaths caused during the tick (ResolveDamage -> RemoveDead) are
// deferred via CommandBuffer and applied after all systems finish.

// Pick the nearest enemy as pursuit target.
uint32_t select_targets(WorldView& view, float dt, CommandBuffer& cmds);

// Compute normalised desired direction toward current target.
// Zeroes direction when target is within attack range (stop to fight).
uint32_t compute_desired_movement(WorldView& view, float dt, CommandBuffer& cmds);

// Set velocity = desired_direction * move_speed (instant steering).
uint32_t apply_crowd_steering(WorldView& view, float dt, CommandBuffer& cmds);

// Tick cooldowns, produce hit events when in range and ready.
// Does NOT modify Health directly -- damage is deferred to ResolveDamage.
uint32_t attack_targets(WorldView& view, float dt, CommandBuffer& cmds);

// Consume hit events and apply accumulated damage to Health components.
uint32_t resolve_damage(WorldView& view, float dt, CommandBuffer& cmds);

// Queue CommandBuffer::destroy for agents whose health <= 0.
uint32_t remove_dead(WorldView& view, float dt, CommandBuffer& cmds);

// Soft local separation: push agents apart when within personal space.
// Runs after ApplyCrowdSteer so velocity already carries pursuit intent.
uint32_t apply_separation(WorldView& view, float dt, CommandBuffer& cmds);

// Per-tick counters (reset at the start of each tick).
uint32_t crowd_attacks_this_tick();
uint32_t crowd_deaths_queued_this_tick();
uint32_t crowd_candidates_scanned_this_tick();
uint32_t crowd_separation_pairs_this_tick();
void     reset_crowd_tick_counters();

}  // namespace de
