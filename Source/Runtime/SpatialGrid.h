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

    void clear() {
        cells_.clear();
        has_bounds_ = false;
    }

    void insert(EntityId id, uint8_t team, float x, float y) {
        int32_t cx = to_cell(x);
        int32_t cy = to_cell(y);
        cells_[make_key(cx, cy)].push_back({id, team, x, y});
        if (!has_bounds_) {
            min_cx_ = max_cx_ = cx;
            min_cy_ = max_cy_ = cy;
            has_bounds_ = true;
        } else {
            if (cx < min_cx_) min_cx_ = cx;
            if (cx > max_cx_) max_cx_ = cx;
            if (cy < min_cy_) min_cy_ = cy;
            if (cy > max_cy_) max_cy_ = cy;
        }
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

        // Compute the maximum ring needed to cover all occupied cells.
        // Chebyshev distance from query cell to the farthest occupied cell.
        int32_t max_ring = 0;
        if (has_bounds_) {
            int32_t d0 = std::max(std::abs(cx - min_cx_),
                                  std::abs(cx - max_cx_));
            int32_t d1 = std::max(std::abs(cy - min_cy_),
                                  std::abs(cy - max_cy_));
            max_ring = std::max(d0, d1);
        }

        // Helper: scan one cell and update best.
        auto scan_cell = [&](int32_t gx, int32_t gy) {
            auto it = cells_.find(make_key(gx, gy));
            if (it == cells_.end()) return;
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
        };

        for (int32_t ring = 0; ring <= max_ring; ++ring) {
            if (ring == 0) {
                scan_cell(cx, cy);
            } else {
                // Iterate the 4 sides of the ring border directly.
                for (int32_t d = -ring; d <= ring; ++d) {
                    scan_cell(cx + d, cy - ring);  // top row
                    scan_cell(cx + d, cy + ring);  // bottom row
                }
                for (int32_t d = -ring + 1; d <= ring - 1; ++d) {
                    scan_cell(cx - ring, cy + d);  // left column
                    scan_cell(cx + ring, cy + d);  // right column
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

    // Call fn(entry, dist2) for every entry within `radius` of (x,y),
    // excluding `self`.  Returns the number of entries visited.
    template<typename Fn>
    uint32_t for_each_nearby(float x, float y, float radius,
                             EntityId self, Fn&& fn) const {
        int32_t cx = to_cell(x);
        int32_t cy = to_cell(y);
        int32_t cr = static_cast<int32_t>(std::ceil(radius / cell_size));
        float r2 = radius * radius;
        uint32_t visited = 0;
        for (int32_t dx = -cr; dx <= cr; ++dx) {
            for (int32_t dy = -cr; dy <= cr; ++dy) {
                auto it = cells_.find(make_key(cx + dx, cy + dy));
                if (it == cells_.end()) continue;
                for (const auto& e : it->second) {
                    if (e.id == self) continue;
                    float ddx = e.x - x;
                    float ddy = e.y - y;
                    float d2 = ddx * ddx + ddy * ddy;
                    if (d2 <= r2) {
                        fn(e, d2);
                        ++visited;
                    }
                }
            }
        }
        return visited;
    }

private:
    std::unordered_map<int64_t, std::vector<Entry>> cells_;
    int32_t min_cx_ = 0, max_cx_ = 0;
    int32_t min_cy_ = 0, max_cy_ = 0;
    bool    has_bounds_ = false;

    int32_t to_cell(float v) const {
        return static_cast<int32_t>(std::floor(v / cell_size));
    }

    static int64_t make_key(int32_t cx, int32_t cy) {
        return (static_cast<int64_t>(cx) << 32) |
               static_cast<uint32_t>(cy);
    }
};

}  // namespace de
