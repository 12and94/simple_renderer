#pragma once

#include <sstream>
#include <stdexcept>
#include <string_view>

namespace vr {

inline void failAssert(
    const char* expression, const char* file, int line, std::string_view message = {}) {
    std::ostringstream oss;
    oss << "Assert failed: (" << expression << ") at " << file << ':' << line;
    if (!message.empty()) {
        oss << " - " << message;
    }
    throw std::runtime_error(oss.str());
}

}  // namespace vr

#define VR_ASSERT(expr, msg)             \
    do {                                 \
        if (!(expr)) {                   \
            ::vr::failAssert(#expr, __FILE__, __LINE__, (msg)); \
        }                                \
    } while (false)

