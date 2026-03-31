#pragma once

#include "ECS/ComponentId.h"
#include "ECS/EntityPool.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace de {

// -----------------------------------------------------------------
// Archetype  --  stores entities that share the exact same set of
//                component types, laid out as SoA columns.
// -----------------------------------------------------------------
struct Archetype {
    // --- type signature ---------------------------------------------------
    std::vector<ComponentInfo>  component_infos;   // sorted by ComponentId
    std::vector<EntityId>      entities;           // parallel to columns

    // --- SoA columns (raw byte storage) -----------------------------------
    std::vector<uint8_t*>      columns;            // one allocation per component

    std::size_t                count    = 0;
    std::size_t                capacity = 0;

    // --- queries ----------------------------------------------------------
    bool       has(ComponentId cid) const;
    int        column_index(ComponentId cid) const;  // -1 if absent

    // --- row management ---------------------------------------------------
    std::size_t push_entity(EntityId id);            // append, returns row
    void        remove_row(std::size_t row);         // swap-remove (destroys components)
    void        erase_row(std::size_t row);          // swap-remove (NO destruction -- for migration)
    void        reserve(std::size_t new_cap);

    // --- component access -------------------------------------------------
    void*       get_raw(std::size_t col, std::size_t row);
    const void* get_raw(std::size_t col, std::size_t row) const;

    // --- lifecycle --------------------------------------------------------
    void        clear();
    ~Archetype();

    Archetype() = default;
    Archetype(Archetype&& other) noexcept;
    Archetype& operator=(Archetype&& other) noexcept;
    Archetype(const Archetype&) = delete;
    Archetype& operator=(const Archetype&) = delete;
};

}  // namespace de
