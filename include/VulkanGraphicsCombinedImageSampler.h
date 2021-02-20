#ifndef VGFX_COMBINED_IMAGE_SAMPLER_H
#define VGFX_COMBINED_IMAGE_SAMPLER_H

#include "VulkanGraphicsImageView.h"
#include "VulkanGraphicsSampler.h"

namespace vgfx
{
    class CombinedImageSampler
    {
    public:
        CombinedImageSampler(
            ImageView& imageView,
            Sampler& sampler)
            : m_imageView(imageView)
            , m_sampler(sampler)
        {

        }

        ~CombinedImageSampler() = default;

        ImageView& getImageView() { return m_imageView; }
        Sampler& getSampler() { return m_sampler; }

    private:
        ImageView& m_imageView;
        Sampler& m_sampler;
    };
}

#endif
