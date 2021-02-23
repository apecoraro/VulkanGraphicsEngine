#include "VulkanGraphicsImageView.h"

#include <cassert>
#include <stdexcept>

namespace vgfx
{
    ImageView::ImageView(
        Context& context,
        const Config& config,
        VkImage image)
        : m_context(context)
    {
        VkDevice device = context.getLogicalDevice();
        assert(device != VK_NULL_HANDLE && "Invalid context!");

        VkAllocationCallbacks* pAllocationCallbacks = context.getAllocationCallbacks();

        VkImageViewCreateInfo imageViewInfo = config.imageViewInfo;
        imageViewInfo.image = image;

        if (vkCreateImageView(device, &imageViewInfo, pAllocationCallbacks, &m_imageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view!");
        }
    }

    void ImageView::destroy()
    {
        if (m_imageView != VK_NULL_HANDLE) {
            VkDevice device = m_context.getLogicalDevice();
            assert(device != VK_NULL_HANDLE && "Context disposed before ImageView!");

            VkAllocationCallbacks* pAllocationCallbacks = m_context.getAllocationCallbacks();

            vkDestroyImageView(device, m_imageView, pAllocationCallbacks);

            m_imageView = VK_NULL_HANDLE;
        }
    }
}
