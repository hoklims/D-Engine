#pragma once

#include "ECS/World.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace de {

// Deferred structural mutation buffer for the ECS World.
//
// Mutation contract for fixed systems:
//   During system execution (inside tick), structural mutations on the
//   World -- create(), destroy(), set() of new component types, remove()
//   -- are FORBIDDEN.  Only reading and writing existing component data
//   through each()/get() is allowed.
//
//   All structural changes must go through CommandBuffer.  The buffer is
//   applied at a single deterministic point: after ALL systems have run,
//   before tick_count increments.
//
//   Apply order: spawns first, then destroys.
struct CommandBuffer {
    // Queue destruction of an entity.
    void destroy(EntityId id);

    // Queue creation of a new entity.  init is called with the World
    // and the newly created EntityId during apply().
    template <typename F>
    void spawn(F&& init);

    // Apply all pending commands to the World.
    void apply(World& world);

    // Discard all pending commands.
    void clear();

    uint32_t pending() const;
    uint32_t last_applied_count() const;

private:
    std::vector<EntityId> destroys_;
    std::vector<std::function<void(World&, EntityId)>> spawns_;
    uint32_t last_applied_ = 0;
};

// -- inline / template implementations ---------------------------------------

inline void CommandBuffer::destroy(EntityId id) {
    destroys_.push_back(id);
}

template <typename F>
void CommandBuffer::spawn(F&& init) {
    spawns_.emplace_back(std::forward<F>(init));
}

inline void CommandBuffer::apply(World& world) {
    last_applied_ = 0;
    for (auto& init : spawns_) {
        EntityId e = world.create();
        init(world, e);
        ++last_applied_;
    }
    for (EntityId id : destroys_) {
        if (world.alive(id)) {
            world.destroy(id);
            ++last_applied_;
        }
    }
    spawns_.clear();
    destroys_.clear();
}

inline void CommandBuffer::clear() {
    spawns_.clear();
    destroys_.clear();
}

inline uint32_t CommandBuffer::pending() const {
    return static_cast<uint32_t>(spawns_.size() + destroys_.size());
}

inline uint32_t CommandBuffer::last_applied_count() const {
    return last_applied_;
}

}  // namespace de
