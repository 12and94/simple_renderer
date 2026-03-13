#include "vulkan/VulkanContext.h"

#include "core/Assert.h"
#include "core/Log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>

namespace vr {

namespace {

constexpr std::array<const char*, 1> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

constexpr std::array<const char*, 1> kRequiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

VkResult createDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    const VkAllocationCallbacks* allocator,
    VkDebugUtilsMessengerEXT* messenger) {
    const auto function = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (!function) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    return function(instance, createInfo, allocator, messenger);
}

void destroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* allocator) {
    const auto function = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (function) {
        function(instance, messenger, allocator);
    }
}

}  // namespace

void VulkanContext::initialize(HINSTANCE hinstance, HWND hwnd, bool enableValidationLayers) {
    enableValidationLayers_ = enableValidationLayers;

    createInstance();
    setupDebugMessenger();
    createSurface(hinstance, hwnd);
    pickPhysicalDevice();
    createLogicalDevice();
}

void VulkanContext::shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (surface_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    if (debugMessenger_ != VK_NULL_HANDLE && instance_ != VK_NULL_HANDLE) {
        destroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
        debugMessenger_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::createInstance() {
    if (enableValidationLayers_ && !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested but not available.");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Renderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Custom";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    const std::vector<const char*> extensions = getRequiredInstanceExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers_) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
    }

    const VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance.");
    }

    log::info("Vulkan instance created.");
}

void VulkanContext::setupDebugMessenger() {
    if (!enableValidationLayers_) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    const VkResult result =
        createDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debugMessenger_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create debug messenger.");
    }
}

void VulkanContext::createSurface(HINSTANCE hinstance, HWND hwnd) {
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = hinstance;
    createInfo.hwnd = hwnd;

    const VkResult result = vkCreateWin32SurfaceKHR(instance_, &createInfo, nullptr, &surface_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Win32 surface.");
    }
}

void VulkanContext::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable physical devices found.");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    int bestScore = -1;
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    QueueFamilyIndices bestIndices{};

    for (const VkPhysicalDevice device : devices) {
        if (!isDeviceSuitable(device)) {
            continue;
        }

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);

        int score = 0;
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        }
        score += static_cast<int>(properties.limits.maxImageDimension2D);

        if (score > bestScore) {
            bestScore = score;
            bestDevice = device;
            bestIndices = findQueueFamilies(device);
        }
    }

    if (bestDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable physical device.");
    }

    physicalDevice_ = bestDevice;
    queueFamilyIndices_ = bestIndices;

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &properties);
    log::info(std::string("Selected physical device: ") + properties.deviceName);
}

void VulkanContext::createLogicalDevice() {
    VR_ASSERT(queueFamilyIndices_.isComplete(), "Queue family indices are not complete.");

    const std::set<uint32_t> uniqueQueueFamilies = {
        queueFamilyIndices_.graphicsFamily.value(),
        queueFamilyIndices_.presentFamily.value(),
    };

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilies.size());

    constexpr float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(kRequiredDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kRequiredDeviceExtensions.data();

    if (enableValidationLayers_) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
    }

    const VkResult result = vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device.");
    }

    vkGetDeviceQueue(device_, queueFamilyIndices_.graphicsFamily.value(), 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, queueFamilyIndices_.presentFamily.value(), 0, &presentQueue_);

    log::info("Logical device created.");
}

bool VulkanContext::checkValidationLayerSupport() const {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* requiredLayer : kValidationLayers) {
        const auto layerFound = std::find_if(
            availableLayers.begin(),
            availableLayers.end(),
            [requiredLayer](const VkLayerProperties& layerProperties) {
                return std::strcmp(requiredLayer, layerProperties.layerName) == 0;
            });

        if (layerFound == availableLayers.end()) {
            return false;
        }
    }
    return true;
}

std::vector<const char*> VulkanContext::getRequiredInstanceExtensions() const {
    std::vector<const char*> extensions{
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };

    if (enableValidationLayers_) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

VulkanContext::QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices{};

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueCount > 0 &&
            (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
        if (queueFamilies[i].queueCount > 0 && presentSupport == VK_TRUE) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device) const {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(
        kRequiredDeviceExtensions.begin(), kRequiredDeviceExtensions.end());

    for (const VkExtensionProperties& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) const {
    const QueueFamilyIndices indices = findQueueFamilies(device);
    if (!indices.isComplete()) {
        return false;
    }

    if (!checkDeviceExtensionSupport(device)) {
        return false;
    }

    VkPhysicalDeviceFeatures supportedFeatures{};
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);
    return supportedFeatures.samplerAnisotropy == VK_TRUE;
}

void VulkanContext::populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*) {
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        log::error(callbackData->pMessage);
    } else {
        log::warn(callbackData->pMessage);
    }

    return VK_FALSE;
}

}  // namespace vr

