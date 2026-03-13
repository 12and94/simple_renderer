#include "scene/Scene.h"

#include "math/Math.h"

#include <cstddef>
#include <utility>

namespace vr {

void Scene::clear() {
    nodes_.clear();
    meshes_.clear();
    materials_.clear();
    textures_.clear();
    name_ = "empty_scene";
}

void Scene::setName(std::string name) {
    name_ = std::move(name);
}

const std::string& Scene::name() const {
    return name_;
}

void Scene::updateWorldMatrices() {
    const math::Mat4 identity = math::identity();
    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (nodes_[i].parent == -1) {
            updateNodeWorldRecursive(static_cast<int>(i), identity);
        }
    }
}

void Scene::updateNodeWorldRecursive(int nodeIndex, const math::Mat4& parentWorld) {
    Node& node = nodes_[static_cast<size_t>(nodeIndex)];
    const math::Mat4 local = node.transform.localMatrix();
    node.worldMatrix = math::multiply(parentWorld, local);

    for (int childIndex : node.children) {
        if (childIndex >= 0 && static_cast<size_t>(childIndex) < nodes_.size()) {
            updateNodeWorldRecursive(childIndex, node.worldMatrix);
        }
    }
}

}  // namespace vr
