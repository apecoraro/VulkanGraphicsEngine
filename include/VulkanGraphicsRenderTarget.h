#pragma once

#include "VulkanGraphicsContext.h"

#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Image;
    class ImageView;
    class DepthStencilBuffer;

    // Encapsulates a frame buffer i.e. images and optionally depth buffer that can be
    // used for dynamic rendering.
    class RenderTarget
    {
    public:

        RenderTarget() = default;

        void addRenderImage(
            const Image& image,
            VkFormat renderFormat = VK_FORMAT_UNDEFINED);

        void attachDepthStencilBuffer(
            const DepthStencilBuffer& depthStencilBuffer,
            VkFormat renderFormat = VK_FORMAT_UNDEFINED);

        void addImagesAndAttachDepthStencilBuffer(
            const std::vector<std::unique_ptr<Image>>& imageTargets,
            const DepthStencilBuffer* pOptionalDepthStencilBuffer=nullptr,
            VkFormat overrideImageRenderFormat = VK_FORMAT_UNDEFINED,
            VkFormat overrideDepthStencilRenderFormat = VK_FORMAT_UNDEFINED);

        bool hasDepthStencilBuffer() const { return m_pDepthStencilView != nullptr; }

        const VkExtent2D& getExtent() const { return m_targetExtent; }

        size_t getAttachmentCount() const {
            return m_colorAttachmentViews.size();
        }

        const ImageView& getAttachmentView(size_t index) const {
            return *m_colorAttachmentViews[index];
        }

        const ImageView* getDepthStencilView() const {
            return m_pDepthStencilView;
        }

    protected:
        std::vector<ImageView*> m_colorAttachmentViews;
        ImageView* m_pDepthStencilView = nullptr;
        VkExtent2D m_targetExtent = {};
    };
}

