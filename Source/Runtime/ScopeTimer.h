#pragma once

#include <cstdint>

namespace de {

// RAII CPU timer using QueryPerformanceCounter.
// Writes elapsed seconds to *target on destruction.
// If accumulate is true, adds to *target instead of overwriting.
struct ScopeTimer {
    explicit ScopeTimer(double* target, bool accumulate = false);
    ~ScopeTimer();

    ScopeTimer(const ScopeTimer&) = delete;
    ScopeTimer& operator=(const ScopeTimer&) = delete;

private:
    double* target_;
    int64_t start_;
    int64_t frequency_;
    bool accumulate_;
};

} // namespace de
