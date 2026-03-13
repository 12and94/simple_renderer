#pragma once

#include <chrono>

namespace vr {

class Timer {
public:
    void reset() { last_ = clock::now(); }

    float tickSeconds() {
        const auto now = clock::now();
        const std::chrono::duration<float> delta = now - last_;
        last_ = now;
        return delta.count();
    }

private:
    using clock = std::chrono::high_resolution_clock;
    clock::time_point last_ = clock::now();
};

}  // namespace vr

