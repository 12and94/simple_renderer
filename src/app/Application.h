#pragma once

#include "app/InputState.h"
#include "app/Timer.h"
#include "render/Renderer.h"
#include "scene/Camera.h"
#include "scene/GltfLoader.h"
#include "scene/Scene.h"
#include "vulkan/VulkanContext.h"

#include <cstdint>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace vr {

class Application {
public:
    void run();

private:
    void initWindow();
    void initRenderer();
    void mainLoop();
    void cleanup();
    void handleResize(uint32_t width, uint32_t height);
    void updateCamera(float deltaTimeSeconds);

    static LRESULT CALLBACK staticWindowProc(
        HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    HINSTANCE hinstance_ = nullptr;
    HWND window_ = nullptr;
    uint32_t width_ = 1600;
    uint32_t height_ = 900;
    bool running_ = false;
    bool framebufferResized_ = false;
    InputState inputState_{};
    Timer timer_{};

    VulkanContext vulkanContext_;
    Renderer renderer_;
    GltfLoader gltfLoader_;
    Scene scene_;
    Camera camera_;
};

}  // namespace vr
