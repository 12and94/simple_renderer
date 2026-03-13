#include "app/Application.h"

#include "core/Log.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <windowsx.h>

namespace vr {

namespace {
constexpr wchar_t kWindowClassName[] = L"VulkanRendererWindowClass";
constexpr wchar_t kWindowTitle[] = L"Vulkan Renderer";
constexpr float kMinMoveSpeed = 0.25f;
constexpr float kMaxMoveSpeed = 50.0f;
constexpr float kMinPitch = -89.0f;
constexpr float kMaxPitch = 89.0f;
constexpr float kMaxDtSeconds = 0.05f;
}

void Application::run() {
    try {
        initWindow();
        initRenderer();
        mainLoop();
        cleanup();
    } catch (...) {
        cleanup();
        throw;
    }
}

void Application::initWindow() {
    hinstance_ = GetModuleHandleW(nullptr);
    if (!hinstance_) {
        throw std::runtime_error("GetModuleHandleW failed.");
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &Application::staticWindowProc;
    wc.hInstance = hinstance_;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;

    if (!RegisterClassExW(&wc)) {
        throw std::runtime_error("RegisterClassExW failed.");
    }

    RECT windowRect{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    window_ = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        hinstance_,
        this);

    if (!window_) {
        throw std::runtime_error("CreateWindowExW failed.");
    }

    ShowWindow(window_, SW_SHOW);
    UpdateWindow(window_);
}

void Application::initRenderer() {
    bool enableValidation = false;
#if defined(VR_ENABLE_VALIDATION) && defined(VR_DEBUG_BUILD)
    enableValidation = true;
#endif

    vulkanContext_.initialize(hinstance_, window_, enableValidation);
    renderer_.initialize(vulkanContext_, width_, height_);
    scene_.setName("startup_scene");

    const std::filesystem::path candidateScenePaths[] = {
        std::filesystem::path("assets/models/DamageHelmet/glTF-Binary/DamagedHelmet.glb"),
        std::filesystem::path("assets/models/DamagedHelmet/glTF-Binary/DamagedHelmet.glb"),
        std::filesystem::path("assets/models/DamagedHelmet/glTF/DamagedHelmet.gltf"),
    };

    bool sceneLoaded = false;
    for (const auto& candidate : candidateScenePaths) {
        if (!std::filesystem::exists(candidate)) {
            continue;
        }

        std::string error;
        if (!gltfLoader_.loadFromFile(candidate, scene_, &error)) {
            log::warn("Failed to load candidate glTF: " + candidate.string() + " - " + error);
            continue;
        }

        log::info("Loaded glTF scene: " + candidate.string());
        sceneLoaded = true;
        break;
    }

    if (!sceneLoaded) {
        log::info("No default glTF scene found, using procedural test mesh.");
    }

    renderer_.setScene(scene_);
}

void Application::mainLoop() {
    running_ = true;
    timer_.reset();

    while (running_) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running_ = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        if (!running_) {
            break;
        }

        if (framebufferResized_) {
            handleResize(width_, height_);
            framebufferResized_ = false;
        }

        const float dt = std::min(timer_.tickSeconds(), kMaxDtSeconds);
        updateCamera(dt);
        renderer_.renderFrame(scene_, camera_);
        inputState_.resetFrameDeltas();
        Sleep(1);
    }
}

void Application::cleanup() {
    renderer_.shutdown();
    vulkanContext_.shutdown();

    if (window_) {
        DestroyWindow(window_);
        window_ = nullptr;
    }

    if (hinstance_) {
        UnregisterClassW(kWindowClassName, hinstance_);
        hinstance_ = nullptr;
    }
}

void Application::handleResize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }
    renderer_.resize(width, height);
    std::string message = "Window resized to " + std::to_string(width) + "x" + std::to_string(height);
    log::info(message);
}

void Application::updateCamera(float deltaTimeSeconds) {
    if (inputState_.rightMouseDown) {
        camera_.yawDegrees += static_cast<float>(inputState_.mouseDeltaX) * camera_.mouseSensitivity;
        camera_.pitchDegrees -= static_cast<float>(inputState_.mouseDeltaY) * camera_.mouseSensitivity;
        camera_.pitchDegrees = std::clamp(camera_.pitchDegrees, kMinPitch, kMaxPitch);
    }

    if (std::abs(inputState_.scrollDelta) > 0.001f) {
        const float scale = 1.0f + inputState_.scrollDelta * 0.1f;
        camera_.moveSpeed =
            std::clamp(camera_.moveSpeed * std::max(scale, 0.1f), kMinMoveSpeed, kMaxMoveSpeed);
    }

    const float yawRad = camera_.yawDegrees * 0.017453292519943295769f;
    const float pitchRad = camera_.pitchDegrees * 0.017453292519943295769f;

    const float cosPitch = std::cos(pitchRad);
    const float sinPitch = std::sin(pitchRad);
    const float cosYaw = std::cos(yawRad);
    const float sinYaw = std::sin(yawRad);

    float forwardX = cosYaw * cosPitch;
    float forwardY = sinPitch;
    float forwardZ = sinYaw * cosPitch;
    const float forwardLen = std::sqrt(
        forwardX * forwardX + forwardY * forwardY + forwardZ * forwardZ);
    if (forwardLen > 1e-6f) {
        forwardX /= forwardLen;
        forwardY /= forwardLen;
        forwardZ /= forwardLen;
    }

    float rightX = forwardZ;
    float rightY = 0.0f;
    float rightZ = -forwardX;
    const float rightLen = std::sqrt(rightX * rightX + rightY * rightY + rightZ * rightZ);
    if (rightLen > 1e-6f) {
        rightX /= rightLen;
        rightY /= rightLen;
        rightZ /= rightLen;
    }

    float speed = camera_.moveSpeed;
    if (inputState_.keys[VK_SHIFT]) {
        speed *= 2.0f;
    }
    const float move = speed * deltaTimeSeconds;

    if (inputState_.keys['W']) {
        camera_.position[0] += forwardX * move;
        camera_.position[1] += forwardY * move;
        camera_.position[2] += forwardZ * move;
    }
    if (inputState_.keys['S']) {
        camera_.position[0] -= forwardX * move;
        camera_.position[1] -= forwardY * move;
        camera_.position[2] -= forwardZ * move;
    }
    if (inputState_.keys['A']) {
        camera_.position[0] -= rightX * move;
        camera_.position[1] -= rightY * move;
        camera_.position[2] -= rightZ * move;
    }
    if (inputState_.keys['D']) {
        camera_.position[0] += rightX * move;
        camera_.position[1] += rightY * move;
        camera_.position[2] += rightZ * move;
    }
    if (inputState_.keys['Q']) {
        camera_.position[1] -= move;
    }
    if (inputState_.keys['E']) {
        camera_.position[1] += move;
    }
}

LRESULT CALLBACK Application::staticWindowProc(
    HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        const auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        auto* app = reinterpret_cast<Application*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }

    auto* app = reinterpret_cast<Application*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (app) {
        return app->windowProc(hwnd, message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT Application::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_KEYDOWN:
            if (wParam < inputState_.keys.size()) {
                inputState_.keys[static_cast<size_t>(wParam)] = true;
            }
            return 0;
        case WM_KEYUP:
            if (wParam < inputState_.keys.size()) {
                inputState_.keys[static_cast<size_t>(wParam)] = false;
            }
            return 0;
        case WM_RBUTTONDOWN:
            inputState_.rightMouseDown = true;
            inputState_.hasLastMousePos = false;
            SetCapture(hwnd);
            return 0;
        case WM_RBUTTONUP:
            inputState_.rightMouseDown = false;
            inputState_.hasLastMousePos = false;
            ReleaseCapture();
            return 0;
        case WM_MOUSEMOVE: {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            if (inputState_.rightMouseDown) {
                if (inputState_.hasLastMousePos) {
                    inputState_.mouseDeltaX += x - inputState_.lastMouseX;
                    inputState_.mouseDeltaY += y - inputState_.lastMouseY;
                }
                inputState_.lastMouseX = x;
                inputState_.lastMouseY = y;
                inputState_.hasLastMousePos = true;
            }
            return 0;
        }
        case WM_MOUSEWHEEL:
            inputState_.scrollDelta += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
                                       static_cast<float>(WHEEL_DELTA);
            return 0;
        case WM_SIZE: {
            width_ = static_cast<uint32_t>(LOWORD(lParam));
            height_ = static_cast<uint32_t>(HIWORD(lParam));
            framebufferResized_ = true;
            return 0;
        }
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

}  // namespace vr
