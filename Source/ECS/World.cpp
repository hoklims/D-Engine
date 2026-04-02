#include "ECS/World.h"

#include <algorithm>

namespace de {

// -----------------------------------------------------------------
//  Entity lifecycle
// -----------------------------------------------------------------

bool World::is_iterating() const {
    return iterating_ > 0;
}

EntityId World::create() {
    enforce_not_iterating();
    EntityId id = pool_.create();

    // ensure records_ is large enough
    if (id.index >= records_.size()) {
        records_.resize(static_cast<std::size_t>(id.index) + 1);
    }

    // create empty archetype (index 0) if needed
    if (archetypes_.empty()) {
        archetypes_.emplace_back();
    }

    // place into the empty archetype (archetype 0 = no components)
    auto& empty_arch = archetypes_[0];
    std::size_t row = empty_arch.push_entity(id);

    records_[id.index] = Record{0, static_cast<uint32_t>(row)};
    return id;
}

void World::destroy(EntityId id) {
    enforce_not_iterating();
    if (!alive(id)) return;

    auto& rec  = record_of(id);
    auto& arch = archetypes_[rec.archetype];

    // if this was not the last row, the swap-remove moved another entity
    // into our row -- fix its record
    std::size_t last = arch.count - 1;
    if (rec.row != last) {
        EntityId moved = arch.entities[last];
        records_[moved.index].row = rec.row;
    }

    arch.remove_row(rec.row);
    pool_.destroy(id);
}

bool World::alive(EntityId id) const {
    return pool_.alive(id);
}

uint32_t World::entity_count() const {
    return pool_.count();
}

// -----------------------------------------------------------------
//  Internal helpers
// -----------------------------------------------------------------

World::Record& World::record_of(EntityId id) {
    return records_[id.index];
}

Archetype& World::archetype_of(EntityId id) {
    return archetypes_[records_[id.index].archetype];
}

uint32_t World::find_or_create_archetype(
    const std::vector<ComponentInfo>& infos)
{
    // linear scan -- fine for small number of archetypes
    for (uint32_t i = 0; i < archetypes_.size(); ++i) {
        auto& arch = archetypes_[i];
        if (arch.component_infos.size() != infos.size()) continue;

        bool match = true;
        for (std::size_t c = 0; c < infos.size(); ++c) {
            if (arch.component_infos[c].id != infos[c].id) {
                match = false;
                break;
            }
        }
        if (match) return i;
    }

    // create new archetype
    Archetype a;
    a.component_infos = infos;
    a.columns.resize(infos.size(), nullptr);
    archetypes_.push_back(std::move(a));
    return static_cast<uint32_t>(archetypes_.size() - 1);
}

void World::move_entity(EntityId id, uint32_t dst_idx) {
    enforce_not_iterating();
    auto& rec      = record_of(id);
    auto& src_arch = archetypes_[rec.archetype];
    auto& dst_arch = archetypes_[dst_idx];

    // allocate row in destination
    std::size_t dst_row = dst_arch.push_entity(id);

    // move shared columns to destination, destroy source-only columns
    for (std::size_t dc = 0; dc < dst_arch.component_infos.size(); ++dc) {
        int sc = src_arch.column_index(dst_arch.component_infos[dc].id);
        if (sc >= 0) {
            auto& info = dst_arch.component_infos[dc];
            void* src  = src_arch.get_raw(static_cast<std::size_t>(sc),
                                           rec.row);
            void* dst  = dst_arch.get_raw(dc, dst_row);
            info.move(src, dst, 1);
        }
        // columns from new components are left uninitialized --
        // caller must placement-new them after move_entity returns
    }

    // destroy components that exist in source but NOT in destination
    for (std::size_t sc = 0; sc < src_arch.component_infos.size(); ++sc) {
        if (dst_arch.column_index(src_arch.component_infos[sc].id) < 0) {
            auto& info = src_arch.component_infos[sc];
            info.destroy(src_arch.get_raw(sc, rec.row), 1);
        }
    }

    // swap-erase from source (no destruction -- data already moved/destroyed)
    std::size_t last = src_arch.count - 1;
    if (rec.row != last) {
        EntityId moved = src_arch.entities[last];
        records_[moved.index].row = rec.row;
    }
    src_arch.erase_row(rec.row);

    // update record
    rec.archetype = dst_idx;
    rec.row       = static_cast<uint32_t>(dst_row);
}

}  // namespace de
