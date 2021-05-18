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
        , m_pDepthStencilBuffer(config.pDepthStencilBuffer)
    {
        assert(!config.swapChains.empty());

        // The max image count determines how multi-buffered this render target
        // can be, but some color attachments might not need to be buffered, if
        // their contents are only temporary (i.e. scratch space) or if image
        // barriers are used to prevent race conditions.
        uint32_t maxImageCount = 0u;
        for (const auto& pSwapChain : config.swapChains) {
            maxImageCount = std::max(maxImageCount, pSwapChain->getImageCount());
        }
        m_framebuffers.resize(maxImageCount);

        auto& swapChainExtent = config.swapChains.front()->getImageExtent();
        for (size_t i = 0; i < m_framebuffers.size(); ++i) {
            std::vector<VkImageView> imageViewAttachments;
            for (const auto& pSwapChain : config.swapChains) {
                assert(pSwapChain->getImageExtent().width == swapChainExtent.width);
                assert(pSwapChain->getImageExtent().height == swapChainExtent.height);

                size_t imageViewIndex = i % pSwapChain->getImageCount();
                imageViewAttachments.push_back(pSwapChain->getImageView(imageViewIndex).getHandle());
            }

            if (m_pDepthStencilBuffer != nullptr) {
                imageViewAttachments.push_back(m_pDepthStencilBuffer->getImageView()->getHandle());
            }

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPassTemplate.getHandle();
            framebufferInfo.attachmentCount = static_cast<uint32_t>(imageViewAttachments.size());
            framebufferInfo.pAttachments = imageViewAttachments.data();
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            VkDevice device = m_context.getLogicalDevice();
            VkAllocationCallbacks* pCallbacks = m_context.getAllocationCallbacks();
            if (vkCreateFramebuffer(device, &framebufferInfo, pCallbacks, &m_framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
        m_renderExtent = swapChainExtent;
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
}

