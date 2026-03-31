#pragma once

#include "ECS/World.h"
#include "Runtime/CommandBuffer.h"
#include "Runtime/Components.h"

#include <cstdint>

namespace de {

static constexpr uint32_t k_max_sim_systems = 8;

// Per-system telemetry from the last tick.
struct SystemStats {
    const char*  name               = "";
    double       elapsed_s          = 0.0;
    uint32_t     entities_processed = 0;
};

// Debug snapshot of the simulation pipeline.
struct SimSnapshot {
    uint32_t    entity_count  = 0;
    uint64_t    tick_count    = 0;
    uint32_t    system_count  = 0;
    uint32_t    cmds_queued   = 0;
    uint32_t    cmds_applied  = 0;
    SystemStats systems[k_max_sim_systems] = {};
};

// Signature for a fixed-step simulation system.
// Systems receive a read/write World reference for component data,
// the timestep, and a CommandBuffer for deferred structural mutations.
// Returns the number of entities processed.
//
// Contract: systems MUST NOT call create/destroy/set(new type)/remove
// on the World directly.  Structural changes go through CommandBuffer.
using FixedSystemFn = uint32_t(*)(World& world, float dt, CommandBuffer& cmds);

// A named system in the fixed-step pipeline.
struct FixedSystem {
    const char*   name = "";
    FixedSystemFn fn   = nullptr;
};

// Owns a World and runs an ordered pipeline of fixed systems each tick.
//
// Lifecycle contract:
//   bootstrap() is idempotent -- always resets to virgin state first
//   (world cleared, tick_count zeroed, pipeline re-registered).
//   Safe to call multiple times or after tick()/shutdown().
//
// Tick flow:
//   1. Clear command buffer
//   2. Run all systems in order (they queue deferred commands)
//   3. Apply all deferred commands at once (spawns first, then destroys)
//   4. Increment tick_count
struct SimState {
    World world;

    void bootstrap();
    void tick(double step_dt);
    void shutdown();

    void add_system(const char* name, FixedSystemFn fn);

    SimSnapshot snapshot() const;
    uint32_t    system_count() const;

private:
    uint64_t      tick_count_            = 0;
    uint32_t      system_count_          = 0;
    uint32_t      cmds_queued_last_      = 0;
    uint32_t      cmds_applied_last_     = 0;
    FixedSystem   pipeline_[k_max_sim_systems]  = {};
    SystemStats   last_stats_[k_max_sim_systems] = {};
    CommandBuffer cmds_;

    void register_systems();
};

}  // namespace de
