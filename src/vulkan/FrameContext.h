#pragma once

#include <vulkan/vulkan.h>

namespace vr {

struct FrameContext {
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer primaryCommandBuffer = VK_NULL_HANDLE;
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;
};

}  // namespace vr

