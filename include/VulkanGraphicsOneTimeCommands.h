#pragma once 

#include "VulkanGraphicsMemoryAllocator.h"
#include "VulkanGraphicsCommandBufferFactory.h"
#include "VulkanGraphicsCommandQueue.h"
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
            CommandQueue& commandQueue);
        ~OneTimeCommandsHelper();

        void execute(const OneTimeCommandsRunner::RecordCommandsFunc& recordCommandsFunc) {
            OneTimeCommandsRunner runner(
                m_commandBufferFactory,
                recordCommandsFunc);
            runner.submit(m_commandQueue);
        }

        // Records and submits one-time-use command buffer that copies the
        // provided data into the provided buffer.
        void copyDataToBuffer(
            MemoryAllocator::Buffer& buffer,
            const std::vector<uint8_t>& data);

        void copyDataToBuffer(
            MemoryAllocator::Buffer& buffer,
            const void* pData,
            VkDeviceSize dataSizeBytes);

        enum class GenerateMips
        {
            No,
            Yes
        };
        void copyDataToImage(
            Image& image,
            const void* pData,
            VkDeviceSize dataSizeBytes,
            GenerateMips genMips);

    private:
        Context& m_context;
        CommandBufferFactory& m_commandBufferFactory;
        CommandQueue m_commandQueue;
    };
}
