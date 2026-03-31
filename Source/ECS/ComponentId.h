#pragma once

#include <cstdint>
#include <cstddef>
#include <typeinfo>

namespace de {

// -----------------------------------------------------------------
// ComponentId  --  lightweight type-erased component identifier
//
// Uses a function-pointer tag (one per type) to avoid RTTI overhead
// while still giving unique ids per component type.
// -----------------------------------------------------------------

using ComponentId = const void*;

template <typename T>
ComponentId component_id() {
    // Each instantiation produces a unique address.
    static const char tag = 0;
    return &tag;
}

// Runtime info needed to manage a component column
struct ComponentInfo {
    ComponentId id   = nullptr;
    std::size_t size = 0;           // sizeof(T)
    std::size_t align = 0;         // alignof(T)
    void (*destroy)(void* ptr, std::size_t count) = nullptr;
    void (*move)(void* src, void* dst, std::size_t count) = nullptr;
};

template <typename T>
ComponentInfo make_component_info() {
    ComponentInfo info;
    info.id    = component_id<T>();
    info.size  = sizeof(T);
    info.align = alignof(T);
    info.destroy = [](void* ptr, std::size_t count) {
        T* arr = static_cast<T*>(ptr);
        for (std::size_t i = 0; i < count; ++i) {
            arr[i].~T();
        }
    };
    info.move = [](void* src, void* dst, std::size_t count) {
        T* s = static_cast<T*>(src);
        T* d = static_cast<T*>(dst);
        for (std::size_t i = 0; i < count; ++i) {
            new (d + i) T(static_cast<T&&>(s[i]));
            s[i].~T();
        }
    };
    return info;
}

}  // namespace de
