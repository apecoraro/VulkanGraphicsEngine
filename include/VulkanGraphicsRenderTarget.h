#pragma once

#include "VulkanGraphicsContext.h"

#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Image;
    class ImageView;
    class DepthStencilBuffer;

    // Encapsulates a set of VkFramebuffers, i.e. images and optionally depth buffer that can be
    // used by RenderPass instances.
    class RenderTarget
    {
    public:
        // RenderTarget can consist of multiple color buffers and a depth/stencil buffer and
        // optionally multiple copies/chains (e.g. SwapChain) of the buffers.
        struct AttachmentViews
        {
        public:
            std::vector<ImageView*> targetImageViews;
            ImageView* pDepthStencilView;
        };
        struct Config
        {
        public:
            std::vector<AttachmentViews> attachmentChain;
            VkExtent2D targetExtent;
        };
        RenderTarget(
            Context& context,
            Config&& config);

        bool hasDepthStencilBuffer() const { return m_config.attachmentChain.front().pDepthStencilView != nullptr; }

        const VkExtent2D& getExtent() const { return m_config.targetExtent; }

        const AttachmentViews& getAttachmentChain(size_t index) const { return m_config.attachmentChain[index]; }

    protected:

        Context& m_context;
        Config m_config;
    };

    class RenderTargetBuilder
    {
    public:
        RenderTargetBuilder() = default;
        void addRenderImage(const Image& image, VkFormat renderFormat = VK_FORMAT_UNDEFINED, size_t chainIndex=0u);
        void addAttachmentChain(
            const std::vector<std::unique_ptr<Image>>& imageChain,
            const DepthStencilBuffer* pOptionalDepthStencilBuffer=nullptr,
            VkFormat overrideImageRenderFormat = VK_FORMAT_UNDEFINED,
            VkFormat overrideDepthStencilRenderFormat = VK_FORMAT_UNDEFINED);
        void attachDepthStencilBuffer(const DepthStencilBuffer& depthStencilBuffer, VkFormat renderFormat = VK_FORMAT_UNDEFINED, size_t chainIndex=0u);

        const RenderTarget::Config& getConfig() const { return m_config; }
        std::unique_ptr<RenderTarget> createRenderTarget(Context& context);
    private:
        RenderTarget::Config m_config;
    };
}

