#include "VulkanGraphicsCommandBufferFactory.h"

#include "VulkanGraphicsContext.h"

#include <stdexcept>

#include <vulkan/vulkan.h>

namespace vgfx
{
    CommandBufferFactory::CommandBufferFactory(Context& context, uint32_t queueFamilyIndex)
        : m_context(context)
        , m_queueFamilyIndex(queueFamilyIndex)
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_queueFamilyIndex;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkResult result =
            vkCreateCommandPool(
                context.getLogicalDevice(),
                &poolInfo,
                context.getAllocationCallbacks(),
                &m_commandPool);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create command pool for CommandBufferFactory.");
        }
    }

    CommandBufferFactory::~CommandBufferFactory()
    {
        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(
                m_context.getLogicalDevice(),
                m_commandPool,
                m_context.getAllocationCallbacks());
        }
    }

    VkCommandBuffer CommandBufferFactory::createCommandBuffer(VkCommandBufferLevel level)
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = level;
        allocInfo.commandPool = m_commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkResult result = vkAllocateCommandBuffers(m_context.getLogicalDevice(), &allocInfo, &commandBuffer);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocated command buffer from CommandBufferFactory!");
        }

        return commandBuffer;
    }

    void CommandBufferFactory::freeCommandBuffer(VkCommandBuffer buffer)
    {
        if (buffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(
                m_context.getLogicalDevice(),
                m_commandPool,
                1, &buffer);
        }
    }
}
