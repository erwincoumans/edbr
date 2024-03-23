#pragma once

#include <array>
#include <cstdint>
#include <filesystem>

// don't sort these includes
// clang-format off
#include <vulkan/vulkan.h>
#include <volk.h> // include needed for TracyVulkan.hpp
#include <VkBootstrap.h>
#include <vk_mem_alloc.h>
#include <tracy/TracyVulkan.hpp>
// clang-format on

#include <edbr/Graphics/Common.h>
#include <edbr/Graphics/Vulkan/Swapchain.h>
#include <edbr/Graphics/Vulkan/VulkanImGui.h>
#include <edbr/Graphics/Vulkan/VulkanImmediateExecutor.h>

#include <edbr/Graphics/ImageCache.h>

namespace vkutil
{
struct CreateImageInfo;
}

struct GPUBuffer;
struct GPUImage;

class GfxDevice {
public:
    struct FrameData {
        VkCommandPool commandPool;
        VkCommandBuffer mainCommandBuffer;
        TracyVkCtx tracyVkCtx;
    };

public:
    GfxDevice();
    GfxDevice(const GfxDevice&) = delete;
    GfxDevice& operator=(const GfxDevice&) = delete;

    void init(SDL_Window* window, const char* appName, bool vSync);

    VkCommandBuffer beginFrame();
    void endFrame(VkCommandBuffer cmd, const GPUImage& drawImage);
    void cleanup();

    [[nodiscard]] GPUBuffer createBuffer(
        std::size_t allocSize,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO) const;
    [[nodiscard]] VkDeviceAddress getBufferAddress(const GPUBuffer& buffer) const;
    void destroyBuffer(const GPUBuffer& buffer) const;

    bool deviceSupportsSamplingCount(VkSampleCountFlagBits sample) const;
    VkSampleCountFlagBits getMaxSupportedSamplingCount() const;
    float getMaxAnisotropy() const { return maxSamplerAnisotropy; }

    VulkanImmediateExecutor createImmediateExecutor() const;
    void immediateSubmit(std::function<void(VkCommandBuffer)>&& f) const;

    void waitIdle() const;

    BindlessSetManager& getBindlessSetManager();
    VkDescriptorSetLayout getBindlessDescSetLayout() const;
    const VkDescriptorSet& getBindlessDescSet() const;
    void bindBindlessDescSet(VkCommandBuffer cmd, VkPipelineLayout layout) const;

public:
    [[nodiscard]] ImageId createImage(
        const vkutil::CreateImageInfo& createInfo,
        const char* debugName = nullptr,
        void* pixelData = nullptr,
        ImageId imageId = NULL_IMAGE_ID);
    [[nodiscard]] ImageId loadImageFromFile(
        const std::filesystem::path& path,
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB,
        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT,
        bool mipMap = false);
    ImageId addImageToCache(GPUImage image);

    [[nodiscard]] const GPUImage& getImage(ImageId id) const;

    void uploadImageData(const GPUImage& image, void* pixelData, std::uint32_t layer = 0) const;

    ImageId getWhiteTextureID() { return whiteTextureID; }

    // createImageRaw is mostly intended for low level usage. In most cases,
    // createImage should be preferred as it will automatically
    // add the image to bindless set
    [[nodiscard]] GPUImage createImageRaw(const vkutil::CreateImageInfo& createInfo) const;
    // loadImageFromFileRaw is mostly intended for low level usage. In most cases,
    // loadImageFromFile should be preferred as it will automatically
    // add the image to bindless set
    [[nodiscard]] GPUImage loadImageFromFileRaw(
        const std::filesystem::path& path,
        VkFormat format,
        VkImageUsageFlags usage,
        bool mipMap) const;
    // destroyImage should only be called on images not beloning to image cache / bindless set
    void destroyImage(const GPUImage& image) const;

public:
    VkDevice getDevice() const { return device; }

    std::uint32_t getCurrentFrameIndex() const;
    VkExtent2D getSwapchainExtent() const { return swapchain.getExtent(); }
    const TracyVkCtx& getTracyVkCtx() const;

private:
    void initVulkan(SDL_Window* window, const char* appName);
    void checkDeviceCapabilities();
    void createCommandBuffers();

    FrameData& getCurrentFrame();

private: // data
    vkb::Instance instance;
    vkb::PhysicalDevice physicalDevice;
    vkb::Device device;
    VmaAllocator allocator;

    std::uint32_t graphicsQueueFamily;
    VkQueue graphicsQueue;

    VkSurfaceKHR surface;
    Swapchain swapchain;

    std::array<FrameData, graphics::FRAME_OVERLAP> frames{};
    std::uint32_t frameNumber{0};

    VulkanImmediateExecutor executor;

    VulkanImGuiData imguiData;
    bool imguiDrawn{true};

    VkSampleCountFlagBits supportedSampleCounts;
    VkSampleCountFlagBits highestSupportedSamples{VK_SAMPLE_COUNT_1_BIT};
    float maxSamplerAnisotropy{1.f};

    ImageCache imageCache;

    ImageId whiteTextureID;
};