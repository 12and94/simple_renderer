#pragma once

#include <optional>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <vulkan/vulkan.h>

namespace vr {

class VulkanContext {
public:
    void initialize(HINSTANCE hinstance, HWND hwnd, bool enableValidationLayers);
    void shutdown();

    VkInstance instance() const { return instance_; }
    VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
    VkDevice device() const { return device_; }
    VkSurfaceKHR surface() const { return surface_; }
    VkQueue graphicsQueue() const { return graphicsQueue_; }
    VkQueue presentQueue() const { return presentQueue_; }
    uint32_t graphicsQueueFamilyIndex() const { return queueFamilyIndices_.graphicsFamily.value_or(0); }
    uint32_t presentQueueFamilyIndex() const { return queueFamilyIndices_.presentFamily.value_or(0); }

private:
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    void createInstance();
    void setupDebugMessenger();
    void createSurface(HINSTANCE hinstance, HWND hwnd);
    void pickPhysicalDevice();
    void createLogicalDevice();

    bool checkValidationLayerSupport() const;
    std::vector<const char*> getRequiredInstanceExtensions() const;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    bool isDeviceSuitable(VkPhysicalDevice device) const;

    static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData);

    bool enableValidationLayers_ = false;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    QueueFamilyIndices queueFamilyIndices_{};
};

}  // namespace vr
