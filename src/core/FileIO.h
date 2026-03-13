#pragma once

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace vr {

inline std::vector<char> readBinaryFile(const std::filesystem::path& filePath) {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filePath.string());
    }

    const std::streamsize fileSize = file.tellg();
    if (fileSize <= 0) {
        throw std::runtime_error("File is empty: " + filePath.string());
    }

    std::vector<char> buffer(static_cast<size_t>(fileSize));
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

}  // namespace vr

