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

}  // namespace de
