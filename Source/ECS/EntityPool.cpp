#include "ECS/EntityPool.h"

namespace de {

EntityId EntityPool::create() {
    uint32_t idx;

    if (!free_indices_.empty()) {
        idx = free_indices_.back();
        free_indices_.pop_back();
        slots_[idx].alive = true;
    } else {
        idx = static_cast<uint32_t>(slots_.size());
        slots_.push_back(Slot{1, true});
    }

    return EntityId{idx, slots_[idx].generation};
}

void EntityPool::destroy(EntityId id) {
    if (!alive(id)) return;

    Slot& s  = slots_[id.index];
    s.alive  = false;
    ++s.generation;           // bump so old ids become stale
    free_indices_.push_back(id.index);
}

bool EntityPool::alive(EntityId id) const {
    if (id.index >= slots_.size()) return false;
    const Slot& s = slots_[id.index];
    return s.alive && s.generation == id.generation;
}

uint32_t EntityPool::count() const {
    uint32_t n = 0;
    for (auto& s : slots_) {
        if (s.alive) ++n;
    }
    return n;
}

}  // namespace de
