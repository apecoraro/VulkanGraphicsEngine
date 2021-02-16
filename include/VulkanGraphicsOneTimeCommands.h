#ifndef QUANTUM_GFX_ONE_TIME_COMMANDS_H
#define QUANTUM_GFX_ONE_TIME_COMMANDS_H

#include "VulkanGraphicsMemoryAllocator.h"
#include "VulkanGraphicsCommandBuffers.h"
#include "VulkanGraphicsImage.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>
#include <vulkan/vulkan.h>

namespace vgfx
{
    class Context;

    class OneTimeCommandsRunner
    {
    public:
        using RecordCommandsFunc = std::function<void(VkCommandBuffer commandBuffer)>;
        OneTimeCommandsRunner(
            CommandBufferFactory& commandBufferFactory,
            const RecordCommandsFunc& recordCommandsFunc);
        ~OneTimeCommandsRunner();

        void submit(CommandQueue queue);

    private:
        CommandBufferFactory& m_commandBufferFactory;
        VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    };

    class OneTimeCommandsHelper
    {
    public:
        OneTimeCommandsHelper(
            Context& context,
            CommandBufferFactory& commandBufferFactory,
            const CommandQueue& commandQueue);
        ~OneTimeCommandsHelper();

        // Records and submits one-time-use command buffer that copies the
        // provided data into the provided buffer.
        void copyDataToBuffer(
            MemoryAllocator::Buffer& buffer,
            const std::vector<uint8_t>& data);

        void copyDataToBuffer(
            MemoryAllocator::Buffer& buffer,
            const void* pData,
            VkDeviceSize dataSizeBytes);

        void copyDataToImage(
            Image& image,
            const void* pData,
            VkDeviceSize dataSizeBytes);

    private:
        Context& m_context;
        CommandBufferFactory& m_commandBufferFactory;
        CommandQueue m_commandQueue;
    };
}

#endif