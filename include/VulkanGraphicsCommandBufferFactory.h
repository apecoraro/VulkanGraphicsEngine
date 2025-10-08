#pragma once

#include <VulkanGraphicsCommandQueue.h>

#include <cstdint>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Context;

    class CommandBufferFactory
    {
    public:
        CommandBufferFactory(Context& context, CommandQueue commandQueue);
        ~CommandBufferFactory();

        CommandQueue getCommandQueue() const { return m_commandQueue; }

        VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        void freeCommandBuffer(VkCommandBuffer buffer);
    private:
        Context& m_context;
        CommandQueue m_commandQueue;
        VkCommandPool m_commandPool = VK_NULL_HANDLE;
    };
}

