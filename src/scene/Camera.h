#pragma once

namespace vr {

struct Camera {
    float position[3] = {0.0f, 0.0f, 3.0f};
    float yawDegrees = -90.0f;
    float pitchDegrees = 0.0f;
    float fovDegrees = 60.0f;
    float moveSpeed = 4.0f;
    float mouseSensitivity = 0.12f;
};

}  // namespace vr
