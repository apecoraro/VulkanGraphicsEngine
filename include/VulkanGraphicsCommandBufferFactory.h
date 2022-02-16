#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Context;

    class CommandBufferFactory
    {
    public:
        CommandBufferFactory(Context& context, uint32_t queueFamilyIndex);
        ~CommandBufferFactory();

        uint32_t getQueueFamilyIndex() const { return m_queueFamilyIndex;  }

        VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        void freeCommandBuffer(VkCommandBuffer buffer);
    private:
        Context& m_context;
        uint32_t m_queueFamilyIndex = -1;
        VkCommandPool m_commandPool = VK_NULL_HANDLE;
    };
}

