#include "VulkanGraphicsFence.h"

#include <stdexcept>

namespace vgfx
{
    Fence::Fence(Context& context) : m_context(context)
    {
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkResult result = vkCreateFence(
            m_context.getLogicalDevice(),
            &fenceInfo,
            m_context.getAllocationCallbacks(),
            &m_fence);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate fence!");
        }
    }

    Fence::~Fence()
    {
        if (m_fence != VK_NULL_HANDLE) {
            vkDestroyFence(m_context.getLogicalDevice(), m_fence, m_context.getAllocationCallbacks());
        }
    }
}
