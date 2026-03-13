#include "vulkan/Swapchain.h"

#include "core/Assert.h"
#include "core/Log.h"
#include "vulkan/VulkanContext.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace vr {

void Swapchain::initialize(VulkanContext& context, uint32_t width, uint32_t height) {
    context_ = &context;
    create(width, height);
}

void Swapchain::shutdown() {
    destroySwapchainObjects();
    context_ = nullptr;
}

void Swapchain::recreate(uint32_t width, uint32_t height) {
    VR_ASSERT(context_ != nullptr, "Swapchain must be initialized before recreate.");
    destroySwapchainObjects();
    create(width, height);
}

void Swapchain::create(uint32_t width, uint32_t height) {
    VR_ASSERT(context_ != nullptr, "Swapchain requires a valid VulkanContext.");

    const SurfaceSupportDetails support = querySurfaceSupport(context_->physicalDevice());
    if (support.formats.empty() || support.presentModes.empty()) {
        throw std::runtime_error("Swapchain surface support is incomplete.");
    }

    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
    const VkPresentModeKHR presentMode = choosePresentMode(support.presentModes);
    const VkExtent2D chosenExtent = chooseExtent(support.capabilities, width, height);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = context_->surface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = chosenExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const uint32_t queueFamilyIndices[] = {
        context_->graphicsQueueFamilyIndex(),
        context_->presentQueueFamilyIndex(),
    };

    if (context_->graphicsQueueFamilyIndex() != context_->presentQueueFamilyIndex()) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    const VkResult createResult =
        vkCreateSwapchainKHR(context_->device(), &createInfo, nullptr, &swapchain_);
    if (createResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swapchain.");
    }

    uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(context_->device(), swapchain_, &swapchainImageCount, nullptr);
    images_.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(context_->device(), swapchain_, &swapchainImageCount, images_.data());

    imageFormat_ = surfaceFormat.format;
    extent_ = chosenExtent;

    try {
        imageViews_.resize(images_.size());
        for (size_t i = 0; i < images_.size(); ++i) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = images_[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = imageFormat_;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            const VkResult viewResult =
                vkCreateImageView(context_->device(), &viewInfo, nullptr, &imageViews_[i]);
            if (viewResult != VK_SUCCESS) {
                throw std::runtime_error("Failed to create swapchain image view.");
            }
        }
    } catch (...) {
        destroySwapchainObjects();
        throw;
    }

    log::info("Swapchain created.");
}

void Swapchain::destroySwapchainObjects() {
    if (context_ == nullptr) {
        return;
    }

    for (VkImageView imageView : imageViews_) {
        if (imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(context_->device(), imageView, nullptr);
        }
    }
    imageViews_.clear();
    images_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context_->device(), swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }

    imageFormat_ = VK_FORMAT_UNDEFINED;
    extent_ = {};
}

Swapchain::SurfaceSupportDetails Swapchain::querySurfaceSupport(VkPhysicalDevice physicalDevice) const {
    SurfaceSupportDetails details{};

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        physicalDevice, context_->surface(), &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physicalDevice, context_->surface(), &formatCount, nullptr);
    if (formatCount > 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, context_->surface(), &formatCount, details.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physicalDevice, context_->surface(), &presentModeCount, nullptr);
    if (presentModeCount > 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice, context_->surface(), &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR Swapchain::chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats) const {
    for (const VkSurfaceFormatKHR& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR Swapchain::choosePresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes) const {
    for (VkPresentModeKHR presentMode : availablePresentModes) {
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::chooseExtent(
    const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) const {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    VkExtent2D actualExtent{};
    actualExtent.width = std::clamp(
        width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(
        height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actualExtent;
}

}  // namespace vr
