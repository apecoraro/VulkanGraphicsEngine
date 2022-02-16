#pragma once

#include "VulkanGraphicsContext.h"

#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Image;
    class ImageView;
    class DepthStencilBuffer;
    class RenderPass;

    // Encapsulates a set of VkFramebuffers, i.e. images and optionally depth buffer that can be
    // used by RenderPass instances.
    class RenderTarget
    {
    public:
        // RenderTarget can consist of multiple color buffers and a depth/stencil buffer and
        // optionally multiple copies/chains (e.g. SwapChain) of the buffers.
        struct Config
        {
        public:
            struct Attachments
            {
                std::vector<ImageView*> targetImageViews;
                ImageView* pDepthStencilView;
            };
            std::vector<Attachments> attachmentChain;
            VkExtent2D targetExtent;
        };
        RenderTarget(
            Context& context,
            const Config& config,
            const RenderPass& renderPassTemplate);

        ~RenderTarget()
        {
            destroy();
        }

        bool hasDepthStencilBuffer() const { return m_hasDepthStencilBuffer; }

        VkFramebuffer getFramebuffer(size_t chainIndex) const
        {
            return m_framebuffers[chainIndex % m_framebuffers.size()];
        }
        const VkExtent2D& getExtent() const { return m_renderExtent; }

    protected:
        void destroy();

        Context& m_context;

        std::vector<VkFramebuffer> m_framebuffers;
        bool m_hasDepthStencilBuffer = false;
        VkExtent2D m_renderExtent = {};
    };

    class RenderTargetBuilder
    {
    public:
        RenderTargetBuilder() = default;
        void addRenderImage(const Image& image, VkFormat renderFormat = VK_FORMAT_UNDEFINED, size_t chainIndex=0u);
        void addRenderImageChain(const std::vector<std::unique_ptr<Image>>& imageChain, VkFormat renderFormat = VK_FORMAT_UNDEFINED);
        void addDepthStencilBuffer(const DepthStencilBuffer& depthStencilBuffer, VkFormat = VK_FORMAT_UNDEFINED, size_t chainIndex=0u);
        void addDepthStencilBufferChain(const std::vector<std::unique_ptr<DepthStencilBuffer>>& depthStencilBuffer, VkFormat = VK_FORMAT_UNDEFINED);

        std::unique_ptr<RenderTarget> createRenderTarget(Context& context, const RenderPass& renderPassTemplate);
    private:
        RenderTarget::Config m_config;
    };
}

