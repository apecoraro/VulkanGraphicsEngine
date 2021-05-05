#include "VulkanGraphicsRenderTarget.h"

#include "VulkanGraphicsDepthStencilBuffer.h"
#include "VulkanGraphicsSwapChain.h"

#include <cassert>
#include <stdexcept>

namespace vgfx
{
    RenderTarget::RenderTarget(
        Context& context,
        const SwapChain& swapChain,
        const DepthStencilBuffer* pDepthStencilBuffer,
        const Config& config)
        : m_context(context)
        , m_pDepthStencilBuffer(pDepthStencilBuffer)
    {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = swapChain.getImageFormat();
        colorAttachment.samples = swapChain.getSampleCount();

        colorAttachment.loadOp = config.colorAttachmentLoadOp;
        colorAttachment.storeOp = config.colorAttachmentStoreOp;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = config.colorAttachmentInitialLayout;
        colorAttachment.finalLayout = config.colorAttachmentFinalLayout;

        VkAttachmentReference colorAttachmentRef = {};
        m_colorAttachmentIndex = 0u;
        colorAttachmentRef.attachment = m_colorAttachmentIndex;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Setup the primary rendering pass color attachment info.
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkAccessFlags dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // Setup the primary rendering pass depth stencil attachment info.
        VkAttachmentDescription depthStencilAttachment = {};
        VkAttachmentReference depthStencilAttachmentRef = {};

        uint32_t attachmentCount = 1;

        if (m_pDepthStencilBuffer != nullptr) {
            depthStencilAttachment.format = m_pDepthStencilBuffer->getFormat();
            depthStencilAttachment.samples = swapChain.getSampleCount();

            depthStencilAttachment.loadOp = config.depthAttachmentLoadOp;
            depthStencilAttachment.storeOp = config.depthAttachmentStoreOp;
            depthStencilAttachment.stencilLoadOp = config.stencilAttachmentLoadOp;
            depthStencilAttachment.stencilStoreOp = config.stencilAttachmentStoreOp;
            depthStencilAttachment.initialLayout = config.depthStencilAttachmentInitialLayout;
            depthStencilAttachment.finalLayout = config.depthStencilAttachmentFinalLayout;

            m_depthStencilAttachmentIndex = 1; 
            depthStencilAttachmentRef.attachment = m_depthStencilAttachmentIndex;
            depthStencilAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            // Add refererence to the depth/stencil buffer in the subpass.
            subpass.pDepthStencilAttachment = &depthStencilAttachmentRef;

            srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            ++attachmentCount;
        }

        VkAttachmentDescription attachments[] = { colorAttachment, depthStencilAttachment };

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = attachmentCount;
        renderPassInfo.pAttachments = attachments;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;

        dependency.srcStageMask = srcStageMask;
        dependency.srcAccessMask = 0;

        dependency.dstStageMask = dstStageMask;
        dependency.dstAccessMask = dstAccessMask;

        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VkDevice device = context.getLogicalDevice();
        VkAllocationCallbacks* pAllocationCallbacks = context.getAllocationCallbacks();

        if (vkCreateRenderPass(device, &renderPassInfo, pAllocationCallbacks, &m_renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }

        const auto& swapChainImageViews = swapChain.getImageViews();
        m_framebuffers.resize(swapChainImageViews.size());

        auto& swapChainExtent = swapChain.getImageExtent();
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            VkImageView imageViewAttachments[2] = {
                swapChainImageViews[i]->getHandle(),
                m_pDepthStencilBuffer == nullptr ?
                    VK_NULL_HANDLE : m_pDepthStencilBuffer->getImageView()->getHandle(),
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_renderPass;
            framebufferInfo.attachmentCount = attachmentCount;
            framebufferInfo.pAttachments = imageViewAttachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
        m_renderArea.offset = { 0, 0 };
        m_renderArea.extent = swapChainExtent;
    }

    void RenderTarget::beginRenderPass(VkCommandBuffer commandBuffer, size_t swapChainImageIndex)
    {
        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = m_renderPass;
        renderPassBeginInfo.framebuffer = m_framebuffers[swapChainImageIndex % m_framebuffers.size()];
        renderPassBeginInfo.renderArea = m_renderArea;

        VkClearValue clearValues[] = {
          { 0.0f, 0.0f, 0.0f, 0.0f }, // Color
          { 1.0f, 0 }, // Depth/Stencil
        };
        renderPassBeginInfo.pClearValues = clearValues;
        renderPassBeginInfo.clearValueCount = m_pDepthStencilBuffer == nullptr ? 1 : 2;

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); 
    }

    void RenderTarget::endRenderPass(VkCommandBuffer commandBuffer)
    {
        vkCmdEndRenderPass(commandBuffer);
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

        if (m_renderPass != VK_NULL_HANDLE) {
            assert(device != VK_NULL_HANDLE && "Memory leak in RenderTarget!");

            vkDestroyRenderPass(device, m_renderPass, pAllocationCallbacks);

            m_renderPass = VK_NULL_HANDLE;
        }
    }
}

