#include "render/RenderScene.h"

#include <cstddef>

namespace vr {

void RenderScene::clear() {
    vertices_.clear();
    indices_.clear();
    drawItems_.clear();
}

void RenderScene::buildFromScene(const Scene& scene) {
    clear();

    const auto& sceneNodes = scene.nodes();
    const auto& sceneMeshes = scene.meshes();
    const auto& sceneMaterials = scene.materials();
    if (sceneNodes.empty() || sceneMeshes.empty()) {
        return;
    }

    for (const Node& node : sceneNodes) {
        if (node.meshIndex < 0 || static_cast<size_t>(node.meshIndex) >= sceneMeshes.size()) {
            continue;
        }

        const Mesh& mesh = sceneMeshes[static_cast<size_t>(node.meshIndex)];
        for (const MeshPrimitive& primitive : mesh.primitives) {
            if (primitive.vertices.empty() || primitive.indices.empty()) {
                continue;
            }

            const uint32_t firstIndex = static_cast<uint32_t>(indices_.size());
            const int32_t vertexOffset = static_cast<int32_t>(vertices_.size());

            vertices_.insert(vertices_.end(), primitive.vertices.begin(), primitive.vertices.end());
            indices_.reserve(indices_.size() + primitive.indices.size());
            for (uint32_t index : primitive.indices) {
                indices_.push_back(index + static_cast<uint32_t>(vertexOffset));
            }

            RenderDrawItem item{};
            item.firstIndex = firstIndex;
            item.indexCount = static_cast<uint32_t>(primitive.indices.size());
            item.vertexOffset = 0;
            item.modelMatrix = node.worldMatrix;
            item.materialIndex = primitive.materialIndex;
            if (item.materialIndex >= 0 &&
                static_cast<size_t>(item.materialIndex) < sceneMaterials.size()) {
                const Material& material = sceneMaterials[static_cast<size_t>(item.materialIndex)];
                item.baseColorFactor = material.baseColorFactor;
                item.baseColorTextureIndex = material.baseColorTexture;
                item.normalTextureIndex = material.normalTexture;
                item.metallicRoughnessTextureIndex = material.metallicRoughnessTexture;
                item.emissiveTextureIndex = material.emissiveTexture;
                item.metallicFactor = material.metallicFactor;
                item.roughnessFactor = material.roughnessFactor;
                item.emissiveFactor = material.emissiveFactor;
            }
            drawItems_.push_back(item);
        }
    }
}

}  // namespace vr
