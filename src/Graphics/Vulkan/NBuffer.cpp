#include "NBuffer.h"

#include <cassert>
#include <cstring>

#include <volk.h>

#include "Util.h"

void NBuffer::init(
    VkDevice device,
    VmaAllocator allocator,
    VkBufferUsageFlags usage,
    std::size_t dataSize,
    std::size_t numFramesInFlight,
    const char* label)
{
    assert(numFramesInFlight > 0);
    assert(dataSize > 0);

    framesInFlight = numFramesInFlight;
    gpuBufferSize = dataSize;

    gpuBuffer = vkutil::createBuffer(
        allocator,
        dataSize,
        usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
    vkutil::addDebugLabel(device, gpuBuffer.buffer, label);

    for (std::size_t i = 0; i < numFramesInFlight; ++i) {
        stagingBuffers.push_back(vkutil::createBuffer(
            allocator,
            dataSize,
            usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_HOST));
    }

    initialized = true;
}

void NBuffer::cleanup(VkDevice device, VmaAllocator allocator)
{
    for (const auto& stagingBuffer : stagingBuffers) {
        vkutil::destroyBuffer(allocator, stagingBuffer);
    }
    stagingBuffers.clear();

    vkutil::destroyBuffer(allocator, gpuBuffer);

    initialized = false;
}

void NBuffer::uploadNewData(
    VkCommandBuffer cmd,
    std::size_t frameIndex,
    void* newData,
    std::size_t dataSize)
{
    assert(initialized);
    assert(frameIndex < framesInFlight);
    assert(dataSize <= gpuBufferSize);

    // sync with previous read
    const auto bufferBarrier = VkBufferMemoryBarrier2{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .buffer = gpuBuffer.buffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE,
    };
    const auto dependencyInfo = VkDependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &bufferBarrier,
    };
    vkCmdPipelineBarrier2(cmd, &dependencyInfo);

    auto& staging = stagingBuffers[frameIndex];
    memcpy(staging.info.pMappedData, newData, dataSize);

    const auto region = VkBufferCopy2{
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = 0,
        .dstOffset = 0,
        .size = dataSize,
    };
    const auto bufCopyInfo = VkCopyBufferInfo2{
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .srcBuffer = staging.buffer,
        .dstBuffer = gpuBuffer.buffer,
        .regionCount = 1,
        .pRegions = &region,
    };

    vkCmdCopyBuffer2(cmd, &bufCopyInfo);

    { // sync write
        const auto bufferBarrier = VkBufferMemoryBarrier2{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
            .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
            .buffer = gpuBuffer.buffer,
            .offset = 0,
            .size = VK_WHOLE_SIZE,
        };
        const auto dependencyInfo = VkDependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = &bufferBarrier,
        };
        vkCmdPipelineBarrier2(cmd, &dependencyInfo);
    }
}
