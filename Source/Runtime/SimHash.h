#pragma once

// Deterministic simulation hash for crowd runtime.
//
// Contract -- what this proves:
//   - Two runs with identical bootstrap + identical dt sequence produce
//     identical hash sequences.  Any divergence in hashed state is
//     detected at the exact tick it occurs.
//
// Contract -- what this does NOT prove (yet):
//   - Components outside the hash set (Acceleration, Separation params,
//     DesiredDirection, BattleGoal, EngageRadius, AttackRange, AttackDamage)
//   - Non-crowd entities
//   - Cross-platform / cross-compiler bitwise stability
//   - Input sequence correctness (no input capture yet)
//
// Hash strategy:
//   FNV-1a 64-bit over: tick_count, living crowd agent count, then
//   per-agent (sorted by EntityId.index): identity (index+generation) +
//   Position + Velocity + Health + Target (full EntityId + has_target) +
//   AttackCooldown + BehaviorLod + Team.
//   Sorting by EntityId.index guarantees independence from archetype
//   layout and swap-remove ordering.

#include "ECS/World.h"
#include "Runtime/Components.h"
#include "Runtime/CrowdComponents.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace de {

// -- FNV-1a 64-bit hasher ---------------------------------------------------
// Simple, portable, zero-dependency.  NOT cryptographic.

struct Fnv1a {
    static constexpr uint64_t k_offset = 14695981039346656037ULL;
    static constexpr uint64_t k_prime  = 1099511628211ULL;

    uint64_t state = k_offset;

    void feed(const void* data, std::size_t len) {
        auto p = static_cast<const uint8_t*>(data);
        for (std::size_t i = 0; i < len; ++i) {
            state ^= p[i];
            state *= k_prime;
        }
    }

    template <typename T>
    void feed_value(const T& v) {
        feed(&v, sizeof(T));
    }
};

// -- compute_sim_hash -------------------------------------------------------

inline uint64_t compute_sim_hash(World& world, uint64_t tick_count) {
    // 1. Collect living crowd agent IDs.
    std::vector<EntityId> ids;
    world.each<CrowdAgent>([&](EntityId id, CrowdAgent&) {
        ids.push_back(id);
    });

    // 2. Sort by EntityId.index -- deterministic regardless of archetype
    //    layout or swap-remove history.
    std::sort(ids.begin(), ids.end(),
              [](EntityId a, EntityId b) { return a.index < b.index; });

    // 3. Hash: tick + count + per-agent state.
    Fnv1a h;
    h.feed_value(tick_count);
    uint32_t count = static_cast<uint32_t>(ids.size());
    h.feed_value(count);

    for (EntityId id : ids) {
        h.feed_value(id.index);
        h.feed_value(id.generation);

        auto* pos = world.get<Position>(id);
        if (pos) { h.feed_value(pos->x); h.feed_value(pos->y); }

        auto* vel = world.get<Velocity>(id);
        if (vel) { h.feed_value(vel->dx); h.feed_value(vel->dy); }

        auto* hp = world.get<Health>(id);
        if (hp) { h.feed_value(hp->current); h.feed_value(hp->max); }

        auto* tgt = world.get<Target>(id);
        if (tgt) {
            h.feed_value(tgt->entity.index);
            h.feed_value(tgt->entity.generation);
            uint8_t has = tgt->has_target ? 1 : 0;
            h.feed_value(has);
        }

        auto* cd = world.get<AttackCooldown>(id);
        if (cd) { h.feed_value(cd->remaining); h.feed_value(cd->interval); }

        auto* lod = world.get<BehaviorLod>(id);
        if (lod) { h.feed_value(lod->tier); h.feed_value(lod->stride); }

        auto* tm = world.get<Team>(id);
        if (tm) { h.feed_value(tm->id); }
    }

    return h.state;
}

// -- SimHashHistory -- ring buffer of recent tick hashes --------------------

static constexpr uint32_t k_hash_history_size = 64;

struct SimHashHistory {
    uint64_t hashes[k_hash_history_size] = {};
    uint32_t count  = 0;    // total hashes pushed (may exceed ring size)
    uint64_t latest = 0;    // most recent hash

    void push(uint64_t hash) {
        hashes[count % k_hash_history_size] = hash;
        latest = hash;
        ++count;
    }

    void clear() {
        for (auto& h : hashes) h = 0;
        count  = 0;
        latest = 0;
    }

    // Get hash at absolute tick index.  Returns 0 if evicted or out of range.
    uint64_t at(uint32_t tick) const {
        if (tick >= count) return 0;
        if (count > k_hash_history_size &&
            tick < count - k_hash_history_size) return 0;
        return hashes[tick % k_hash_history_size];
    }
};

// -- HashSequence -- headless run comparison utility -------------------------

struct HashSequence {
    std::vector<uint64_t> hashes;

    bool operator==(const HashSequence& other) const {
        return hashes == other.hashes;
    }
    bool operator!=(const HashSequence& other) const {
        return !(*this == other);
    }

    // First tick index where hashes diverge.  Returns -1 if identical.
    int first_divergence(const HashSequence& other) const {
        uint32_t len = static_cast<uint32_t>(
            (hashes.size() < other.hashes.size())
                ? hashes.size() : other.hashes.size());
        for (uint32_t i = 0; i < len; ++i) {
            if (hashes[i] != other.hashes[i])
                return static_cast<int>(i);
        }
        if (hashes.size() != other.hashes.size())
            return static_cast<int>(len);
        return -1;
    }
};

}  // namespace de
