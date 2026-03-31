#include "Runtime/Clock.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace de {

void Clock::init() {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    frequency_ = freq.QuadPart;

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    start_time_ = now.QuadPart;
    last_time_ = start_time_;
}

void Clock::update() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    int64_t current = now.QuadPart;
    delta_ = static_cast<double>(current - last_time_) / static_cast<double>(frequency_);
    elapsed_ = static_cast<double>(current - start_time_) / static_cast<double>(frequency_);
    last_time_ = current;
    ++frame_count_;
}

double Clock::delta_seconds() const { return delta_; }
double Clock::elapsed_seconds() const { return elapsed_; }
uint64_t Clock::frame_count() const { return frame_count_; }

} // namespace de
