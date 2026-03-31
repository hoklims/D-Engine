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

}  // namespace de
