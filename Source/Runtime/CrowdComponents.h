#pragma once

#include "ECS/EntityPool.h"

#include <cstdint>

namespace de {

// Tag: marks an entity as a crowd agent.
struct CrowdAgent {};

// Faction membership (0-based).
struct Team { uint8_t id = 0; };

// Maximum movement speed (units/s).
struct MoveSpeed { float max = 3.0f; };

// Current pursuit target. has_target == false means idle.
struct Target {
    EntityId entity     = {};
    bool     has_target = false;
};

// Normalized desired movement direction (set by AI, consumed by steering).
struct DesiredDirection { float dx = 0.0f; float dy = 0.0f; };

// Minimal health pool.
struct Health {
    float current = 100.0f;
    float max     = 100.0f;
};

// Melee attack range (units). Agent stops and attacks when within range.
struct AttackRange { float range = 2.0f; };

// Flat damage dealt per attack.
struct AttackDamage { float damage = 10.0f; };

// Simple cooldown timer. remaining <= 0 means ready to attack.
struct AttackCooldown {
    float remaining = 0.0f;   // seconds until next attack
    float interval  = 1.0f;   // seconds between attacks
};

// Strategic rally point for the agent's team.
struct BattleGoal { float x = 0.0f; float y = 0.0f; };

// Distance threshold: enemy must be within this radius for the agent
// to switch from strategic (goal) to tactical (pursuit) mode.
struct EngageRadius { float radius = 15.0f; };

// Local separation -- soft repulsion from nearby agents.
struct Separation {
    float radius   = 0.8f;   // personal space (units)
    float strength = 5.0f;   // push magnitude at full overlap (units/s)
};

// Behavior LOD tier assigned each tick by the classification system.
// Determines simulation frequency for non-critical systems.
//
// Contract:
//   T0 = full fidelity, every tick
//   T1 = every 2 ticks
//   T2 = every 4 ticks
//   T3 = every 8 ticks (quasi-dormant)
//
// Classification criteria (simple, deterministic):
//   - engaged agents (enemy within EngageRadius) are always T0
//   - other agents: tier based on distance to battle center (0,0)
//
// Invariant: combat systems (AttackTargets, ResolveDamage, RemoveDead)
// and physics integration always run for ALL agents regardless of tier.
// Only strategic/navigation/separation systems are gated.
struct BehaviorLod {
    uint8_t  tier   = 0;   // 0-3
    uint8_t  stride = 1;   // update every N ticks (1, 2, 4, 8)
};

static constexpr uint8_t k_lod_tier_count = 4;
static constexpr uint8_t k_lod_strides[k_lod_tier_count] = {1, 2, 4, 8};

// Configurable distance thresholds for LOD classification.
struct BehaviorLodConfig {
    float t1_distance = 30.0f;   // beyond this -> T1
    float t2_distance = 60.0f;   // beyond this -> T2
    float t3_distance = 100.0f;  // beyond this -> T3
};

}  // namespace de
