#include "VulkanGraphicsSemaphore.h"

#include <stdexcept>

namespace vgfx
{
    Semaphore::Semaphore(Context& context)
        : m_context(context)
    {
        VkSemaphoreCreateInfo semaphoreInfo = {};

        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkResult result = vkCreateSemaphore(
            context.getLogicalDevice(),
            &semaphoreInfo,
            context.getAllocationCallbacks(),
            &m_semaphore);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate semaphore!");
        }
    }

    Semaphore::~Semaphore()
    {
        if (m_semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(
                m_context.getLogicalDevice(),
                m_semaphore,
                m_context.getAllocationCallbacks());
        }
    }

}
