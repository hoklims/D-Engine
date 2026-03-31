#include "Runtime/SimState.h"

#include <chrono>

namespace de {

// -- Fixed systems -----------------------------------------------------------

static uint32_t integrate_velocity(World& world, float dt, CommandBuffer&) {
    uint32_t count = 0;
    world.each<Velocity, Acceleration>(
        [dt, &count](EntityId, Velocity& v, Acceleration& a) {
            v.dx += a.ax * dt;
            v.dy += a.ay * dt;
            ++count;
        });
    return count;
}

static uint32_t integrate_position(World& world, float dt, CommandBuffer&) {
    uint32_t count = 0;
    world.each<Position, Velocity>(
        [dt, &count](EntityId, Position& p, Velocity& v) {
            p.x += v.dx * dt;
            p.y += v.dy * dt;
            ++count;
        });
    return count;
}

// -- SimState ----------------------------------------------------------------

void SimState::bootstrap() {
    world = World{};
    tick_count_        = 0;
    system_count_      = 0;
    cmds_queued_last_  = 0;
    cmds_applied_last_ = 0;
    cmds_.clear();
    for (auto& s : last_stats_) s = {};

    register_systems();

    constexpr int count = 4;
    for (int i = 0; i < count; ++i) {
        EntityId e = world.create();
        world.set(e, Position{static_cast<float>(i) * 10.0f, 0.0f});
        world.set(e, Velocity{1.0f, 0.5f});
        world.set(e, Acceleration{0.0f, 0.0f});
    }
}

void SimState::tick(double step_dt) {
    float dt = static_cast<float>(step_dt);
    cmds_.clear();

    for (uint32_t i = 0; i < system_count_; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        uint32_t n = pipeline_[i].fn(world, dt, cmds_);
        auto t1 = std::chrono::high_resolution_clock::now();

        last_stats_[i].name = pipeline_[i].name;
        last_stats_[i].elapsed_s =
            std::chrono::duration<double>(t1 - t0).count();
        last_stats_[i].entities_processed = n;
    }

    cmds_queued_last_ = cmds_.pending();
    cmds_.apply(world);
    cmds_applied_last_ = cmds_.last_applied_count();

    ++tick_count_;
}

void SimState::shutdown() {
    world = World{};
    tick_count_        = 0;
    system_count_      = 0;
    cmds_queued_last_  = 0;
    cmds_applied_last_ = 0;
    cmds_.clear();
    for (auto& s : last_stats_) s = {};
}

SimSnapshot SimState::snapshot() const {
    SimSnapshot snap;
    snap.entity_count  = world.entity_count();
    snap.tick_count    = tick_count_;
    snap.system_count  = system_count_;
    snap.cmds_queued   = cmds_queued_last_;
    snap.cmds_applied  = cmds_applied_last_;
    for (uint32_t i = 0; i < system_count_; ++i) {
        snap.systems[i] = last_stats_[i];
    }
    return snap;
}

uint32_t SimState::system_count() const {
    return system_count_;
}

void SimState::register_systems() {
    add_system("IntegrateVelocity", integrate_velocity);
    add_system("IntegratePosition", integrate_position);
}

void SimState::add_system(const char* name, FixedSystemFn fn) {
    if (system_count_ >= k_max_sim_systems) return;
    pipeline_[system_count_] = {name, fn};
    ++system_count_;
}

}  // namespace de
