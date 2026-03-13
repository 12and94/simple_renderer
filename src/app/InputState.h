#pragma once

#include <array>

namespace vr {

struct InputState {
    std::array<bool, 256> keys{};
    bool rightMouseDown = false;
    bool hasLastMousePos = false;
    int lastMouseX = 0;
    int lastMouseY = 0;
    int mouseDeltaX = 0;
    int mouseDeltaY = 0;
    float scrollDelta = 0.0f;

    void resetFrameDeltas() {
        mouseDeltaX = 0;
        mouseDeltaY = 0;
        scrollDelta = 0.0f;
    }
};

}  // namespace vr

