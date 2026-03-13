#pragma once

#include "math/Math.h"
#include "scene/Transform.h"

#include <vector>

namespace vr {

struct Node {
    int parent = -1;
    std::vector<int> children;
    int meshIndex = -1;
    Transform transform{};
    math::Mat4 worldMatrix = math::identity();
};

}  // namespace vr

