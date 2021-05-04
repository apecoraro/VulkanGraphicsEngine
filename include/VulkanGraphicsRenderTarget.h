#ifndef VGFX_RENDER_TARGET_H
#define VGFX_RENDER_TARGET_H

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsDepthStencilBuffer.h"

#include <set>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class SwapChain;
    class DepthStencilBuffer;

    class RenderTarget
    {
    public:
        struct Config
        {
            // Color attachment configuration.
            VkAttachmentLoadOp colorAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            VkAttachmentStoreOp colorAttachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            VkImageLayout colorAttachmentInitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImageLayout colorAttachmentFinalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            // Depth/Stencil buffer attachment configuration.
            using PickDepthStencilFormatFunc = std::function<VkFormat(const std::set<VkFormat>& candidates)>;
            PickDepthStencilFormatFunc pickDepthStencilFormat; // If null, then no depth/stencil buffer.
            VkAttachmentLoadOp depthAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            VkAttachmentStoreOp depthAttachmentStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            VkAttachmentLoadOp stencilAttachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            VkAttachmentStoreOp stencilAttachmentStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            VkImageLayout depthStencilAttachmentInitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImageLayout depthStencilAttachmentFinalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        };
        RenderTarget(
            Context& context,
            const SwapChain& swapChain,
            const Config& config);

        ~RenderTarget()
        {
            destroy();
        }

        VkRenderPass getRenderPass() const { return m_renderPass; }
        VkFramebuffer getFramebuffer(size_t index) const { return m_framebuffers[index]; }

        const DepthStencilBuffer* getDepthStencilBuffer() const { return m_spDepthStencilBuffer.get(); }
    protected:

        void destroy();

        Context& m_context;

        VkRenderPass m_renderPass = VK_NULL_HANDLE;
        uint32_t m_colorAttachmentIndex = 0u;

        struct DepthStencilDeleter
        {
            DepthStencilDeleter() = default;
            void operator()(DepthStencilBuffer*);
        };
        std::unique_ptr<DepthStencilBuffer, DepthStencilDeleter> m_spDepthStencilBuffer;

        std::unique_ptr<DepthStencilBuffer, DepthStencilDeleter> createDepthStencilBuffer(
            const SwapChain& swapChain,
            const Config& config);

        uint32_t m_depthStencilAttachmentIndex = 1u;

        std::vector<VkFramebuffer> m_framebuffers;
    };
}

#endif
