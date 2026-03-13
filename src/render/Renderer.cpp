#include "render/Renderer.h"

#include "core/Assert.h"
#include "core/FileIO.h"
#include "core/Log.h"
#include "math/Math.h"
#include "vulkan/VulkanContext.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace vr {

namespace {

struct DrawPushConstants {
    float mvp[16];
    float baseColor[4];
    float emissiveFactor[4];
    float materialParams[4];
};

std::filesystem::path executableDirectory() {
    std::array<char, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        throw std::runtime_error("GetModuleFileNameA failed.");
    }
    return std::filesystem::path(buffer.data()).parent_path();
}

std::filesystem::path resolveShaderPath(const std::string& fileName) {
    const std::filesystem::path exeDir = executableDirectory();
    const std::vector<std::filesystem::path> candidates = {
        exeDir / ".." / "shaders" / fileName,
        std::filesystem::current_path() / "build" / "shaders" / fileName,
        std::filesystem::current_path() / "shaders" / fileName,
    };

    for (const std::filesystem::path& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return std::filesystem::weakly_canonical(candidate);
        }
    }

    throw std::runtime_error("Shader file not found: " + fileName);
}

std::vector<Vertex> cubeVertices() {
    return {
        {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {1.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {1.0f, 1.0f}},
        {{1.0f, -1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {0.0f, 1.0f}},
        {{-1.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
        {{-1.0f, 1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{-1.0f, 1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f, -1.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
        {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{1.0f, 1.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{-1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{1.0f, -1.0f, -1.0f}, {0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{1.0f, -1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
        {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
        {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
        {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
        {{1.0f, -1.0f, -1.0f}, {0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    };
}

std::vector<uint32_t> cubeIndices() {
    return {
        0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11,
        12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23,
    };
}

}  // namespace

void Renderer::initialize(VulkanContext& context, uint32_t width, uint32_t height) {
    context_ = &context;
    framebufferWidth_ = width;
    framebufferHeight_ = height;
    currentFrameIndex_ = 0;
    frameCounter_ = 0;

    try {
        swapchain_.initialize(*context_, framebufferWidth_, framebufferHeight_);
        createPersistentResources();
        createPerFrameResources();
        createSwapchainDependentResources();
        imagesInFlight_.assign(swapchain_.images().size(), VK_NULL_HANDLE);
        isInitialized_ = true;
    } catch (...) {
        shutdown();
        throw;
    }

    log::info("Renderer initialized.");
}

void Renderer::shutdown() {
    if (context_ == nullptr) {
        return;
    }

    if (context_->device() != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(context_->device());
    }

    destroySwapchainDependentResources();
    destroyPerFrameResources();
    destroyPersistentResources();
    swapchain_.shutdown();
    imagesInFlight_.clear();

    context_ = nullptr;
    isInitialized_ = false;
    currentFrameIndex_ = 0;
    frameCounter_ = 0;
}

void Renderer::renderFrame(const Scene& scene, const Camera& camera) {
    VR_ASSERT(isInitialized_, "Renderer must be initialized before renderFrame.");
    (void)scene;

    if (framebufferWidth_ == 0 || framebufferHeight_ == 0) {
        return;
    }

    FrameContext& frame = frameContexts_[currentFrameIndex_];
    const VkDevice device = context_->device();
    vkWaitForFences(device, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        device, swapchain_.handle(), UINT64_MAX, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain(framebufferWidth_, framebufferHeight_);
        return;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image.");
    }

    if (imagesInFlight_[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &imagesInFlight_[imageIndex], VK_TRUE, UINT64_MAX);
    }
    imagesInFlight_[imageIndex] = frame.inFlightFence;

    math::Mat4 viewProjection{};
    computeViewProjection(camera, viewProjection);

    vkResetFences(device, 1, &frame.inFlightFence);
    vkResetCommandPool(device, frame.commandPool, 0);
    recordCommandBuffer(frame.primaryCommandBuffer, imageIndex, viewProjection.m.data());

    const VkSemaphore waitSemaphores[] = {frame.imageAvailableSemaphore};
    const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    const VkSemaphore signalSemaphores[] = {frame.renderFinishedSemaphore};

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.primaryCommandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    if (vkQueueSubmit(context_->graphicsQueue(), 1, &submitInfo, frame.inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer.");
    }

    VkSwapchainKHR swapchains[] = {swapchain_.handle()};
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    const VkResult presentResult = vkQueuePresentKHR(context_->presentQueue(), &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain(framebufferWidth_, framebufferHeight_);
    } else if (presentResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image.");
    }

    currentFrameIndex_ = (currentFrameIndex_ + 1) % kMaxFramesInFlight;
    ++frameCounter_;
}

void Renderer::resize(uint32_t width, uint32_t height) {
    framebufferWidth_ = width;
    framebufferHeight_ = height;
    if (width == 0 || height == 0) {
        return;
    }
    recreateSwapchain(width, height);
}

void Renderer::setScene(const Scene& scene) {
    renderScene_.buildFromScene(scene);
    if (renderScene_.hasGeometry()) {
        uploadGeometry(renderScene_.vertices(), renderScene_.indices());
        activeDrawItems_ = renderScene_.drawItems();
        createMaterialResources(scene);
        usingFallbackGeometry_ = false;
        log::info(
            "Renderer scene uploaded: draws=" + std::to_string(activeDrawItems_.size()) +
            ", vertices=" + std::to_string(renderScene_.vertices().size()) +
            ", indices=" + std::to_string(renderScene_.indices().size()));
    } else {
        buildFallbackGeometry();
        log::warn("Scene contains no drawable geometry, fallback mesh is active.");
    }
}

void Renderer::createPersistentResources() {
    createUploadCommandPool();
    createDescriptorSetLayout();
    if (fallbackBaseColorTexture_.image == VK_NULL_HANDLE) {
        const std::vector<uint8_t> white{255, 255, 255, 255};
        createTextureFromPixels(white, 1, 1, 4, true, fallbackBaseColorTexture_);
    }
    if (fallbackLinearWhiteTexture_.image == VK_NULL_HANDLE) {
        const std::vector<uint8_t> white{255, 255, 255, 255};
        createTextureFromPixels(white, 1, 1, 4, false, fallbackLinearWhiteTexture_);
    }
    if (fallbackBlackTexture_.image == VK_NULL_HANDLE) {
        const std::vector<uint8_t> black{0, 0, 0, 255};
        createTextureFromPixels(black, 1, 1, 4, false, fallbackBlackTexture_);
    }
    if (fallbackNormalTexture_.image == VK_NULL_HANDLE) {
        const std::vector<uint8_t> flatNormal{128, 128, 255, 255};
        createTextureFromPixels(flatNormal, 1, 1, 4, false, fallbackNormalTexture_);
    }
    createMeshBuffers();
}

void Renderer::destroyPersistentResources() {
    destroyMaterialResources();
    destroyMeshBuffers();
    destroyTexture(fallbackBaseColorTexture_);
    destroyTexture(fallbackLinearWhiteTexture_);
    destroyTexture(fallbackBlackTexture_);
    destroyTexture(fallbackNormalTexture_);
    destroyDescriptorSetLayout();
    destroyUploadCommandPool();
}

void Renderer::createSwapchainDependentResources() {
    createDepthResources();
    createSwapchainRenderPass();
    createPipelineResources();
    createSwapchainFramebuffers();
}

void Renderer::destroySwapchainDependentResources() {
    destroySwapchainFramebuffers();
    destroyPipelineResources();
    destroySwapchainRenderPass();
    destroyDepthResources();
}

void Renderer::createPerFrameResources() {
    frameContexts_.resize(kMaxFramesInFlight);
    const VkDevice device = context_->device();

    for (FrameContext& frame : frameContexts_) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = context_->graphicsQueueFamilyIndex();
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &frame.commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create per-frame command pool.");
        }

        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = frame.commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &frame.primaryCommandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate per-frame command buffer.");
        }

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.renderFinishedSemaphore) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create semaphores.");
        }

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (vkCreateFence(device, &fenceInfo, nullptr, &frame.inFlightFence) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create frame fence.");
        }
    }
}

void Renderer::destroyPerFrameResources() {
    if (context_ == nullptr) {
        return;
    }
    const VkDevice device = context_->device();
    for (FrameContext& frame : frameContexts_) {
        if (frame.inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, frame.inFlightFence, nullptr);
            frame.inFlightFence = VK_NULL_HANDLE;
        }
        if (frame.renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, frame.renderFinishedSemaphore, nullptr);
            frame.renderFinishedSemaphore = VK_NULL_HANDLE;
        }
        if (frame.imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, frame.imageAvailableSemaphore, nullptr);
            frame.imageAvailableSemaphore = VK_NULL_HANDLE;
        }
        if (frame.commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, frame.commandPool, nullptr);
            frame.commandPool = VK_NULL_HANDLE;
        }
    }
    frameContexts_.clear();
}

void Renderer::createSwapchainRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain_.imageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat_;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(context_->device(), &renderPassInfo, nullptr, &swapchainRenderPass_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass.");
    }
}

void Renderer::destroySwapchainRenderPass() {
    if (context_ == nullptr) {
        return;
    }
    if (swapchainRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(context_->device(), swapchainRenderPass_, nullptr);
        swapchainRenderPass_ = VK_NULL_HANDLE;
    }
}

void Renderer::createSwapchainFramebuffers() {
    const std::vector<VkImageView>& imageViews = swapchain_.imageViews();
    swapchainFramebuffers_.resize(imageViews.size(), VK_NULL_HANDLE);
    for (size_t i = 0; i < imageViews.size(); ++i) {
        std::array<VkImageView, 2> attachments = {imageViews[i], depthImageView_};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = swapchainRenderPass_;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapchain_.extent().width;
        framebufferInfo.height = swapchain_.extent().height;
        framebufferInfo.layers = 1;
        if (vkCreateFramebuffer(context_->device(), &framebufferInfo, nullptr, &swapchainFramebuffers_[i]) !=
            VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer.");
        }
    }
}

void Renderer::destroySwapchainFramebuffers() {
    if (context_ == nullptr) {
        return;
    }
    for (VkFramebuffer framebuffer : swapchainFramebuffers_) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(context_->device(), framebuffer, nullptr);
        }
    }
    swapchainFramebuffers_.clear();
}

void Renderer::createDepthResources() {
    depthFormat_ = findDepthFormat();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {swapchain_.extent().width, swapchain_.extent().height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat_;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(context_->device(), &imageInfo, nullptr, &depthImage_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image.");
    }

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(context_->device(), depthImage_, &memReq);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(context_->device(), &allocInfo, nullptr, &depthImageMemory_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate depth memory.");
    }
    vkBindImageMemory(context_->device(), depthImage_, depthImageMemory_, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = depthImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat_;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(context_->device(), &viewInfo, nullptr, &depthImageView_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image view.");
    }
}

void Renderer::destroyDepthResources() {
    if (context_ == nullptr) {
        return;
    }
    if (depthImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(context_->device(), depthImageView_, nullptr);
        depthImageView_ = VK_NULL_HANDLE;
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(context_->device(), depthImage_, nullptr);
        depthImage_ = VK_NULL_HANDLE;
    }
    if (depthImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(context_->device(), depthImageMemory_, nullptr);
        depthImageMemory_ = VK_NULL_HANDLE;
    }
}

void Renderer::createPipelineResources() {
    const std::vector<char> vertCode = readBinaryFile(resolveShaderPath("basic.vert.spv"));
    const std::vector<char> fragCode = readBinaryFile(resolveShaderPath("basic.frag.spv"));
    VkShaderModule vertModule = createShaderModule(vertCode);
    VkShaderModule fragModule = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "main";
    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "main";
    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    std::array<VkVertexInputAttributeDescription, 4> attrs{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)};
    attrs[2] = {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent)};
    attrs[3] = {3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv0)};

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vertexInput.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchain_.extent().width);
    viewport.height = static_cast<float>(swapchain_.extent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, swapchain_.extent()};
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthState{};
    depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthState.depthTestEnable = VK_TRUE;
    depthState.depthWriteEnable = VK_TRUE;
    depthState.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(DrawPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &materialDescriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(context_->device(), &pipelineLayoutInfo, nullptr, &pipelineLayout_) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout.");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthState;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.layout = pipelineLayout_;
    pipelineInfo.renderPass = swapchainRenderPass_;
    pipelineInfo.subpass = 0;
    if (vkCreateGraphicsPipelines(context_->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline_) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline.");
    }

    vkDestroyShaderModule(context_->device(), fragModule, nullptr);
    vkDestroyShaderModule(context_->device(), vertModule, nullptr);
}

void Renderer::destroyPipelineResources() {
    if (context_ == nullptr) {
        return;
    }
    if (graphicsPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(context_->device(), graphicsPipeline_, nullptr);
        graphicsPipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context_->device(), pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
}

void Renderer::createMeshBuffers() {
    buildFallbackGeometry();
}

void Renderer::destroyMeshBuffers() {
    destroyBuffer(indexBuffer_);
    destroyBuffer(vertexBuffer_);
    activeDrawItems_.clear();
}

void Renderer::uploadGeometry(
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices) {
    if (vertices.empty() || indices.empty()) {
        buildFallbackGeometry();
        return;
    }

    destroyBuffer(indexBuffer_);
    destroyBuffer(vertexBuffer_);

    const VkDeviceSize vertexSize = static_cast<VkDeviceSize>(sizeof(Vertex) * vertices.size());
    const VkDeviceSize indexSize = static_cast<VkDeviceSize>(sizeof(uint32_t) * indices.size());

    BufferResource stagingVertex{};
    BufferResource stagingIndex{};
    createBuffer(
        vertexSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingVertex);
    createBuffer(
        indexSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingIndex);

    void* data = nullptr;
    vkMapMemory(context_->device(), stagingVertex.memory, 0, vertexSize, 0, &data);
    std::memcpy(data, vertices.data(), static_cast<size_t>(vertexSize));
    vkUnmapMemory(context_->device(), stagingVertex.memory);

    vkMapMemory(context_->device(), stagingIndex.memory, 0, indexSize, 0, &data);
    std::memcpy(data, indices.data(), static_cast<size_t>(indexSize));
    vkUnmapMemory(context_->device(), stagingIndex.memory);

    createBuffer(
        vertexSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer_);
    createBuffer(
        indexSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        indexBuffer_);

    copyBuffer(stagingVertex.buffer, vertexBuffer_.buffer, vertexSize);
    copyBuffer(stagingIndex.buffer, indexBuffer_.buffer, indexSize);

    destroyBuffer(stagingVertex);
    destroyBuffer(stagingIndex);
}

void Renderer::buildFallbackGeometry() {
    const std::vector<Vertex> vertices = cubeVertices();
    const std::vector<uint32_t> indices = cubeIndices();
    uploadGeometry(vertices, indices);

    activeDrawItems_.clear();
    RenderDrawItem item{};
    item.firstIndex = 0;
    item.indexCount = static_cast<uint32_t>(indices.size());
    item.vertexOffset = 0;
    item.modelMatrix = math::identity();
    item.materialIndex = -1;
    item.baseColorFactor = {0.75f, 0.78f, 0.82f, 1.0f};
    item.baseColorTextureIndex = -1;
    item.normalTextureIndex = -1;
    item.metallicRoughnessTextureIndex = -1;
    item.emissiveTextureIndex = -1;
    item.metallicFactor = 0.0f;
    item.roughnessFactor = 1.0f;
    item.emissiveFactor = {0.0f, 0.0f, 0.0f};
    activeDrawItems_.push_back(item);
    Scene emptyScene;
    createMaterialResources(emptyScene);
    usingFallbackGeometry_ = true;
}

void Renderer::createTextureFromPixels(
    const std::vector<uint8_t>& pixels,
    uint32_t width,
    uint32_t height,
    uint32_t componentCount,
    bool srgb,
    TextureResource& outTexture) {
    if (width == 0 || height == 0 || pixels.empty()) {
        throw std::runtime_error("Invalid texture data.");
    }

    destroyTexture(outTexture);

    std::vector<uint8_t> rgbaPixels;
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    rgbaPixels.resize(pixelCount * 4);
    const uint32_t srcComponents = std::max(componentCount, 1U);
    for (size_t pixel = 0; pixel < pixelCount; ++pixel) {
        const size_t srcBase = pixel * static_cast<size_t>(srcComponents);
        const size_t dstBase = pixel * 4;

        const uint8_t r = srcBase < pixels.size() ? pixels[srcBase] : 0;
        const uint8_t g = (srcComponents > 1 && srcBase + 1 < pixels.size()) ? pixels[srcBase + 1] : r;
        const uint8_t b = (srcComponents > 2 && srcBase + 2 < pixels.size()) ? pixels[srcBase + 2] : r;
        const uint8_t a = (srcComponents > 3 && srcBase + 3 < pixels.size()) ? pixels[srcBase + 3] : 255;

        rgbaPixels[dstBase + 0] = r;
        rgbaPixels[dstBase + 1] = g;
        rgbaPixels[dstBase + 2] = b;
        rgbaPixels[dstBase + 3] = a;
    }

    const VkFormat imageFormat = srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    BufferResource staging{};
    try {
        const VkDeviceSize imageSize = static_cast<VkDeviceSize>(rgbaPixels.size());
        createBuffer(
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            staging);

        void* data = nullptr;
        vkMapMemory(context_->device(), staging.memory, 0, imageSize, 0, &data);
        std::memcpy(data, rgbaPixels.data(), rgbaPixels.size());
        vkUnmapMemory(context_->device(), staging.memory);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = imageFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(context_->device(), &imageInfo, nullptr, &outTexture.image) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture image.");
        }

        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements(context_->device(), outTexture.image, &memReq);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(context_->device(), &allocInfo, nullptr, &outTexture.memory) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate texture image memory.");
        }
        vkBindImageMemory(context_->device(), outTexture.image, outTexture.memory, 0);

        transitionImageLayout(outTexture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(staging.buffer, outTexture.image, width, height);
        transitionImageLayout(
            outTexture.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = outTexture.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = imageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(context_->device(), &viewInfo, nullptr, &outTexture.imageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture image view.");
        }

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        if (vkCreateSampler(context_->device(), &samplerInfo, nullptr, &outTexture.sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create texture sampler.");
        }

        outTexture.width = width;
        outTexture.height = height;
    } catch (...) {
        destroyBuffer(staging);
        destroyTexture(outTexture);
        throw;
    }

    destroyBuffer(staging);
}

void Renderer::destroyTexture(TextureResource& texture) {
    if (context_ == nullptr) {
        return;
    }
    if (texture.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(context_->device(), texture.sampler, nullptr);
        texture.sampler = VK_NULL_HANDLE;
    }
    if (texture.imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(context_->device(), texture.imageView, nullptr);
        texture.imageView = VK_NULL_HANDLE;
    }
    if (texture.image != VK_NULL_HANDLE) {
        vkDestroyImage(context_->device(), texture.image, nullptr);
        texture.image = VK_NULL_HANDLE;
    }
    if (texture.memory != VK_NULL_HANDLE) {
        vkFreeMemory(context_->device(), texture.memory, nullptr);
        texture.memory = VK_NULL_HANDLE;
    }
    texture.width = 0;
    texture.height = 0;
}

void Renderer::transitionImageLayout(
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout) const {
    VkCommandBuffer cmd = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (
        oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported image layout transition.");
    }

    vkCmdPipelineBarrier(
        cmd,
        srcStage,
        dstStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);

    endSingleTimeCommands(cmd);
}

void Renderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const {
    VkCommandBuffer cmd = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(cmd);
}

void Renderer::createDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 4> samplerBindings{};
    for (uint32_t bindingIndex = 0; bindingIndex < static_cast<uint32_t>(samplerBindings.size()); ++bindingIndex) {
        VkDescriptorSetLayoutBinding& binding = samplerBindings[bindingIndex];
        binding.binding = bindingIndex;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        binding.pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(samplerBindings.size());
    layoutInfo.pBindings = samplerBindings.data();

    if (vkCreateDescriptorSetLayout(context_->device(), &layoutInfo, nullptr, &materialDescriptorSetLayout_) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create material descriptor set layout.");
    }
}

void Renderer::destroyDescriptorSetLayout() {
    if (context_ == nullptr) {
        return;
    }
    if (materialDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context_->device(), materialDescriptorSetLayout_, nullptr);
        materialDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
}

void Renderer::createMaterialResources(const Scene& scene) {
    destroyMaterialResources();

    uint32_t descriptorCount = static_cast<uint32_t>(activeDrawItems_.size());
    if (descriptorCount == 0) {
        descriptorCount = 1;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = descriptorCount * 4;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = descriptorCount;

    if (vkCreateDescriptorPool(context_->device(), &poolInfo, nullptr, &materialDescriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create material descriptor pool.");
    }

    std::vector<VkDescriptorSetLayout> layouts(descriptorCount, materialDescriptorSetLayout_);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = materialDescriptorPool_;
    allocInfo.descriptorSetCount = descriptorCount;
    allocInfo.pSetLayouts = layouts.data();

    drawDescriptorSets_.resize(descriptorCount, VK_NULL_HANDLE);
    if (vkAllocateDescriptorSets(context_->device(), &allocInfo, drawDescriptorSets_.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate material descriptor sets.");
    }

    const auto& textures = scene.textures();
    std::unordered_map<uint64_t, size_t> textureCache;
    materialTextures_.clear();
    materialTextures_.reserve(activeDrawItems_.size() * 4);

    const auto resolveTexture = [&](int textureIndex, bool srgb, const TextureResource& fallback) -> const TextureResource* {
        if (textureIndex < 0 || static_cast<size_t>(textureIndex) >= textures.size()) {
            return &fallback;
        }

        const uint64_t cacheKey = (static_cast<uint64_t>(static_cast<uint32_t>(textureIndex)) << 1) |
                                  static_cast<uint64_t>(srgb ? 1U : 0U);
        const auto cacheIt = textureCache.find(cacheKey);
        if (cacheIt != textureCache.end()) {
            return &materialTextures_[cacheIt->second];
        }

        const TextureAsset& textureAsset = textures[static_cast<size_t>(textureIndex)];
        if (textureAsset.pixels.empty() || textureAsset.width <= 0 || textureAsset.height <= 0) {
            return &fallback;
        }

        materialTextures_.emplace_back();
        TextureResource& newTexture = materialTextures_.back();
        createTextureFromPixels(
            textureAsset.pixels,
            static_cast<uint32_t>(textureAsset.width),
            static_cast<uint32_t>(textureAsset.height),
            static_cast<uint32_t>(std::max(textureAsset.component, 1)),
            srgb,
            newTexture);
        textureCache.emplace(cacheKey, materialTextures_.size() - 1);
        return &newTexture;
    };

    for (size_t drawIndex = 0; drawIndex < activeDrawItems_.size(); ++drawIndex) {
        const RenderDrawItem& item = activeDrawItems_[drawIndex];
        const TextureResource* baseColorTexture =
            resolveTexture(item.baseColorTextureIndex, true, fallbackBaseColorTexture_);
        const TextureResource* normalTexture =
            resolveTexture(item.normalTextureIndex, false, fallbackNormalTexture_);
        const TextureResource* metallicRoughnessTexture =
            resolveTexture(item.metallicRoughnessTextureIndex, false, fallbackLinearWhiteTexture_);
        const TextureResource* emissiveTexture =
            resolveTexture(item.emissiveTextureIndex, false, fallbackBlackTexture_);

        std::array<VkDescriptorImageInfo, 4> imageInfos{};
        imageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[0].imageView = baseColorTexture->imageView;
        imageInfos[0].sampler = baseColorTexture->sampler;

        imageInfos[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[1].imageView = normalTexture->imageView;
        imageInfos[1].sampler = normalTexture->sampler;

        imageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[2].imageView = metallicRoughnessTexture->imageView;
        imageInfos[2].sampler = metallicRoughnessTexture->sampler;

        imageInfos[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[3].imageView = emissiveTexture->imageView;
        imageInfos[3].sampler = emissiveTexture->sampler;

        std::array<VkWriteDescriptorSet, 4> writes{};
        for (uint32_t bindingIndex = 0; bindingIndex < static_cast<uint32_t>(writes.size()); ++bindingIndex) {
            VkWriteDescriptorSet& write = writes[bindingIndex];
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = drawDescriptorSets_[drawIndex];
            write.dstBinding = bindingIndex;
            write.dstArrayElement = 0;
            write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.descriptorCount = 1;
            write.pImageInfo = &imageInfos[bindingIndex];
        }
        vkUpdateDescriptorSets(
            context_->device(),
            static_cast<uint32_t>(writes.size()),
            writes.data(),
            0,
            nullptr);
    }
}

void Renderer::destroyMaterialResources() {
    if (context_ == nullptr) {
        return;
    }

    for (TextureResource& texture : materialTextures_) {
        destroyTexture(texture);
    }
    materialTextures_.clear();
    drawDescriptorSets_.clear();

    if (materialDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context_->device(), materialDescriptorPool_, nullptr);
        materialDescriptorPool_ = VK_NULL_HANDLE;
    }
}

void Renderer::createUploadCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = context_->graphicsQueueFamilyIndex();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    if (vkCreateCommandPool(context_->device(), &poolInfo, nullptr, &uploadCommandPool_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create upload command pool.");
    }
}

void Renderer::destroyUploadCommandPool() {
    if (context_ == nullptr) {
        return;
    }
    if (uploadCommandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(context_->device(), uploadCommandPool_, nullptr);
        uploadCommandPool_ = VK_NULL_HANDLE;
    }
}

void Renderer::recreateSwapchain(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }
    vkDeviceWaitIdle(context_->device());
    destroySwapchainDependentResources();
    swapchain_.recreate(width, height);
    createSwapchainDependentResources();
    imagesInFlight_.assign(swapchain_.images().size(), VK_NULL_HANDLE);
}

void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex, const float* viewProjection) const {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer.");
    }

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.05f, 0.08f, 0.12f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    VkRenderPassBeginInfo passInfo{};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = swapchainRenderPass_;
    passInfo.framebuffer = swapchainFramebuffers_[imageIndex];
    passInfo.renderArea.offset = {0, 0};
    passInfo.renderArea.extent = swapchain_.extent();
    passInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    passInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

    VkBuffer vbs[] = {vertexBuffer_.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbs, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.buffer, 0, VK_INDEX_TYPE_UINT32);
    const math::Mat4 vp = {std::array<float, 16>{
        viewProjection[0], viewProjection[1], viewProjection[2], viewProjection[3],
        viewProjection[4], viewProjection[5], viewProjection[6], viewProjection[7],
        viewProjection[8], viewProjection[9], viewProjection[10], viewProjection[11],
        viewProjection[12], viewProjection[13], viewProjection[14], viewProjection[15]}};

    size_t drawIndex = 0;
    for (const RenderDrawItem& item : activeDrawItems_) {
        if (!drawDescriptorSets_.empty()) {
            const size_t descriptorIndex = std::min(drawIndex, drawDescriptorSets_.size() - 1);
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout_,
                0,
                1,
                &drawDescriptorSets_[descriptorIndex],
                0,
                nullptr);
        }

        math::Mat4 model = item.modelMatrix;
        if (usingFallbackGeometry_) {
            const math::Mat4 spin = math::rotateY(static_cast<float>(frameCounter_) * 0.01f);
            model = math::multiply(spin, model);
        }
        const math::Mat4 mvp = math::multiply(vp, model);
        DrawPushConstants push{};
        std::memcpy(push.mvp, mvp.m.data(), sizeof(push.mvp));
        push.baseColor[0] = item.baseColorFactor.x;
        push.baseColor[1] = item.baseColorFactor.y;
        push.baseColor[2] = item.baseColorFactor.z;
        push.baseColor[3] = item.baseColorFactor.w;
        push.emissiveFactor[0] = item.emissiveFactor.x;
        push.emissiveFactor[1] = item.emissiveFactor.y;
        push.emissiveFactor[2] = item.emissiveFactor.z;
        push.emissiveFactor[3] = 1.0f;
        push.materialParams[0] = item.metallicFactor;
        push.materialParams[1] = item.roughnessFactor;
        push.materialParams[2] = 1.0f;
        push.materialParams[3] = 0.0f;
        vkCmdPushConstants(
            cmd,
            pipelineLayout_,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(DrawPushConstants),
            &push);
        vkCmdDrawIndexed(cmd, item.indexCount, 1, item.firstIndex, item.vertexOffset, 0);
        ++drawIndex;
    }
    vkCmdEndRenderPass(cmd);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end command buffer.");
    }
}

void Renderer::computeViewProjection(const Camera& camera, math::Mat4& outViewProjection) const {
    const float yaw = math::radians(camera.yawDegrees);
    const float pitch = math::radians(camera.pitchDegrees);
    math::Vec3 eye{camera.position[0], camera.position[1], camera.position[2]};
    math::Vec3 dir{
        std::cos(yaw) * std::cos(pitch),
        std::sin(pitch),
        std::sin(yaw) * std::cos(pitch),
    };
    dir = math::normalize(dir);

    const math::Mat4 view = math::lookAt(eye, math::add(eye, dir), {0.0f, 1.0f, 0.0f});
    const float aspect = static_cast<float>(swapchain_.extent().width) /
                         static_cast<float>(swapchain_.extent().height);
    const math::Mat4 proj = math::perspective(math::radians(camera.fovDegrees), aspect, 0.1f, 200.0f);
    outViewProjection = math::multiply(proj, view);
}

void Renderer::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    BufferResource& outBuffer) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(context_->device(), &bufferInfo, nullptr, &outBuffer.buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer.");
    }

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(context_->device(), outBuffer.buffer, &memReq);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);
    if (vkAllocateMemory(context_->device(), &allocInfo, nullptr, &outBuffer.memory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory.");
    }
    vkBindBufferMemory(context_->device(), outBuffer.buffer, outBuffer.memory, 0);
    outBuffer.size = size;
}

void Renderer::destroyBuffer(BufferResource& buffer) {
    if (context_ == nullptr) {
        return;
    }
    if (buffer.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_->device(), buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
    }
    if (buffer.memory != VK_NULL_HANDLE) {
        vkFreeMemory(context_->device(), buffer.memory, nullptr);
        buffer.memory = VK_NULL_HANDLE;
    }
    buffer.size = 0;
}

void Renderer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer cmd = beginSingleTimeCommands();
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmd, srcBuffer, dstBuffer, 1, &copyRegion);
    endSingleTimeCommands(cmd);
}

uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(context_->physicalDevice(), &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1U << i)) != 0 &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("No suitable memory type found.");
}

VkCommandBuffer Renderer::beginSingleTimeCommands() const {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = uploadCommandPool_;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(context_->device(), &allocInfo, &cmd) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate upload command buffer.");
    }
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    return cmd;
}

void Renderer::endSingleTimeCommands(VkCommandBuffer cmd) const {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    vkQueueSubmit(context_->graphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context_->graphicsQueue());
    vkFreeCommandBuffers(context_->device(), uploadCommandPool_, 1, &cmd);
}

VkFormat Renderer::findDepthFormat() const {
    const std::array<VkFormat, 3> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(context_->physicalDevice(), format, &props);
        if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0) {
            return format;
        }
    }
    throw std::runtime_error("No depth format available.");
}

VkShaderModule Renderer::createShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(context_->device(), &createInfo, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module.");
    }
    return module;
}

}  // namespace vr
