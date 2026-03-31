#include "ECS/Archetype.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

// Aligned allocation helpers -- MSVC uses _aligned_malloc/_aligned_free,
// standard C++17 would use std::aligned_alloc but MSVC doesn't support it.
static void* aligned_alloc_impl(std::size_t alignment, std::size_t size) {
    return _aligned_malloc(size, alignment);
}

static void aligned_free_impl(void* ptr) {
    _aligned_free(ptr);
}

namespace de {

// -----------------------------------------------------------------
//  Signature queries
// -----------------------------------------------------------------

bool Archetype::has(ComponentId cid) const {
    return column_index(cid) >= 0;
}

int Archetype::column_index(ComponentId cid) const {
    for (std::size_t i = 0; i < component_infos.size(); ++i) {
        if (component_infos[i].id == cid) return static_cast<int>(i);
    }
    return -1;
}

// -----------------------------------------------------------------
//  Row management
// -----------------------------------------------------------------

void Archetype::reserve(std::size_t new_cap) {
    if (new_cap <= capacity) return;

    for (std::size_t c = 0; c < component_infos.size(); ++c) {
        auto& info = component_infos[c];
        auto* buf  = static_cast<uint8_t*>(
            aligned_alloc_impl(info.align, new_cap * info.size));

        if (count > 0 && columns[c]) {
            info.move(columns[c], buf, count);
        }
        aligned_free_impl(columns[c]);
        columns[c] = buf;
    }
    capacity = new_cap;
}

std::size_t Archetype::push_entity(EntityId id) {
    if (count == capacity) {
        reserve(capacity == 0 ? 8 : capacity * 2);
    }
    entities.push_back(id);
    // columns: space already reserved, caller must placement-new
    ++count;
    return count - 1;
}

void Archetype::remove_row(std::size_t row) {
    if (row >= count) return;

    std::size_t last = count - 1;

    for (std::size_t c = 0; c < component_infos.size(); ++c) {
        auto& info = component_infos[c];
        uint8_t* base = columns[c];

        // destroy element at row
        info.destroy(base + row * info.size, 1);

        if (row != last) {
            // move last element into the vacated row
            info.move(base + last * info.size,
                      base + row  * info.size, 1);
        }
    }

    // swap-remove entity id
    if (row != last) {
        entities[row] = entities[last];
    }
    entities.pop_back();
    --count;
}

void Archetype::erase_row(std::size_t row) {
    if (row >= count) return;

    std::size_t last = count - 1;

    // move last element's data into vacated row WITHOUT destroying row first
    // (the caller already moved the data out via migration)
    if (row != last) {
        for (std::size_t c = 0; c < component_infos.size(); ++c) {
            auto& info = component_infos[c];
            uint8_t* base = columns[c];
            info.move(base + last * info.size,
                      base + row  * info.size, 1);
        }
        entities[row] = entities[last];
    }
    entities.pop_back();
    --count;
}

// -----------------------------------------------------------------
//  Component access
// -----------------------------------------------------------------

void* Archetype::get_raw(std::size_t col, std::size_t row) {
    return columns[col] + row * component_infos[col].size;
}

const void* Archetype::get_raw(std::size_t col, std::size_t row) const {
    return columns[col] + row * component_infos[col].size;
}

// -----------------------------------------------------------------
//  Lifecycle
// -----------------------------------------------------------------

void Archetype::clear() {
    for (std::size_t c = 0; c < component_infos.size(); ++c) {
        if (count > 0 && columns[c]) {
            component_infos[c].destroy(columns[c], count);
        }
        aligned_free_impl(columns[c]);
    }
    columns.clear();
    entities.clear();
    component_infos.clear();
    count    = 0;
    capacity = 0;
}

Archetype::~Archetype() {
    clear();
}

Archetype::Archetype(Archetype&& other) noexcept
    : component_infos(std::move(other.component_infos))
    , entities(std::move(other.entities))
    , columns(std::move(other.columns))
    , count(other.count)
    , capacity(other.capacity)
{
    other.count    = 0;
    other.capacity = 0;
}

Archetype& Archetype::operator=(Archetype&& other) noexcept {
    if (this != &other) {
        clear();
        component_infos = std::move(other.component_infos);
        entities        = std::move(other.entities);
        columns         = std::move(other.columns);
        count           = other.count;
        capacity        = other.capacity;
        other.count     = 0;
        other.capacity  = 0;
    }
    return *this;
}

}  // namespace de
