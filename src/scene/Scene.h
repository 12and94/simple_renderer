#pragma once

#include "scene/Material.h"
#include "scene/Mesh.h"
#include "scene/Node.h"

#include <cstdint>
#include <string>
#include <vector>

namespace vr {

struct TextureAsset {
    std::string uri;
    int width = 0;
    int height = 0;
    int component = 0;
    int bits = 0;
    int pixelType = 0;
    std::vector<uint8_t> pixels;
};

class Scene {
public:
    void clear();
    void setName(std::string name);
    const std::string& name() const;

    std::vector<Node>& nodes() { return nodes_; }
    const std::vector<Node>& nodes() const { return nodes_; }

    std::vector<Mesh>& meshes() { return meshes_; }
    const std::vector<Mesh>& meshes() const { return meshes_; }

    std::vector<Material>& materials() { return materials_; }
    const std::vector<Material>& materials() const { return materials_; }

    std::vector<TextureAsset>& textures() { return textures_; }
    const std::vector<TextureAsset>& textures() const { return textures_; }

    void updateWorldMatrices();

private:
    void updateNodeWorldRecursive(int nodeIndex, const math::Mat4& parentWorld);

    std::string name_ = "empty_scene";
    std::vector<Node> nodes_;
    std::vector<Mesh> meshes_;
    std::vector<Material> materials_;
    std::vector<TextureAsset> textures_;
};

}  // namespace vr
