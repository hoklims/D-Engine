#pragma once

#include "ECS/WorldView.h"
#include "Runtime/CommandBuffer.h"
#include "Runtime/BattlefieldGrid.h"
#include "Runtime/CrowdComponents.h"

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
//   SelectTargets > ComputeBattleGoal > ComputeDesiredMove >
//   ApplyCrowdSteer > ApplySeparation >
//   AttackTargets > ResolveDamage > RemoveDead >
//   IntegrateVelocity > IntegratePosition
//
// ComputeBattleGoal sets DesiredDirection toward the strategic rally
// point.  ComputeDesiredMove overrides it with local pursuit when the
// nearest enemy is within EngageRadius.
//
// AttackTargets produces hit events into a buffer without modifying HP.
// ResolveDamage consumes the buffer and applies all damage at once.
// This guarantees that the result is independent of iteration order:
// every agent alive at the start of the tick gets to act, and all
// damage is applied simultaneously before death checks.
//
// Deaths caused during the tick (ResolveDamage -> RemoveDead) are
// deferred via CommandBuffer and applied after all systems finish.

// Classify each agent into a behavior LOD tier based on engagement
// state and distance to battle center.  Must run before any gated
// system so that tier/stride are fresh for the current tick.
uint32_t classify_behavior_lod(WorldView& view, float dt, CommandBuffer& cmds);

// Pick the nearest enemy as pursuit target.
uint32_t select_targets(WorldView& view, float dt, CommandBuffer& cmds);

// Set desired direction toward BattleGoal (strategic layer).
// ComputeDesiredMove may override this with local pursuit.
//
// Navigation contract:
//   If per-team BattlefieldGrids are installed, direction is sampled
//   from the flow field.  When sample_flow() fails (agent out of grid,
//   blocked cell, unreachable cell), the agent receives a ZERO direction
//   instead of falling back to direct line-of-sight.  This guarantees
//   that obstacles are never silently bypassed.
//   When no grid is installed, direct line toward BattleGoal is used.
uint32_t compute_battle_goal(WorldView& view, float dt, CommandBuffer& cmds);

// Install / remove per-team battlefield navigation grids.
// Array indexed by team ID.  Pass nullptr/0 to disable navigation.
// Pointers must remain valid for the lifetime of the crowd tick.
void     set_battlefield_grids(const BattlefieldGrid* const* grids, uint32_t count);
uint32_t get_battlefield_grid_count();

// Override desired direction with local pursuit when the nearest enemy
// is within EngageRadius.  Zeroes direction inside attack range (stop to fight).
// Leaves battle-goal direction intact when no enemy is close enough.
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
uint32_t crowd_agents_engaged_this_tick();
uint32_t crowd_nav_queries_this_tick();
uint32_t crowd_nav_failures_this_tick();
uint32_t crowd_lod_tier_count(uint8_t tier);
uint32_t crowd_lod_skipped_this_tick();

// Install LOD config and current tick count for gating decisions.
void     set_behavior_lod_config(const BehaviorLodConfig* cfg);
void     set_crowd_tick_count(uint64_t tick);
void     reset_crowd_tick_counters();

}  // namespace de
