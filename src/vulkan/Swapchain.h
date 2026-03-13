#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

namespace vr {

class VulkanContext;

class Swapchain {
public:
    void initialize(VulkanContext& context, uint32_t width, uint32_t height);
    void shutdown();
    void recreate(uint32_t width, uint32_t height);

    VkSwapchainKHR handle() const { return swapchain_; }
    VkFormat imageFormat() const { return imageFormat_; }
    VkExtent2D extent() const { return extent_; }
    const std::vector<VkImage>& images() const { return images_; }
    const std::vector<VkImageView>& imageViews() const { return imageViews_; }
    uint32_t width() const { return extent_.width; }
    uint32_t height() const { return extent_.height; }

private:
    struct SurfaceSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    void create(uint32_t width, uint32_t height);
    void destroySwapchainObjects();
    SurfaceSupportDetails querySurfaceSupport(VkPhysicalDevice physicalDevice) const;
    VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
    VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) const;

    VulkanContext* context_ = nullptr;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat imageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::vector<VkImageView> imageViews_;
};

}  // namespace vr

