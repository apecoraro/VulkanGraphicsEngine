#ifndef VGFX_COMBINED_IMAGE_SAMPLER_H
#define VGFX_COMBINED_IMAGE_SAMPLER_H

#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsImageView.h"
#include "VulkanGraphicsSampler.h"

namespace vgfx
{
    class CombinedImageSampler : public DescriptorUpdater
    {
    public:
        CombinedImageSampler(
            ImageView& imageView,
            Sampler& sampler)
            : DescriptorUpdater(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
            , m_imageView(imageView)
            , m_sampler(sampler)
        {
            // TODO should imageLayout not be hard coded?
            m_imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            m_imageInfo.imageView = m_imageView.getHandle();
            m_imageInfo.sampler = m_sampler.getHandle();
        }

        ~CombinedImageSampler() = default;

        ImageView& getImageView() { return m_imageView; }
        Sampler& getSampler() { return m_sampler; }

        void write(VkWriteDescriptorSet* pWriteSet) override
        {
            DescriptorUpdater::write(pWriteSet);

            VkWriteDescriptorSet& writeSet = *pWriteSet;
            // TODO should dstArrayElement not be hard coded?
            writeSet.dstArrayElement = 0;

            writeSet.pImageInfo = &m_imageInfo;
        }

    private:
        ImageView& m_imageView;
        Sampler& m_sampler;
        VkDescriptorImageInfo m_imageInfo = {};
    };
}

#endif
