#include "VulkanGraphicsDepthStencilBuffer.h"

#include <cassert>
#include <stdexcept>

namespace vgfx
{
    DepthStencilBuffer::DepthStencilBuffer(
        Context& context, const Image::Config& config)
        : m_spImage(new Image(context, config))
    {
        ImageView::Config viewCfg(config.imageInfo.format, VK_IMAGE_VIEW_TYPE_2D);
        m_spImageView.reset(new ImageView(context, viewCfg, *m_spImage.get()));
    }
}

