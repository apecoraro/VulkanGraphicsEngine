#include "VulkanGraphicsOneTimeCommands.h"

#include "VulkanGraphicsContext.h"

#include <cassert>

namespace vgfx
{
    OneTimeCommandsRunner::OneTimeCommandsRunner(
        CommandBufferFactory& commandBufferFactory,
        const RecordCommandsFunc& drawFunc)
        : m_commandBufferFactory(commandBufferFactory)
    {
        m_commandBuffer = commandBufferFactory.createCommandBuffer();

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

        drawFunc(m_commandBuffer);

        vkEndCommandBuffer(m_commandBuffer);
    }

    OneTimeCommandsRunner::~OneTimeCommandsRunner()
    {
        if (m_commandBuffer != VK_NULL_HANDLE) {
            m_commandBufferFactory.freeCommandBuffer(m_commandBuffer);
        }
    }

    void OneTimeCommandsRunner::submit(CommandQueue commandQueue)
    {
        assert(commandQueue.queueFamilyIndex == m_commandBufferFactory.getQueueFamilyIndex());

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_commandBuffer;

        vkQueueSubmit(commandQueue.queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(commandQueue.queue);
    }

    OneTimeCommandsHelper::OneTimeCommandsHelper(
        Context& context,
        CommandBufferFactory& commandBufferFactory,
        // TODO CommandBufferFactory - create command buffers, must use pool created for the provided queue
        CommandQueue& queue)
        : m_context(context)
        , m_commandBufferFactory(commandBufferFactory)
        , m_commandQueue(queue)
    {
    }

    OneTimeCommandsHelper::~OneTimeCommandsHelper()
    {
    }

    void OneTimeCommandsHelper::copyDataToBuffer(
        MemoryAllocator::Buffer& buffer,
        const std::vector<uint8_t>& data)
    {
        copyDataToBuffer(buffer, data.data(), data.size());
    }

    void OneTimeCommandsHelper::copyDataToBuffer(
        MemoryAllocator::Buffer& buffer,
        const void* pData,
        VkDeviceSize dataSizeBytes)
    {
        auto& memoryAllocator = m_context.getMemoryAllocator();
        auto stagingBuffer =
            memoryAllocator.createBuffer(
                dataSizeBytes,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY);

        void* pDataDst = nullptr;
        memoryAllocator.mapBuffer(stagingBuffer, &pDataDst);

        memcpy(pDataDst, pData, dataSizeBytes);

        memoryAllocator.unmapBuffer(stagingBuffer);

        OneTimeCommandsRunner runner(
            m_commandBufferFactory,
            [&buffer, &dataSizeBytes, &stagingBuffer] (VkCommandBuffer commandBuffer) {
                VkBufferCopy copyRegion = {};
                copyRegion.srcOffset = 0; // Optional
                copyRegion.dstOffset = 0; // Optional
                copyRegion.size = dataSizeBytes;
                vkCmdCopyBuffer(commandBuffer, stagingBuffer.handle, buffer.handle, 1, &copyRegion);
            });

        runner.submit(m_commandQueue);

        memoryAllocator.destroyBuffer(stagingBuffer);
    }

    static void RecordImageMemBarrierCommand(
        VkCommandBuffer vulkanCommandBuffer,
        VkImage image,
        uint32_t baseMipLevel,
        uint32_t levelCount,
        VkImageLayout oldLayout,
        VkImageLayout newLayout)
    {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;

        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = baseMipLevel,
        barrier.subresourceRange.levelCount = levelCount;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            barrier.srcAccessMask = 0;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else
        {
            assert(false && "Unsupported old layout transition in QuantumVideoDecoder!");
        }

        VkPipelineStageFlags destinationStage;
        if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        {
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
        {
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (newLayout == VK_IMAGE_LAYOUT_GENERAL)
        {
            barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else
        {
            assert(false && "Unsupported new layout transition in QuantumVideoDecoder!");
        }

        vkCmdPipelineBarrier(vulkanCommandBuffer,
            sourceStage,
            destinationStage,
            0, // VkDependencyFlags
            0, // Memory Barrier Count
            nullptr, // const VkMemoryBarrier*
            0, // Buffer Memory Barrier Count
            nullptr, // const VkBufferMemoryBarrier*
            1, // Image Memory Barrier Count
            &barrier); // const VkImageMemoryBarrier*
    }

    static void RecordImageBlitCommand(
        VkCommandBuffer vulkanCommandBuffer,
        VkImage srcImage,
        const VkOffset3D& srcSize,
        uint32_t srcMipLevel,
        VkImageLayout srcLayout,
        VkImage dstImage,
        const VkOffset3D& dstSize,
        uint32_t dstMipLevel,
        VkImageLayout dstLayout,
        VkFilter blitFilter)
    {
        VkImageBlit blit = {};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = srcSize;
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = srcMipLevel,
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = dstSize;
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = dstMipLevel;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(vulkanCommandBuffer,
            srcImage, srcLayout,
            dstImage, dstLayout,
            1u, // blit regions count
            &blit, // blit region
            VK_FILTER_LINEAR);
    }

    void OneTimeCommandsHelper::copyDataToImage(
        Image& image,
        const void* pData,
        VkDeviceSize dataSizeBytes,
        GenerateMips genMips)
    {
        auto& memoryAllocator = m_context.getMemoryAllocator();
        auto stagingBuffer =
            memoryAllocator.createBuffer(
                dataSizeBytes,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY);

        void* pDataDst = nullptr;
        memoryAllocator.mapBuffer(stagingBuffer, &pDataDst);

        memcpy(pDataDst, pData, dataSizeBytes);

        memoryAllocator.unmapBuffer(stagingBuffer);

        OneTimeCommandsRunner runner(
            m_commandBufferFactory,
            [&image, &dataSizeBytes, &stagingBuffer, &genMips] (VkCommandBuffer commandBuffer) {
                RecordImageMemBarrierCommand(
                    commandBuffer,
                    image.getHandle(),
                    0u, // base mip level
                    image.getMipLevels(),
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

                VkBufferImageCopy copyRegion = {};
                copyRegion.bufferOffset = 0;
                copyRegion.bufferRowLength = 0;
                copyRegion.bufferImageHeight = 0;

                copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                copyRegion.imageSubresource.mipLevel = 0;
                copyRegion.imageSubresource.baseArrayLayer = 0;
                copyRegion.imageSubresource.layerCount = 1;

                copyRegion.imageOffset = { 0, 0, 0 };
                copyRegion.imageExtent = {
                    image.getWidth(),
                    image.getHeight(),
                    1
                };

                vkCmdCopyBufferToImage(
                    commandBuffer,
                    stagingBuffer.handle,
                    image.getHandle(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &copyRegion);

                if (genMips == GenerateMips::Yes) {
                    VkOffset3D inputSize = {
                        static_cast<int32_t>(image.getWidth()),
                        static_cast<int32_t>(image.getHeight()),
                        1u,
                    };
                    VkOffset3D outputSize = {
                        std::max(inputSize.x >> 1, 1),
                        std::max(inputSize.y >> 1, 1),
                        1,
                    };
                    for (uint32_t mipLevel = 1; mipLevel < image.getMipLevels(); ++mipLevel) {
                        RecordImageMemBarrierCommand(
                            commandBuffer,
                            image.getHandle(),
                            mipLevel - 1u, // transfer source level to transfer src
                            1u,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

                        RecordImageBlitCommand(
                            commandBuffer,
                            image.getHandle(),
                            inputSize,
                            mipLevel - 1u,
                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                            image.getHandle(),
                            outputSize,
                            mipLevel,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_FILTER_LINEAR);

                        inputSize = outputSize;
                        outputSize = {
                            std::max(inputSize.x >> 1, 1),
                            std::max(inputSize.y >> 1, 1),
                            1,
                        };
                    }

                    RecordImageMemBarrierCommand(
                        commandBuffer,
                        image.getHandle(),
                        0u, // base mip level
                        image.getMipLevels(),
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }
            });

        runner.submit(m_commandQueue);

        memoryAllocator.destroyBuffer(stagingBuffer);
    }
}