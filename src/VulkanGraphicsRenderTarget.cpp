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
        const Config& config,
        const RenderPass& renderPassTemplate)
        : m_context(context)
    {
        assert(!config.attachmentChain.empty());

        // The max image count determines how multi-buffered this render target
        // can be, but some color attachmentChain might not need to be buffered, if
        // their contents are only temporary (i.e. scratch space) or if image
        // barriers are used to prevent race conditions.
        m_framebuffers.resize(config.attachmentChain.size());

        m_hasDepthStencilBuffer = config.attachmentChain.front().pDepthStencilView != nullptr;

        std::vector<VkImageView> imageViewAttachments(
            config.attachmentChain.front().targetImageViews.size() + (m_hasDepthStencilBuffer ? 1 : 0));

        std::vector<VkImageView> lastImageViewAttachments;
        for (size_t i = 0; i < m_framebuffers.size(); ++i) {
            size_t attachmentIndex = 0;
            for (const auto& pTargetImageView : config.attachmentChain[i].targetImageViews) {
                imageViewAttachments[attachmentIndex] = pTargetImageView->getHandle();
                ++attachmentIndex;
            }

            if (config.attachmentChain[i].pDepthStencilView != nullptr) {
                imageViewAttachments[attachmentIndex] = config.attachmentChain[i].pDepthStencilView->getHandle();
            }

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPassTemplate.getHandle();
            framebufferInfo.attachmentCount = static_cast<uint32_t>(imageViewAttachments.size());
            framebufferInfo.pAttachments = imageViewAttachments.data();
            framebufferInfo.width = config.targetExtent.width;
            framebufferInfo.height = config.targetExtent.height;
            framebufferInfo.layers = 1;

            VkDevice device = m_context.getLogicalDevice();
            VkAllocationCallbacks* pCallbacks = m_context.getAllocationCallbacks();
            if (vkCreateFramebuffer(device, &framebufferInfo, pCallbacks, &m_framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }

            std::vector<VkImageView> tmp;
            tmp.swap(imageViewAttachments);
            imageViewAttachments.swap(lastImageViewAttachments);
            lastImageViewAttachments.swap(tmp);
        }
        m_renderExtent = config.targetExtent;
    }
 
    void RenderTarget::destroy()
    {
        VkDevice device = m_context.getLogicalDevice();
        VkAllocationCallbacks* pAllocationCallbacks = m_context.getAllocationCallbacks();
        if (!m_framebuffers.empty()) {
            assert(device != VK_NULL_HANDLE && "Memory leak in RenderTarget!");

            for (auto& framebuffer : m_framebuffers) {
                vkDestroyFramebuffer(device, framebuffer, pAllocationCallbacks);
            }

            m_framebuffers.clear();
        }
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

    void RenderTargetBuilder::addDepthStencilBuffer(const DepthStencilBuffer& depthStencilBuffer, VkFormat renderFormat, size_t chainIndex)
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

    std::unique_ptr<RenderTarget> RenderTargetBuilder::createRenderTarget(Context& context, const RenderPass& renderPassTemplate)
    {
        return std::make_unique<RenderTarget>(context, m_config, renderPassTemplate);
    }
}

