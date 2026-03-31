#include "Runtime/ScopeTimer.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace de {

ScopeTimer::ScopeTimer(double* target, bool accumulate)
    : target_(target), accumulate_(accumulate) {
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    frequency_ = freq.QuadPart;

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    start_ = now.QuadPart;
}

ScopeTimer::~ScopeTimer() {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    double elapsed = static_cast<double>(now.QuadPart - start_)
                   / static_cast<double>(frequency_);

    if (accumulate_) {
        *target_ += elapsed;
    } else {
        *target_ = elapsed;
    }
}

} // namespace de
