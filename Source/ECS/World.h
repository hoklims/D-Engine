#pragma once

#include "ECS/EntityPool.h"
#include "ECS/Archetype.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <tuple>
#include <vector>

namespace de {

// -----------------------------------------------------------------
// World  --  top-level ECS facade
//
// Owns the entity pool and all archetypes.  Provides typed API for
// creating entities, adding/removing components, and iterating.
// -----------------------------------------------------------------
struct World {
    // --- entity lifecycle -------------------------------------------------
    EntityId create();
    void     destroy(EntityId id);
    bool     alive(EntityId id) const;

    // --- component operations ---------------------------------------------
    template <typename T>
    void set(EntityId id, T value);

    template <typename T>
    void remove(EntityId id);

    template <typename T>
    bool has(EntityId id) const;

    template <typename T>
    T*       get(EntityId id);

    template <typename T>
    const T* get(EntityId id) const;

    // --- iteration --------------------------------------------------------
    // each<A, B, ...>(func) calls func(EntityId, A&, B&, ...) for every
    // entity that has at least components A, B, ...
    template <typename... Cs, typename F>
    void each(F&& func);

    uint32_t entity_count() const;

    // Returns true if an each() iteration is in progress.
    // Structural mutations (create/destroy/set-new-type/remove) assert
    // on this in Debug builds.
    bool is_iterating() const;

private:
    // Location of an entity inside an archetype
    struct Record {
        uint32_t archetype = 0;   // index into archetypes_
        uint32_t row       = 0;   // row inside that archetype
    };

    EntityPool             pool_;
    std::vector<Record>    records_;       // indexed by EntityId::index
    std::vector<Archetype> archetypes_;

    // --- internal helpers ------------------------------------------------
    uint32_t iterating_ = 0;

    // RAII guard for iterating_ counter.  Exception-safe.
    struct IterationGuard {
        uint32_t& counter;
        explicit IterationGuard(uint32_t& c) : counter(c) { ++counter; }
        ~IterationGuard() { --counter; }
        IterationGuard(const IterationGuard&) = delete;
        IterationGuard& operator=(const IterationGuard&) = delete;
    };

    // Fatal check active in ALL builds (not just Debug).
    void enforce_not_iterating() const {
        if (iterating_ > 0) {
            std::fprintf(stderr,
                "FATAL: structural ECS mutation during iteration\n");
            std::fflush(stderr);
            std::abort();
        }
    }

    Archetype&  archetype_of(EntityId id);
    Record&     record_of(EntityId id);
    uint32_t    find_or_create_archetype(const std::vector<ComponentInfo>& infos);
    void        move_entity(EntityId id, uint32_t dst_arch);
};

// =====================================================================
//  Template implementations (must be in header)
// =====================================================================

template <typename T>
void World::set(EntityId id, T value) {
    if (!alive(id)) return;

    auto& rec  = record_of(id);
    auto& arch = archetypes_[rec.archetype];
    int   col  = arch.column_index(component_id<T>());

    if (col >= 0) {
        // component already present -- overwrite in place
        T* ptr = static_cast<T*>(arch.get_raw(static_cast<std::size_t>(col),
                                               rec.row));
        *ptr = static_cast<T&&>(value);
        return;
    }

    // build new signature = current + T
    std::vector<ComponentInfo> new_infos = arch.component_infos;
    new_infos.push_back(make_component_info<T>());

    // sort by ComponentId to keep canonical order
    // std::less<> provides a guaranteed total order on pointers (C++14 [comparisons])
    std::sort(new_infos.begin(), new_infos.end(),
              [](const ComponentInfo& a, const ComponentInfo& b) {
                  return std::less<const void*>{}(a.id, b.id);
              });

    uint32_t dst = find_or_create_archetype(new_infos);

    // move entity to new archetype (copies shared columns)
    move_entity(id, dst);

    // write the new component into the freshly-added column
    auto& dst_arch = archetypes_[dst];
    int   new_col  = dst_arch.column_index(component_id<T>());
    T*    slot     = static_cast<T*>(dst_arch.get_raw(
                        static_cast<std::size_t>(new_col), rec.row));
    new (slot) T(static_cast<T&&>(value));
}

template <typename T>
void World::remove(EntityId id) {
    if (!alive(id)) return;

    auto& rec  = record_of(id);
    auto& arch = archetypes_[rec.archetype];
    int   col  = arch.column_index(component_id<T>());
    if (col < 0) return;  // doesn't have it

    // build new signature = current - T
    std::vector<ComponentInfo> new_infos;
    for (auto& ci : arch.component_infos) {
        if (ci.id != component_id<T>()) {
            new_infos.push_back(ci);
        }
    }

    uint32_t dst = find_or_create_archetype(new_infos);
    move_entity(id, dst);
}

template <typename T>
bool World::has(EntityId id) const {
    if (!alive(id)) return false;
    auto& rec  = records_[id.index];
    auto& arch = archetypes_[rec.archetype];
    return arch.has(component_id<T>());
}

template <typename T>
T* World::get(EntityId id) {
    if (!alive(id)) return nullptr;
    auto& rec  = record_of(id);
    auto& arch = archetypes_[rec.archetype];
    int   col  = arch.column_index(component_id<T>());
    if (col < 0) return nullptr;
    return static_cast<T*>(arch.get_raw(static_cast<std::size_t>(col),
                                         rec.row));
}

template <typename T>
const T* World::get(EntityId id) const {
    if (!alive(id)) return nullptr;
    auto& rec  = records_[id.index];
    auto& arch = archetypes_[rec.archetype];
    int   col  = arch.column_index(component_id<T>());
    if (col < 0) return nullptr;
    return static_cast<const T*>(arch.get_raw(static_cast<std::size_t>(col),
                                               rec.row));
}

template <typename... Cs, typename F>
void World::each(F&& func) {
    IterationGuard guard(iterating_);
    for (auto& arch : archetypes_) {
        if (arch.count == 0) continue;

        // resolve column indices for each requested component
        int cols[] = { arch.column_index(component_id<Cs>())... };
        bool all_present = true;
        for (int c : cols) {
            if (c < 0) { all_present = false; break; }
        }
        if (!all_present) continue;

        // iterate rows
        std::size_t n = arch.count;
        // get base pointers once
        std::tuple<Cs*...> bases{
            static_cast<Cs*>(arch.get_raw(
                static_cast<std::size_t>(
                    arch.column_index(component_id<Cs>())), 0))...
        };

        for (std::size_t row = 0; row < n; ++row) {
            func(arch.entities[row],
                 std::get<Cs*>(bases)[row]...);
        }
    }
}

}  // namespace de
