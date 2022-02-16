#pragma once

#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsImageView.h"
#include "VulkanGraphicsSampler.h"

#include <vector>

namespace vgfx
{
    class ImageDescriptorUpdater : public DescriptorUpdater
    {
    public:
        ImageDescriptorUpdater(
            const ImageView& imageView,
            VkDescriptorType descriptorType,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            : DescriptorUpdater(descriptorType)
        {
            m_imageInfo.imageLayout = imageLayout;
            m_imageInfo.imageView = imageView.getHandle();
        }

        ~ImageDescriptorUpdater() = default;

        void write(VkWriteDescriptorSet* pWriteSet) override
        {
            DescriptorUpdater::write(pWriteSet);

            VkWriteDescriptorSet& writeSet = *pWriteSet;
            // TODO should dstArrayElement not be hard coded?
            writeSet.dstArrayElement = 0;

            writeSet.pImageInfo = &m_imageInfo;
        }

    private:
        VkDescriptorImageInfo m_imageInfo = {};
    };

    class ImageArrayDescriptorUpdater : public DescriptorUpdater
    {
    public:

        ImageArrayDescriptorUpdater(
            const std::vector<std::unique_ptr<ImageView>>& imageViews,
            VkDescriptorType descriptorType,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            : DescriptorUpdater(descriptorType)
        {
            m_imageInfoArray.reserve(imageViews.size());
            for (const auto& spImageView : imageViews) {
                VkDescriptorImageInfo imageInfo = {};
                imageInfo.imageView = spImageView->getHandle();
                imageInfo.imageLayout = imageLayout;
                m_imageInfoArray.push_back(imageInfo);
            }
        }

        ~ImageArrayDescriptorUpdater() = default;

        void write(VkWriteDescriptorSet* pWriteSet) override
        {
            DescriptorUpdater::write(pWriteSet);

            VkWriteDescriptorSet& writeSet = *pWriteSet;
            // TODO should dstArrayElement not be hard coded?
            writeSet.dstArrayElement = 0;
            writeSet.descriptorCount = static_cast<uint32_t>(m_imageInfoArray.size());

            writeSet.pImageInfo = m_imageInfoArray.data();
        }

    private:
        std::vector<VkDescriptorImageInfo> m_imageInfoArray;
    };

    class CombinedImageSamplerDescriptorUpdater : public DescriptorUpdater
    {
    public:
        CombinedImageSamplerDescriptorUpdater(
            const ImageView& imageView,
            const Sampler& sampler,
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            : DescriptorUpdater(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            m_imageInfo.imageLayout = imageLayout;
            m_imageInfo.imageView = imageView.getHandle();
            m_imageInfo.sampler = sampler.getHandle();
        }

        ~CombinedImageSamplerDescriptorUpdater() = default;

        void write(VkWriteDescriptorSet* pWriteSet) override
        {
            DescriptorUpdater::write(pWriteSet);

            VkWriteDescriptorSet& writeSet = *pWriteSet;
            // TODO should dstArrayElement not be hard coded?
            writeSet.dstArrayElement = 0;

            writeSet.pImageInfo = &m_imageInfo;
        }

    private:
        VkDescriptorImageInfo m_imageInfo = {};
    };

    class SamplerDescriptorUpdater : public DescriptorUpdater
    {
    public:
        SamplerDescriptorUpdater(const Sampler& sampler)
            : DescriptorUpdater(VK_DESCRIPTOR_TYPE_SAMPLER)
        {
            m_imageInfo.sampler = sampler.getHandle();
        }

        ~SamplerDescriptorUpdater() = default;

        void write(VkWriteDescriptorSet * pWriteSet) override
        {
            DescriptorUpdater::write(pWriteSet);

            VkWriteDescriptorSet& writeSet = *pWriteSet;
            // TODO should dstArrayElement not be hard coded?
            writeSet.dstArrayElement = 0;

            writeSet.pImageInfo = &m_imageInfo;
        }

    private:
        VkDescriptorImageInfo m_imageInfo = {};
    };
}

