#pragma once

#include "ECS/World.h"
#include "Runtime/Components.h"

#include <cstdint>

namespace de {

// Snapshot of the simulation state for debug / telemetry.
struct SimSnapshot {
    uint32_t entity_count = 0;
    uint64_t tick_count   = 0;
};

// Minimal simulation state: owns a World, bootstraps test entities,
// and runs a trivial Position += Velocity * dt system each tick.
//
// Lifecycle contract:
//   bootstrap() is idempotent -- it always resets to a virgin state
//   first (world cleared, tick_count zeroed), then spawns entities.
//   Safe to call multiple times or after tick()/shutdown().
struct SimState {
    World world;

    void bootstrap();
    void tick(double step_dt);
    void shutdown();

    SimSnapshot snapshot() const;

private:
    uint64_t tick_count_ = 0;
};

}  // namespace de
