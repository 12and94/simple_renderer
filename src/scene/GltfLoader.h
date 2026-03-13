#pragma once

#include "scene/Scene.h"

#include <filesystem>
#include <string>

namespace vr {

class GltfLoader {
public:
    bool loadFromFile(
        const std::filesystem::path& filePath,
        Scene& outScene,
        std::string* outError = nullptr) const;
};

}  // namespace vr

