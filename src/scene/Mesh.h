#pragma once

#include <cstdint>
#include <vector>

namespace vr {

struct Vertex {
    float position[3];
    float normal[3];
    float tangent[4];
    float uv0[2];
};

struct MeshPrimitive {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    int materialIndex = -1;
};

struct Mesh {
    std::vector<MeshPrimitive> primitives;
};

}  // namespace vr
