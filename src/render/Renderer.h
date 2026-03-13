#pragma once

#include "render/RenderScene.h"
#include "scene/Camera.h"
#include "scene/Mesh.h"
#include "scene/Scene.h"
#include "vulkan/FrameContext.h"
#include "vulkan/Swapchain.h"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace vr {

class VulkanContext;

class Renderer {
public:
    void initialize(VulkanContext& context, uint32_t width, uint32_t height);
    void shutdown();
    void renderFrame(const Scene& scene, const Camera& camera);
    void resize(uint32_t width, uint32_t height);
    void setScene(const Scene& scene);

private:
    static constexpr uint32_t kMaxFramesInFlight = 2;

    struct BufferResource {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize size = 0;
    };

    struct TextureResource {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    void createPersistentResources();
    void destroyPersistentResources();
    void createSwapchainDependentResources();
    void destroySwapchainDependentResources();

    void createPerFrameResources();
    void destroyPerFrameResources();
    void createSwapchainRenderPass();
    void destroySwapchainRenderPass();
    void createSwapchainFramebuffers();
    void destroySwapchainFramebuffers();
    void createDepthResources();
    void destroyDepthResources();
    void createPipelineResources();
    void destroyPipelineResources();
    void createMeshBuffers();
    void destroyMeshBuffers();
    void createDescriptorSetLayout();
    void destroyDescriptorSetLayout();
    void createMaterialResources(const Scene& scene);
    void destroyMaterialResources();
    void createUploadCommandPool();
    void destroyUploadCommandPool();

    void recreateSwapchain(uint32_t width, uint32_t height);
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, const float* viewProjection) const;
    void computeViewProjection(const Camera& camera, math::Mat4& outViewProjection) const;
    void uploadGeometry(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void buildFallbackGeometry();
    void createTextureFromPixels(
        const std::vector<uint8_t>& pixels,
        uint32_t width,
        uint32_t height,
        uint32_t componentCount,
        bool srgb,
        TextureResource& outTexture);
    void destroyTexture(TextureResource& texture);
    void transitionImageLayout(
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout) const;
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;

    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        BufferResource& outBuffer);
    void destroyBuffer(BufferResource& buffer);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    VkCommandBuffer beginSingleTimeCommands() const;
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) const;
    VkFormat findDepthFormat() const;
    VkShaderModule createShaderModule(const std::vector<char>& code) const;

    bool isInitialized_ = false;
    VulkanContext* context_ = nullptr;
    Swapchain swapchain_{};
    std::vector<FrameContext> frameContexts_;
    std::vector<VkFence> imagesInFlight_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
    VkRenderPass swapchainRenderPass_ = VK_NULL_HANDLE;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout materialDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool materialDescriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> drawDescriptorSets_;
    BufferResource vertexBuffer_{};
    BufferResource indexBuffer_{};
    VkCommandPool uploadCommandPool_ = VK_NULL_HANDLE;
    TextureResource fallbackBaseColorTexture_{};
    TextureResource fallbackLinearWhiteTexture_{};
    TextureResource fallbackBlackTexture_{};
    TextureResource fallbackNormalTexture_{};
    std::vector<TextureResource> materialTextures_;
    RenderScene renderScene_{};
    std::vector<RenderDrawItem> activeDrawItems_;
    bool usingFallbackGeometry_ = true;
    uint32_t currentFrameIndex_ = 0;
    uint32_t framebufferWidth_ = 0;
    uint32_t framebufferHeight_ = 0;
    uint64_t frameCounter_ = 0;
};

}  // namespace vr
