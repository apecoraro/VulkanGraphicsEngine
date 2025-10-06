#include "VulkanGraphicsRenderTarget.h"

#include "VulkanGraphicsDepthStencilBuffer.h"

#include <cassert>
#include <stdexcept>

namespace vgfx
{
    void RenderTarget::addRenderImage(const Image& image, VkFormat renderFormat)
    {
        if (m_targetExtent.width <= 0 || m_targetExtent.height <= 0) {
            m_targetExtent.width = image.getWidth();
            m_targetExtent.height = image.getHeight();
            assert(m_targetExtent.width > 0 && m_targetExtent.height > 0);
        }

        assert(m_targetExtent.width == image.getWidth() && m_targetExtent.height == image.getHeight());

        ImageView::Config imageViewConfig(
            renderFormat == VK_FORMAT_UNDEFINED ? image.getFormat() : renderFormat,
            VK_IMAGE_VIEW_TYPE_2D);
        m_colorAttachmentViews.push_back(&image.getOrCreateView(imageViewConfig));
    }

    void RenderTarget::addImagesAndAttachDepthStencilBuffer(
            const std::vector<std::unique_ptr<Image>>& imageAttachments,
            const DepthStencilBuffer* pOptionalDepthStencilBuffer,
            VkFormat overrideImageRenderFormat,
            VkFormat overrideDepthStencilRenderFormat)
    {
        for (const auto& image : imageAttachments) {
            addRenderImage(*image.get(), overrideImageRenderFormat);
            if (pOptionalDepthStencilBuffer != nullptr) {
                attachDepthStencilBuffer(*pOptionalDepthStencilBuffer);
            }
        }
    }

    void RenderTarget::attachDepthStencilBuffer(const DepthStencilBuffer& depthStencilBuffer, VkFormat renderFormat)
    {
        if (m_targetExtent.width <= 0 || m_targetExtent.height <= 0) {
            m_targetExtent.width = depthStencilBuffer.getWidth();
            m_targetExtent.height = depthStencilBuffer.getHeight();
            assert(m_targetExtent.width > 0 && m_targetExtent.height > 0);
        }

        assert(m_targetExtent.width == depthStencilBuffer.getWidth()
            && m_targetExtent.height == depthStencilBuffer.getHeight());

        ImageView::Config viewConfig(
            renderFormat == VK_FORMAT_UNDEFINED ? depthStencilBuffer.getFormat() : renderFormat,
            VK_IMAGE_VIEW_TYPE_2D);
        m_pDepthStencilView = &depthStencilBuffer.getOrCreateImageView(viewConfig);
    }
}

