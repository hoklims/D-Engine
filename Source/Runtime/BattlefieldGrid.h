#pragma once

// Static 2D grid for battlefield navigation.
//
// Cells are either FREE or BLOCKED.  An integration field stores the
// shortest-distance cost from every free cell to a goal cell, computed
// via BFS (uniform-cost because all cells have equal traversal cost).
// Agents sample a flow direction from the field instead of pointing
// straight at their strategic goal.
//
// NOT part of the generic ECS -- this is a crowd-runtime utility.

#include <cmath>
#include <cstdint>
#include <vector>

namespace de {

struct BattlefieldGrid {

    enum Cell : uint8_t { FREE = 0, BLOCKED = 1 };

    // Unreachable cost sentinel (> any valid BFS distance).
    static constexpr float k_unreachable = 1e9f;

    // -- Setup ---------------------------------------------------------------

    // Allocate a grid of `w` x `h` cells covering a world-space rectangle.
    // Origin is the bottom-left corner; each cell is `cell_size` wide/tall.
    // All cells start FREE.
    void init(int w, int h, float cell_size, float origin_x, float origin_y) {
        width_      = w;
        height_     = h;
        cell_size_  = cell_size;
        origin_x_   = origin_x;
        origin_y_   = origin_y;
        cells_.assign(static_cast<size_t>(w) * h, FREE);
        cost_.assign(static_cast<size_t>(w) * h, k_unreachable);
        flow_x_.assign(static_cast<size_t>(w) * h, 0.0f);
        flow_y_.assign(static_cast<size_t>(w) * h, 0.0f);
    }

    void set_blocked(int cx, int cy) {
        if (in_bounds(cx, cy)) cells_[idx(cx, cy)] = BLOCKED;
    }

    void set_free(int cx, int cy) {
        if (in_bounds(cx, cy)) cells_[idx(cx, cy)] = FREE;
    }

    bool is_blocked(int cx, int cy) const {
        return !in_bounds(cx, cy) || cells_[idx(cx, cy)] == BLOCKED;
    }

    int  width()     const { return width_; }
    int  height()    const { return height_; }
    float cell_size() const { return cell_size_; }

    uint32_t blocked_count() const {
        uint32_t n = 0;
        for (auto c : cells_) if (c == BLOCKED) ++n;
        return n;
    }

    // -- Integration field ---------------------------------------------------

    // Compute shortest-distance field toward a world-space goal.
    // Uses BFS on the 4-connected grid (uniform cost = 1 per step).
    // After this call, flow vectors point toward the goal along the
    // shortest obstacle-free path.
    //
    // Returns the number of reachable cells.
    uint32_t build_integration_field(float goal_world_x, float goal_world_y) {
        const size_t total = static_cast<size_t>(width_) * height_;
        for (size_t i = 0; i < total; ++i) {
            cost_[i]   = k_unreachable;
            flow_x_[i] = 0.0f;
            flow_y_[i] = 0.0f;
        }

        int gx, gy;
        world_to_cell(goal_world_x, goal_world_y, gx, gy);

        // Clamp goal to grid bounds.
        gx = clamp(gx, 0, width_  - 1);
        gy = clamp(gy, 0, height_ - 1);

        // If the goal cell itself is blocked, find nearest free cell.
        if (cells_[idx(gx, gy)] == BLOCKED) {
            if (!find_nearest_free(gx, gy, gx, gy)) return 0;
        }

        // BFS wavefront.
        bfs_queue_.clear();
        cost_[idx(gx, gy)] = 0.0f;
        bfs_queue_.push_back({gx, gy});

        uint32_t reachable = 1;
        size_t head = 0;
        while (head < bfs_queue_.size()) {
            auto [cx, cy] = bfs_queue_[head++];
            float next_cost = cost_[idx(cx, cy)] + 1.0f;

            static constexpr int dx[4] = {1, -1, 0,  0};
            static constexpr int dy[4] = {0,  0, 1, -1};
            for (int d = 0; d < 4; ++d) {
                int nx = cx + dx[d];
                int ny = cy + dy[d];
                if (!in_bounds(nx, ny)) continue;
                size_t ni = idx(nx, ny);
                if (cells_[ni] == BLOCKED) continue;
                if (cost_[ni] <= next_cost) continue;
                cost_[ni] = next_cost;
                bfs_queue_.push_back({nx, ny});
                ++reachable;
            }
        }

        // Build flow vectors: each cell points toward its lowest-cost neighbor.
        for (int cy = 0; cy < height_; ++cy) {
            for (int cx = 0; cx < width_; ++cx) {
                size_t i = idx(cx, cy);
                if (cells_[i] == BLOCKED) continue;
                if (cost_[i] >= k_unreachable) continue;

                float best = cost_[i];
                float fx = 0.0f;
                float fy = 0.0f;

                static constexpr int ddx[4] = {1, -1, 0,  0};
                static constexpr int ddy[4] = {0,  0, 1, -1};
                for (int d = 0; d < 4; ++d) {
                    int nx = cx + ddx[d];
                    int ny = cy + ddy[d];
                    if (!in_bounds(nx, ny)) continue;
                    float nc = cost_[idx(nx, ny)];
                    if (nc < best) {
                        best = nc;
                        fx = static_cast<float>(ddx[d]);
                        fy = static_cast<float>(ddy[d]);
                    }
                }
                flow_x_[i] = fx;
                flow_y_[i] = fy;
            }
        }

        return reachable;
    }

    // -- Queries -------------------------------------------------------------

    // Sample the flow direction at a world-space position.
    // Returns false if the position is out of bounds or unreachable.
    bool sample_flow(float wx, float wy, float& out_dx, float& out_dy) const {
        int cx, cy;
        world_to_cell(wx, wy, cx, cy);
        if (!in_bounds(cx, cy)) { out_dx = 0.0f; out_dy = 0.0f; return false; }
        size_t i = idx(cx, cy);
        if (cost_[i] >= k_unreachable) { out_dx = 0.0f; out_dy = 0.0f; return false; }
        out_dx = flow_x_[i];
        out_dy = flow_y_[i];
        return true;
    }

    // Cost at a world-space position (k_unreachable if blocked/out-of-bounds).
    float cost_at(float wx, float wy) const {
        int cx, cy;
        world_to_cell(wx, wy, cx, cy);
        if (!in_bounds(cx, cy)) return k_unreachable;
        return cost_[idx(cx, cy)];
    }

    // Convert world position to cell coordinates.
    void world_to_cell(float wx, float wy, int& cx, int& cy) const {
        cx = static_cast<int>(std::floor((wx - origin_x_) / cell_size_));
        cy = static_cast<int>(std::floor((wy - origin_y_) / cell_size_));
    }

    // Convert cell center to world position.
    void cell_to_world(int cx, int cy, float& wx, float& wy) const {
        wx = origin_x_ + (static_cast<float>(cx) + 0.5f) * cell_size_;
        wy = origin_y_ + (static_cast<float>(cy) + 0.5f) * cell_size_;
    }

private:
    int   width_     = 0;
    int   height_    = 0;
    float cell_size_ = 1.0f;
    float origin_x_  = 0.0f;
    float origin_y_  = 0.0f;

    std::vector<Cell>  cells_;
    std::vector<float> cost_;
    std::vector<float> flow_x_;
    std::vector<float> flow_y_;

    struct CellCoord { int x, y; };
    std::vector<CellCoord> bfs_queue_;  // reused across builds

    bool in_bounds(int cx, int cy) const {
        return cx >= 0 && cx < width_ && cy >= 0 && cy < height_;
    }

    size_t idx(int cx, int cy) const {
        return static_cast<size_t>(cy) * width_ + cx;
    }

    static int clamp(int v, int lo, int hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    }

    // Spiral search for nearest free cell from (sx, sy).
    bool find_nearest_free(int sx, int sy, int& out_x, int& out_y) const {
        int max_ring = width_ > height_ ? width_ : height_;
        for (int r = 1; r <= max_ring; ++r) {
            for (int d = -r; d <= r; ++d) {
                int candidates[4][2] = {
                    {sx + d, sy - r}, {sx + d, sy + r},
                    {sx - r, sy + d}, {sx + r, sy + d}
                };
                for (auto& c : candidates) {
                    if (in_bounds(c[0], c[1]) &&
                        cells_[idx(c[0], c[1])] == FREE) {
                        out_x = c[0]; out_y = c[1];
                        return true;
                    }
                }
            }
        }
        return false;
    }
};

}  // namespace de
