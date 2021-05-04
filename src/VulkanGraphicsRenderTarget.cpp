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
        const Config& config)
        : m_context(context)
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

        if (config.pickDepthStencilFormat != nullptr) {
            m_spDepthStencilBuffer = createDepthStencilBuffer(swapChain, config);

            depthStencilAttachment.format = m_spDepthStencilBuffer->getFormat();
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
                m_spDepthStencilBuffer == nullptr ?
                    VK_NULL_HANDLE : m_spDepthStencilBuffer->getImageView()->getHandle(),
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
    }

    std::unique_ptr<DepthStencilBuffer, RenderTarget::DepthStencilDeleter>
    RenderTarget::createDepthStencilBuffer(const SwapChain& swapChain, const Config& config)
    {
        VkFormat candidates[] = {
            VK_FORMAT_D16_UNORM,
            VK_FORMAT_X8_D24_UNORM_PACK32,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
        };
        std::set<VkFormat> supportedDepthStencilFormats;
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), format, &props);

            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                supportedDepthStencilFormats.insert(format);
            }
        }

        VkFormat selectedFormat = config.pickDepthStencilFormat(supportedDepthStencilFormats);

        auto& swapChainExtent = swapChain.getImageExtent();
        Image::Config depthImageCfg(
            swapChainExtent.width,
            swapChainExtent.height,
            selectedFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        return std::unique_ptr<DepthStencilBuffer, DepthStencilDeleter>(new DepthStencilBuffer(m_context, depthImageCfg));
    }

    void RenderTarget::DepthStencilDeleter::operator()(DepthStencilBuffer* pDSBuffer)
    {
        if (pDSBuffer != nullptr) {
            delete pDSBuffer;
        }
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

