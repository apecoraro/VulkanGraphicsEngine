#include "VulkanGraphicsOneTimeCommands.h"

#include "VulkanGraphicsContext.h"

#include <cassert>

namespace vgfx
{
    OneTimeCommandsRunner::OneTimeCommandsRunner(
        CommandBufferFactory& commandBufferFactory,
        const RecordCommandsFunc& recordCommandsFunc)
        : m_commandBufferFactory(commandBufferFactory)
    {
        m_commandBuffer = commandBufferFactory.createCommandBuffer();

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

        recordCommandsFunc(m_commandBuffer);

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
        const CommandQueue& queue)
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
}