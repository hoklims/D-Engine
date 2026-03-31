#pragma once

#include <cstdint>
#include <vector>

namespace de {

struct EntityId {
    uint32_t index      = 0;
    uint32_t generation = 0;

    bool operator==(EntityId other) const {
        return index == other.index && generation == other.generation;
    }
    bool operator!=(EntityId other) const { return !(*this == other); }
};

inline constexpr EntityId null_entity{0, 0};

// -----------------------------------------------------------------
// EntityPool  --  manages EntityId allocation with generational reuse
// -----------------------------------------------------------------
struct EntityPool {
    EntityId  create();
    void      destroy(EntityId id);
    bool      alive(EntityId id) const;
    uint32_t  count() const;

private:
    struct Slot {
        uint32_t generation = 1;   // starts at 1 so gen-0 ids are always invalid
        bool     alive      = false;
    };

    std::vector<Slot>     slots_;
    std::vector<uint32_t> free_indices_;
};

}  // namespace de
