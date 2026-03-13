#include "scene/GltfLoader.h"

#include "core/Log.h"

#if defined(VR_HAS_TINYGLTF)
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace vr {

#if defined(VR_HAS_TINYGLTF)
namespace {

double readScalar(const unsigned char* data, int componentType, bool normalized) {
    switch (componentType) {
        case TINYGLTF_COMPONENT_TYPE_BYTE: {
            const int8_t v = *reinterpret_cast<const int8_t*>(data);
            if (normalized) {
                return std::max(-1.0, static_cast<double>(v) / 127.0);
            }
            return static_cast<double>(v);
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
            const uint8_t v = *reinterpret_cast<const uint8_t*>(data);
            if (normalized) {
                return static_cast<double>(v) / 255.0;
            }
            return static_cast<double>(v);
        }
        case TINYGLTF_COMPONENT_TYPE_SHORT: {
            const int16_t v = *reinterpret_cast<const int16_t*>(data);
            if (normalized) {
                return std::max(-1.0, static_cast<double>(v) / 32767.0);
            }
            return static_cast<double>(v);
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
            const uint16_t v = *reinterpret_cast<const uint16_t*>(data);
            if (normalized) {
                return static_cast<double>(v) / 65535.0;
            }
            return static_cast<double>(v);
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
            const uint32_t v = *reinterpret_cast<const uint32_t*>(data);
            if (normalized) {
                return static_cast<double>(v) / 4294967295.0;
            }
            return static_cast<double>(v);
        }
        case TINYGLTF_COMPONENT_TYPE_FLOAT: {
            const float v = *reinterpret_cast<const float*>(data);
            return static_cast<double>(v);
        }
        default:
            return 0.0;
    }
}

const tinygltf::Accessor* findAttributeAccessor(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const char* semantic) {
    const auto it = primitive.attributes.find(semantic);
    if (it == primitive.attributes.end()) {
        return nullptr;
    }

    const int accessorIndex = it->second;
    if (accessorIndex < 0 || accessorIndex >= static_cast<int>(model.accessors.size())) {
        return nullptr;
    }
    return &model.accessors[static_cast<size_t>(accessorIndex)];
}

int textureIndexToImageIndex(const tinygltf::Model& model, int textureIndex) {
    if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size())) {
        return -1;
    }
    const tinygltf::Texture& texture = model.textures[static_cast<size_t>(textureIndex)];
    if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size())) {
        return -1;
    }
    return texture.source;
}

bool extractAccessorData(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::vector<double>& outData,
    std::string& error) {
    if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
        error = "Invalid accessor bufferView.";
        return false;
    }
    const tinygltf::BufferView& view = model.bufferViews[static_cast<size_t>(accessor.bufferView)];
    if (view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size())) {
        error = "Invalid bufferView buffer index.";
        return false;
    }
    const tinygltf::Buffer& buffer = model.buffers[static_cast<size_t>(view.buffer)];

    const size_t componentCount = static_cast<size_t>(tinygltf::GetNumComponentsInType(accessor.type));
    const size_t componentSize = static_cast<size_t>(tinygltf::GetComponentSizeInBytes(accessor.componentType));
    const size_t stride = accessor.ByteStride(view) > 0
                              ? static_cast<size_t>(accessor.ByteStride(view))
                              : componentCount * componentSize;

    const size_t startOffset = static_cast<size_t>(view.byteOffset + accessor.byteOffset);
    if (startOffset >= buffer.data.size()) {
        error = "Accessor data offset out of range.";
        return false;
    }

    outData.resize(static_cast<size_t>(accessor.count) * componentCount);
    for (size_t i = 0; i < static_cast<size_t>(accessor.count); ++i) {
        const unsigned char* elementPtr = buffer.data.data() + startOffset + i * stride;
        for (size_t c = 0; c < componentCount; ++c) {
            const unsigned char* componentPtr = elementPtr + c * componentSize;
            outData[i * componentCount + c] =
                readScalar(componentPtr, accessor.componentType, accessor.normalized);
        }
    }
    return true;
}

bool extractIndices(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    std::vector<uint32_t>& outIndices,
    std::string& error) {
    if (primitive.indices < 0) {
        return true;
    }
    if (primitive.indices >= static_cast<int>(model.accessors.size())) {
        error = "Invalid primitive indices accessor.";
        return false;
    }

    const tinygltf::Accessor& accessor = model.accessors[static_cast<size_t>(primitive.indices)];
    std::vector<double> raw;
    if (!extractAccessorData(model, accessor, raw, error)) {
        return false;
    }

    outIndices.resize(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        const double value = raw[i];
        if (value < 0.0 || value > static_cast<double>(std::numeric_limits<uint32_t>::max())) {
            error = "Index value out of uint32 range.";
            return false;
        }
        outIndices[i] = static_cast<uint32_t>(value);
    }
    return true;
}

void generateNormals(MeshPrimitive& primitive) {
    struct Vec3 {
        float x;
        float y;
        float z;
    };
    const auto sub = [](const Vec3& a, const Vec3& b) -> Vec3 {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    };
    const auto cross = [](const Vec3& a, const Vec3& b) -> Vec3 {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x,
        };
    };
    const auto normalize = [](const Vec3& v) -> Vec3 {
        const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len <= 1e-6f) {
            return {0.0f, 1.0f, 0.0f};
        }
        return {v.x / len, v.y / len, v.z / len};
    };

    std::vector<Vec3> accum(primitive.vertices.size(), {0.0f, 0.0f, 0.0f});
    for (size_t i = 0; i + 2 < primitive.indices.size(); i += 3) {
        const uint32_t i0 = primitive.indices[i + 0];
        const uint32_t i1 = primitive.indices[i + 1];
        const uint32_t i2 = primitive.indices[i + 2];
        if (i0 >= primitive.vertices.size() || i1 >= primitive.vertices.size() || i2 >= primitive.vertices.size()) {
            continue;
        }

        const Vertex& v0 = primitive.vertices[i0];
        const Vertex& v1 = primitive.vertices[i1];
        const Vertex& v2 = primitive.vertices[i2];

        const Vec3 p0{v0.position[0], v0.position[1], v0.position[2]};
        const Vec3 p1{v1.position[0], v1.position[1], v1.position[2]};
        const Vec3 p2{v2.position[0], v2.position[1], v2.position[2]};

        const Vec3 e1 = sub(p1, p0);
        const Vec3 e2 = sub(p2, p0);
        const Vec3 n = cross(e1, e2);

        accum[i0].x += n.x;
        accum[i0].y += n.y;
        accum[i0].z += n.z;
        accum[i1].x += n.x;
        accum[i1].y += n.y;
        accum[i1].z += n.z;
        accum[i2].x += n.x;
        accum[i2].y += n.y;
        accum[i2].z += n.z;
    }

    for (size_t i = 0; i < primitive.vertices.size(); ++i) {
        const Vec3 n = normalize(accum[i]);
        primitive.vertices[i].normal[0] = n.x;
        primitive.vertices[i].normal[1] = n.y;
        primitive.vertices[i].normal[2] = n.z;
    }
}

void generateTangents(MeshPrimitive& primitive, bool hasUv) {
    if (!hasUv) {
        for (Vertex& v : primitive.vertices) {
            v.tangent[0] = 1.0f;
            v.tangent[1] = 0.0f;
            v.tangent[2] = 0.0f;
            v.tangent[3] = 1.0f;
        }
        return;
    }

    struct Vec3 {
        float x;
        float y;
        float z;
    };
    const auto dot = [](const Vec3& a, const Vec3& b) -> float {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    const auto sub = [](const Vec3& a, const Vec3& b) -> Vec3 {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    };
    const auto scale = [](const Vec3& v, float s) -> Vec3 {
        return {v.x * s, v.y * s, v.z * s};
    };
    const auto add = [](const Vec3& a, const Vec3& b) -> Vec3 {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    };
    const auto cross = [](const Vec3& a, const Vec3& b) -> Vec3 {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x,
        };
    };
    const auto normalize = [](const Vec3& v) -> Vec3 {
        const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len <= 1e-6f) {
            return {1.0f, 0.0f, 0.0f};
        }
        return {v.x / len, v.y / len, v.z / len};
    };

    std::vector<Vec3> tanAccum(primitive.vertices.size(), {0.0f, 0.0f, 0.0f});
    std::vector<Vec3> bitanAccum(primitive.vertices.size(), {0.0f, 0.0f, 0.0f});

    for (size_t i = 0; i + 2 < primitive.indices.size(); i += 3) {
        const uint32_t i0 = primitive.indices[i + 0];
        const uint32_t i1 = primitive.indices[i + 1];
        const uint32_t i2 = primitive.indices[i + 2];
        if (i0 >= primitive.vertices.size() || i1 >= primitive.vertices.size() || i2 >= primitive.vertices.size()) {
            continue;
        }

        const Vertex& v0 = primitive.vertices[i0];
        const Vertex& v1 = primitive.vertices[i1];
        const Vertex& v2 = primitive.vertices[i2];

        const Vec3 p0{v0.position[0], v0.position[1], v0.position[2]};
        const Vec3 p1{v1.position[0], v1.position[1], v1.position[2]};
        const Vec3 p2{v2.position[0], v2.position[1], v2.position[2]};

        const float du1 = v1.uv0[0] - v0.uv0[0];
        const float dv1 = v1.uv0[1] - v0.uv0[1];
        const float du2 = v2.uv0[0] - v0.uv0[0];
        const float dv2 = v2.uv0[1] - v0.uv0[1];

        const float det = du1 * dv2 - dv1 * du2;
        if (std::abs(det) <= 1e-8f) {
            continue;
        }
        const float invDet = 1.0f / det;

        const Vec3 e1 = sub(p1, p0);
        const Vec3 e2 = sub(p2, p0);

        const Vec3 tangent = scale(sub(scale(e1, dv2), scale(e2, dv1)), invDet);
        const Vec3 bitangent = scale(sub(scale(e2, du1), scale(e1, du2)), invDet);

        tanAccum[i0] = add(tanAccum[i0], tangent);
        tanAccum[i1] = add(tanAccum[i1], tangent);
        tanAccum[i2] = add(tanAccum[i2], tangent);
        bitanAccum[i0] = add(bitanAccum[i0], bitangent);
        bitanAccum[i1] = add(bitanAccum[i1], bitangent);
        bitanAccum[i2] = add(bitanAccum[i2], bitangent);
    }

    for (size_t i = 0; i < primitive.vertices.size(); ++i) {
        const Vec3 n{
            primitive.vertices[i].normal[0],
            primitive.vertices[i].normal[1],
            primitive.vertices[i].normal[2]};
        Vec3 t = tanAccum[i];
        t = sub(t, scale(n, dot(n, t)));
        t = normalize(t);

        const Vec3 b = bitanAccum[i];
        const float handedness = dot(cross(n, t), b) < 0.0f ? -1.0f : 1.0f;

        primitive.vertices[i].tangent[0] = t.x;
        primitive.vertices[i].tangent[1] = t.y;
        primitive.vertices[i].tangent[2] = t.z;
        primitive.vertices[i].tangent[3] = handedness;
    }
}

}  // namespace

bool GltfLoader::loadFromFile(
    const std::filesystem::path& filePath,
    Scene& outScene,
    std::string* outError) const {
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string warning;
    std::string error;

    bool ok = false;
    const std::string extension = filePath.extension().string();
    if (extension == ".glb" || extension == ".GLB") {
        ok = loader.LoadBinaryFromFile(&model, &error, &warning, filePath.string());
    } else {
        ok = loader.LoadASCIIFromFile(&model, &error, &warning, filePath.string());
    }

    if (!warning.empty()) {
        log::warn("tinygltf warning: " + warning);
    }
    if (!ok) {
        if (outError != nullptr) {
            *outError = error.empty() ? "tinygltf failed with unknown error." : error;
        }
        return false;
    }

    outScene.clear();
    outScene.setName(filePath.stem().string());

    // Textures / images.
    auto& sceneTextures = outScene.textures();
    sceneTextures.reserve(model.images.size());
    for (size_t imageIndex = 0; imageIndex < model.images.size(); ++imageIndex) {
        const tinygltf::Image& image = model.images[imageIndex];
        TextureAsset tex{};
        tex.uri = image.uri.empty() ? ("embedded_image_" + std::to_string(imageIndex)) : image.uri;
        tex.width = image.width;
        tex.height = image.height;
        tex.component = image.component;
        tex.bits = image.bits;
        tex.pixelType = image.pixel_type;
        tex.pixels = image.image;
        sceneTextures.push_back(tex);
    }

    // Materials.
    auto& sceneMaterials = outScene.materials();
    sceneMaterials.reserve(model.materials.size());
    for (const tinygltf::Material& material : model.materials) {
        Material m{};
        if (material.pbrMetallicRoughness.baseColorFactor.size() == 4) {
            m.baseColorFactor = {
                static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[0]),
                static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[1]),
                static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[2]),
                static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[3]),
            };
        }
        m.metallicFactor = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
        m.roughnessFactor = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
        if (material.emissiveFactor.size() == 3) {
            m.emissiveFactor = {
                static_cast<float>(material.emissiveFactor[0]),
                static_cast<float>(material.emissiveFactor[1]),
                static_cast<float>(material.emissiveFactor[2]),
            };
        }
        m.baseColorTexture =
            textureIndexToImageIndex(model, material.pbrMetallicRoughness.baseColorTexture.index);
        m.metallicRoughnessTexture =
            textureIndexToImageIndex(model, material.pbrMetallicRoughness.metallicRoughnessTexture.index);
        m.normalTexture = textureIndexToImageIndex(model, material.normalTexture.index);
        m.emissiveTexture = textureIndexToImageIndex(model, material.emissiveTexture.index);
        m.doubleSided = material.doubleSided;
        sceneMaterials.push_back(m);
    }

    // Meshes.
    auto& sceneMeshes = outScene.meshes();
    sceneMeshes.reserve(model.meshes.size());
    for (const tinygltf::Mesh& mesh : model.meshes) {
        Mesh outMesh{};

        for (const tinygltf::Primitive& primitive : mesh.primitives) {
            if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                continue;
            }

            const tinygltf::Accessor* posAccessor = findAttributeAccessor(model, primitive, "POSITION");
            if (posAccessor == nullptr) {
                continue;
            }

            std::string parseError;
            std::vector<double> positions;
            if (!extractAccessorData(model, *posAccessor, positions, parseError)) {
                if (outError != nullptr) {
                    *outError = parseError;
                }
                return false;
            }
            if (posAccessor->type != TINYGLTF_TYPE_VEC3) {
                if (outError != nullptr) {
                    *outError = "POSITION accessor must be VEC3.";
                }
                return false;
            }

            MeshPrimitive outPrimitive{};
            outPrimitive.materialIndex = primitive.material;

            const size_t vertexCount = static_cast<size_t>(posAccessor->count);
            outPrimitive.vertices.resize(vertexCount);
            for (size_t i = 0; i < vertexCount; ++i) {
                Vertex& v = outPrimitive.vertices[i];
                v.position[0] = static_cast<float>(positions[i * 3 + 0]);
                v.position[1] = static_cast<float>(positions[i * 3 + 1]);
                v.position[2] = static_cast<float>(positions[i * 3 + 2]);
                v.normal[0] = 0.0f;
                v.normal[1] = 1.0f;
                v.normal[2] = 0.0f;
                v.tangent[0] = 1.0f;
                v.tangent[1] = 0.0f;
                v.tangent[2] = 0.0f;
                v.tangent[3] = 1.0f;
                v.uv0[0] = 0.0f;
                v.uv0[1] = 0.0f;
            }

            bool hasNormals = false;
            bool hasTangents = false;
            bool hasUv0 = false;

            const tinygltf::Accessor* normalAccessor = findAttributeAccessor(model, primitive, "NORMAL");
            if (normalAccessor != nullptr) {
                std::vector<double> normals;
                if (!extractAccessorData(model, *normalAccessor, normals, parseError)) {
                    if (outError != nullptr) {
                        *outError = parseError;
                    }
                    return false;
                }
                if (normalAccessor->count == posAccessor->count &&
                    normalAccessor->type == TINYGLTF_TYPE_VEC3) {
                    hasNormals = true;
                    for (size_t i = 0; i < vertexCount; ++i) {
                        outPrimitive.vertices[i].normal[0] = static_cast<float>(normals[i * 3 + 0]);
                        outPrimitive.vertices[i].normal[1] = static_cast<float>(normals[i * 3 + 1]);
                        outPrimitive.vertices[i].normal[2] = static_cast<float>(normals[i * 3 + 2]);
                    }
                }
            }

            const tinygltf::Accessor* tangentAccessor = findAttributeAccessor(model, primitive, "TANGENT");
            if (tangentAccessor != nullptr) {
                std::vector<double> tangents;
                if (!extractAccessorData(model, *tangentAccessor, tangents, parseError)) {
                    if (outError != nullptr) {
                        *outError = parseError;
                    }
                    return false;
                }
                if (tangentAccessor->count == posAccessor->count &&
                    tangentAccessor->type == TINYGLTF_TYPE_VEC4) {
                    hasTangents = true;
                    for (size_t i = 0; i < vertexCount; ++i) {
                        outPrimitive.vertices[i].tangent[0] = static_cast<float>(tangents[i * 4 + 0]);
                        outPrimitive.vertices[i].tangent[1] = static_cast<float>(tangents[i * 4 + 1]);
                        outPrimitive.vertices[i].tangent[2] = static_cast<float>(tangents[i * 4 + 2]);
                        outPrimitive.vertices[i].tangent[3] = static_cast<float>(tangents[i * 4 + 3]);
                    }
                }
            }

            const tinygltf::Accessor* uvAccessor = findAttributeAccessor(model, primitive, "TEXCOORD_0");
            if (uvAccessor != nullptr) {
                std::vector<double> uvs;
                if (!extractAccessorData(model, *uvAccessor, uvs, parseError)) {
                    if (outError != nullptr) {
                        *outError = parseError;
                    }
                    return false;
                }
                if (uvAccessor->count == posAccessor->count && uvAccessor->type == TINYGLTF_TYPE_VEC2) {
                    hasUv0 = true;
                    for (size_t i = 0; i < vertexCount; ++i) {
                        outPrimitive.vertices[i].uv0[0] = static_cast<float>(uvs[i * 2 + 0]);
                        outPrimitive.vertices[i].uv0[1] = static_cast<float>(uvs[i * 2 + 1]);
                    }
                }
            }

            if (!extractIndices(model, primitive, outPrimitive.indices, parseError)) {
                if (outError != nullptr) {
                    *outError = parseError;
                }
                return false;
            }
            if (outPrimitive.indices.empty()) {
                outPrimitive.indices.resize(vertexCount);
                for (size_t i = 0; i < vertexCount; ++i) {
                    outPrimitive.indices[i] = static_cast<uint32_t>(i);
                }
            }

            if (!hasNormals) {
                generateNormals(outPrimitive);
            }
            if (!hasTangents) {
                generateTangents(outPrimitive, hasUv0);
            }

            outMesh.primitives.push_back(std::move(outPrimitive));
        }

        sceneMeshes.push_back(std::move(outMesh));
    }

    // Nodes.
    auto& sceneNodes = outScene.nodes();
    sceneNodes.resize(model.nodes.size());
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        const tinygltf::Node& srcNode = model.nodes[i];
        Node& dstNode = sceneNodes[i];

        dstNode.meshIndex = srcNode.mesh;
        dstNode.children.reserve(srcNode.children.size());
        for (int child : srcNode.children) {
            dstNode.children.push_back(child);
        }

        if (srcNode.translation.size() == 3) {
            dstNode.transform.translation = {
                static_cast<float>(srcNode.translation[0]),
                static_cast<float>(srcNode.translation[1]),
                static_cast<float>(srcNode.translation[2]),
            };
        }
        if (srcNode.rotation.size() == 4) {
            dstNode.transform.rotation = {
                static_cast<float>(srcNode.rotation[0]),
                static_cast<float>(srcNode.rotation[1]),
                static_cast<float>(srcNode.rotation[2]),
                static_cast<float>(srcNode.rotation[3]),
            };
        }
        if (srcNode.scale.size() == 3) {
            dstNode.transform.scale = {
                static_cast<float>(srcNode.scale[0]),
                static_cast<float>(srcNode.scale[1]),
                static_cast<float>(srcNode.scale[2]),
            };
        }
        if (srcNode.matrix.size() == 16) {
            dstNode.transform.useLocalMatrixOverride = true;
            for (size_t m = 0; m < 16; ++m) {
                dstNode.transform.localMatrixOverride.m[m] = static_cast<float>(srcNode.matrix[m]);
            }
        }
    }

    for (size_t i = 0; i < sceneNodes.size(); ++i) {
        for (int child : sceneNodes[i].children) {
            if (child >= 0 && static_cast<size_t>(child) < sceneNodes.size()) {
                sceneNodes[static_cast<size_t>(child)].parent = static_cast<int>(i);
            }
        }
    }

    outScene.updateWorldMatrices();

    log::info(
        "glTF loaded: meshes=" + std::to_string(outScene.meshes().size()) +
        ", nodes=" + std::to_string(outScene.nodes().size()) +
        ", materials=" + std::to_string(outScene.materials().size()) +
        ", textures=" + std::to_string(outScene.textures().size()));

    return true;
}

#else

bool GltfLoader::loadFromFile(
    const std::filesystem::path& filePath,
    Scene& outScene,
    std::string* outError) const {
    (void)filePath;
    (void)outScene;
    if (outError != nullptr) {
        *outError =
            "tinygltf is disabled. Reconfigure CMake with VR_ENABLE_TINYGLTF=ON and VR_TINYGLTF_ROOT set.";
    }
    log::warn("GltfLoader disabled: tinygltf is not enabled.");
    return false;
}

#endif

}  // namespace vr
