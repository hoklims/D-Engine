#pragma once

#include "ECS/World.h"

namespace de {

// Read/write view over a World that hides structural mutation.
//
// Provides access to component DATA (each, get, has, alive) but
// NOT to structural operations (create, destroy, set, remove).
// Used by fixed systems to enforce the deferred-mutation contract
// at compile time.
struct WorldView {
    explicit WorldView(World& w) : world_(w) {}

    template <typename... Cs, typename F>
    void each(F&& func);

    template <typename T>
    T* get(EntityId id);

    template <typename T>
    const T* get(EntityId id) const;

    template <typename T>
    bool has(EntityId id) const;

    bool     alive(EntityId id) const;
    uint32_t entity_count() const;

private:
    World& world_;
};

// -- template / inline implementations ---------------------------------------

template <typename... Cs, typename F>
void WorldView::each(F&& func) {
    world_.each<Cs...>(std::forward<F>(func));
}

template <typename T>
T* WorldView::get(EntityId id) { return world_.get<T>(id); }

template <typename T>
const T* WorldView::get(EntityId id) const { return world_.get<T>(id); }

template <typename T>
bool WorldView::has(EntityId id) const { return world_.has<T>(id); }

inline bool WorldView::alive(EntityId id) const {
    return world_.alive(id);
}

inline uint32_t WorldView::entity_count() const {
    return world_.entity_count();
}

}  // namespace de
