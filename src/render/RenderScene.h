#pragma once

#include "math/Math.h"
#include "scene/Mesh.h"
#include "scene/Scene.h"

#include <cstdint>
#include <vector>

namespace vr {

struct RenderDrawItem {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    int32_t vertexOffset = 0;
    math::Mat4 modelMatrix = math::identity();
    int materialIndex = -1;
    math::Vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    int baseColorTextureIndex = -1;
    int normalTextureIndex = -1;
    int metallicRoughnessTextureIndex = -1;
    int emissiveTextureIndex = -1;
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    math::Vec3 emissiveFactor{0.0f, 0.0f, 0.0f};
};

class RenderScene {
public:
    void clear();
    void buildFromScene(const Scene& scene);

    bool hasGeometry() const { return !drawItems_.empty(); }
    const std::vector<Vertex>& vertices() const { return vertices_; }
    const std::vector<uint32_t>& indices() const { return indices_; }
    const std::vector<RenderDrawItem>& drawItems() const { return drawItems_; }

private:
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    std::vector<RenderDrawItem> drawItems_;
};

}  // namespace vr
