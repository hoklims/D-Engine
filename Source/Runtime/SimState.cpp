#include "Runtime/SimState.h"

namespace de {

void SimState::bootstrap() {
    // Always reset to virgin state first (idempotent).
    world = World{};
    tick_count_ = 0;

    // Spawn a handful of entities with Position + Velocity for testing.
    constexpr int count = 4;
    for (int i = 0; i < count; ++i) {
        EntityId e = world.create();
        world.set(e, Position{static_cast<float>(i) * 10.0f, 0.0f});
        world.set(e, Velocity{1.0f, 0.5f});
    }
}

void SimState::tick(double step_dt) {
    float dt = static_cast<float>(step_dt);
    world.each<Position, Velocity>([dt](EntityId, Position& p, Velocity& v) {
        p.x += v.dx * dt;
        p.y += v.dy * dt;
    });
    ++tick_count_;
}

void SimState::shutdown() {
    world = World{};
    tick_count_ = 0;
}

SimSnapshot SimState::snapshot() const {
    return SimSnapshot{world.entity_count(), tick_count_};
}

}  // namespace de
