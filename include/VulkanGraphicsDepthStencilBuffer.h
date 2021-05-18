#ifndef VGFX_DEPTH_STENCIL_BUFFER_H
#define VGFX_DEPTH_STENCIL_BUFFER_H

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsImage.h"
#include "VulkanGraphicsImageView.h"

#include <memory>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class DepthStencilBuffer
    {
    public:
        DepthStencilBuffer(Context& context, const Image::Config& config);

        VkFormat getFormat() const { return m_spImage->getFormat(); }
        VkSampleCountFlagBits getSampleCount() const { return m_spImage->getSampleCount(); }
        const std::unique_ptr<ImageView>& getImageView() const { return m_spImageView; }

    private:
        std::unique_ptr<Image> m_spImage;
        std::unique_ptr<ImageView> m_spImageView;
    };
}

#endif
