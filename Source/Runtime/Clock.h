#pragma once

#include <cstdint>

namespace de {

struct Clock {
    void init();
    void update();

    double delta_seconds() const;
    double elapsed_seconds() const;
    uint64_t frame_count() const;

private:
    int64_t frequency_ = 0;
    int64_t start_time_ = 0;
    int64_t last_time_ = 0;
    double delta_ = 0.0;
    double elapsed_ = 0.0;
    uint64_t frame_count_ = 0;
};

} // namespace de
