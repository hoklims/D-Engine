#pragma once

// Minimal 2D spatial hash grid for crowd target selection.
//
// Rebuilt every tick.  Cells are square buckets of side `cell_size`.
// Lookup uses expanding-ring search (Chebyshev distance) with early
// exit once the nearest enemy found is closer than any unsearched
// cell could be.
//
// NOT part of the generic ECS -- this is a crowd-runtime utility.

#include "ECS/EntityPool.h"

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace de {

struct SpatialGrid {

    struct Entry {
        EntityId id;
        uint8_t  team;
        float    x, y;
    };

    struct FindResult {
        EntityId id    = {};
        float    dist2 = 0.0f;
        bool     found = false;
    };

    float cell_size = 10.0f;

    void clear() { cells_.clear(); }

    void insert(EntityId id, uint8_t team, float x, float y) {
        cells_[make_key(to_cell(x), to_cell(y))].push_back({id, team, x, y});
    }

    // Find the nearest agent on a different team.
    // Skips `self` and same-team entries.
    // `candidates_checked` counts enemy comparisons actually performed.
    FindResult find_nearest_enemy(float x, float y, uint8_t my_team,
                                  EntityId self,
                                  uint32_t& candidates_checked) const {
        candidates_checked = 0;
        int32_t cx = to_cell(x);
        int32_t cy = to_cell(y);

        float    best_d2 = 0.0f;
        EntityId best    = {};
        bool     found   = false;

        for (int32_t ring = 0; ring <= 200; ++ring) {
            // Visit only cells on the border of this ring.
            for (int32_t dx = -ring; dx <= ring; ++dx) {
                for (int32_t dy = -ring; dy <= ring; ++dy) {
                    if (ring > 0 &&
                        std::abs(dx) < ring && std::abs(dy) < ring)
                        continue;

                    auto it = cells_.find(make_key(cx + dx, cy + dy));
                    if (it == cells_.end()) continue;

                    for (const auto& e : it->second) {
                        if (e.id == self) continue;
                        if (e.team == my_team) continue;
                        ++candidates_checked;
                        float ddx = e.x - x;
                        float ddy = e.y - y;
                        float d2  = ddx * ddx + ddy * ddy;
                        if (!found || d2 < best_d2) {
                            best_d2 = d2;
                            best    = e.id;
                            found   = true;
                        }
                    }
                }
            }

            // Early exit: the nearest point of any unsearched cell
            // (Chebyshev distance > ring) is at least ring * cell_size
            // away.  If our best find is within that distance, no
            // unsearched cell can beat it.
            if (found) {
                float fence = static_cast<float>(ring) * cell_size;
                if (best_d2 <= fence * fence) break;
            }
        }

        return {best, best_d2, found};
    }

private:
    std::unordered_map<int64_t, std::vector<Entry>> cells_;

    int32_t to_cell(float v) const {
        return static_cast<int32_t>(std::floor(v / cell_size));
    }

    static int64_t make_key(int32_t cx, int32_t cy) {
        return (static_cast<int64_t>(cx) << 32) |
               static_cast<uint32_t>(cy);
    }
};

}  // namespace de
