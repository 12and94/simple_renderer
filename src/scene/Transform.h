#pragma once

#include "math/Math.h"

namespace vr {

struct Transform {
    math::Vec3 translation{0.0f, 0.0f, 0.0f};
    math::Vec4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    math::Vec3 scale{1.0f, 1.0f, 1.0f};
    bool useLocalMatrixOverride = false;
    math::Mat4 localMatrixOverride = math::identity();

    math::Mat4 localMatrix() const {
        if (useLocalMatrixOverride) {
            return localMatrixOverride;
        }
        const math::Mat4 t = math::translation(translation);
        const math::Mat4 r = math::quaternionToMatrix(rotation);
        const math::Mat4 s = math::scaleMatrix(scale);
        return math::multiply(math::multiply(t, r), s);
    }
};

}  // namespace vr

