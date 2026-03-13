#pragma once

#include "math/Math.h"

namespace vr {

struct Material {
    math::Vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    math::Vec3 emissiveFactor{0.0f, 0.0f, 0.0f};
    int baseColorTexture = -1;
    int normalTexture = -1;
    int metallicRoughnessTexture = -1;
    int emissiveTexture = -1;
    bool doubleSided = false;
};

}  // namespace vr

