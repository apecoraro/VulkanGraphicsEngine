#include "VulkanGraphicsRenderTarget.h"

#include "VulkanGraphicsDepthStencilBuffer.h"
#include "VulkanGraphicsRenderPass.h"
#include "VulkanGraphicsSwapChain.h"

#include <cassert>
#include <stdexcept>

namespace vgfx
{
    RenderTarget::RenderTarget(
        Context& context,
        Config&& config)
        : m_context(context)
        , m_config(config)
    {
        assert(!m_config.attachmentChain.empty());
    }
 
    void RenderTargetBuilder::addRenderImage(const Image& image, VkFormat renderFormat, size_t chainIndex)
    {
        if (m_config.attachmentChain.empty()) {
            m_config.targetExtent.width = image.getWidth();
            m_config.targetExtent.height = image.getHeight();
        }

        assert(m_config.targetExtent.width == image.getWidth() && m_config.targetExtent.height == image.getHeight());

        assert(chainIndex <= m_config.attachmentChain.size());
        if (chainIndex == m_config.attachmentChain.size()) {
            m_config.attachmentChain.resize(chainIndex + 1);
        }

        ImageView::Config imageViewConfig(
            renderFormat == VK_FORMAT_UNDEFINED ? image.getFormat() : renderFormat,
            VK_IMAGE_VIEW_TYPE_2D);
        m_config.attachmentChain[chainIndex].targetImageViews.push_back(&image.getOrCreateView(imageViewConfig));
    }

    void RenderTargetBuilder::addAttachmentChain(
            const std::vector<std::unique_ptr<Image>>& imageChain,
            const DepthStencilBuffer* pOptionalDepthStencilBuffer=nullptr,
            VkFormat overrideImageRenderFormat = VK_FORMAT_UNDEFINED,
            VkFormat overrideDepthStencilRenderFormat = VK_FORMAT_UNDEFINED)
    {
        for (const auto& image : imageChain) {
            addRenderImage(*image.get(), overrideImageRenderFormat);
            if (pOptionalDepthStencilBuffer != nullptr) {
                attachDepthStencilBuffer(*pOptionalDepthStencilBuffer);
            }
        }
    }

    void RenderTargetBuilder::attachDepthStencilBuffer(const DepthStencilBuffer& depthStencilBuffer, VkFormat renderFormat, size_t chainIndex)
    {
        if (m_config.attachmentChain.empty()) {
            m_config.targetExtent.width = depthStencilBuffer.getWidth();
            m_config.targetExtent.height = depthStencilBuffer.getHeight();
        }

        assert(m_config.targetExtent.width == depthStencilBuffer.getWidth() && m_config.targetExtent.height == depthStencilBuffer.getHeight());

        assert(chainIndex <= m_config.attachmentChain.size());
        if (chainIndex == m_config.attachmentChain.size()) {
            m_config.attachmentChain.resize(chainIndex + 1);
        }

        ImageView::Config viewConfig(
            renderFormat == VK_FORMAT_UNDEFINED ? depthStencilBuffer.getFormat() : renderFormat,
            VK_IMAGE_VIEW_TYPE_2D);
        m_config.attachmentChain[chainIndex].pDepthStencilView = &depthStencilBuffer.getOrCreateImageView(viewConfig);
    }

    std::unique_ptr<RenderTarget> RenderTargetBuilder::createRenderTarget(Context& context)
    {
        return std::make_unique<RenderTarget>(context, std::move(m_config));
    }
}

