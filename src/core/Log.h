#pragma once

#include <iostream>
#include <string_view>

namespace vr::log {

inline void info(std::string_view message) {
    std::cout << "[INFO] " << message << '\n';
}

inline void warn(std::string_view message) {
    std::cout << "[WARN] " << message << '\n';
}

inline void error(std::string_view message) {
    std::cerr << "[ERROR] " << message << '\n';
}

}  // namespace vr::log

