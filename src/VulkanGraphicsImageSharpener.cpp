#include "VulkanGraphicsImageSharpener.h"

#define A_CPU
#include "ffx_a.h"
#include "ffx_cas.h"

namespace vgfx
{
    ImageSharpener::ImageSharpener(Context& context, uint32_t swapChainImageCount, float sharpnessVal)
        : m_context(context)
        , m_swapChainImageCount(swapChainImageCount)
        , m_sharpnessValue(sharpnessVal)
    {
        std::string computeShaderPath = context.getAppConfig().dataDirectoryPath;
        computeShaderPath += "/";
        if (context.isFp16Supported()) {
            computeShaderPath += "CAS_ShaderFloat16.spv";
        } else { // DownsamplePrecision::FP32
            computeShaderPath += "CAS_ShaderFloat32.spv";
        }
        m_spComputeProgram =
            Program::CreateFromFile(
                computeShaderPath,
                context,
                Program::Type::Compute,
                "main");

        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(Constants);
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        std::vector<VkPushConstantRange> pushConstantRanges = { pushConstantRange };

        DescriptorSetLayout::DescriptorBindings layoutBindings;
        layoutBindings[0] =
            DescriptorSetLayout::DescriptorBinding(
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                VK_SHADER_STAGE_COMPUTE_BIT);

        layoutBindings[1] =
            DescriptorSetLayout::DescriptorBinding(
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                VK_SHADER_STAGE_COMPUTE_BIT);

        DescriptorSetLayouts descriptorSetLayouts;
        descriptorSetLayouts.push_back(DescriptorSetLayoutInfo(
            std::make_unique<DescriptorSetLayout>(context, layoutBindings),
            swapChainImageCount));

        m_spSharpenCS =
            std::make_unique<ComputeShader>(
                *m_spComputeProgram.get(),
                pushConstantRanges,
                std::move(descriptorSetLayouts));

        m_spSharpenPipeline =
            std::make_unique<ComputePipeline>(context, *m_spSharpenCS.get());

        updateSharpness(sharpnessVal);
    }

    void ImageSharpener::createRenderingResources(DescriptorPool& pool)
    {
        pool.allocateDescriptorSets(
            *m_spSharpenCS->getDescriptorSetLayouts().front().spDescriptorSetLayout.get(),
            m_swapChainImageCount,
            &m_descriptorSets);
    }

    void ImageSharpener::updateSharpness(float sharpnessVal)
    {
        m_sharpnessValue = sharpnessVal;
        m_computeConstants = true;
    }

    void ImageSharpener::recordCommandBuffer(
        VkCommandBuffer cmdBuffer,
        size_t swapChainImageIndex,
        const Image& inputImage,
        const ImageView& inputImageView,
        Image& outputImage,
        ImageView& outputImageView)
    {

        if (m_computeConstants
                || m_curInputWidth != inputImage.getWidth()
                || m_curInputHeight != inputImage.getHeight()
                || m_curOutputWidth != outputImage.getWidth()
                || m_curOutputHeight != outputImage.getHeight()) {
            m_computeConstants = false;
            m_curInputWidth = inputImage.getWidth();
            m_curInputHeight = inputImage.getHeight();
            m_curOutputWidth = outputImage.getWidth();
            m_curOutputHeight = outputImage.getHeight();

            CasSetup(
                reinterpret_cast<AU1*>(&m_constants.const0),
                reinterpret_cast<AU1*>(&m_constants.const1),
                m_sharpnessValue,
                static_cast<AF1>(m_curInputWidth),
                static_cast<AF1>(m_curInputHeight),
                static_cast<AF1>(m_curOutputWidth),
                static_cast<AF1>(m_curOutputHeight));
        }

        ImageDescriptorUpdater inputImageUpdater(
            inputImageView, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL);

        ImageDescriptorUpdater outputImageUpdater(
            outputImageView, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL);

        m_descriptorUpdaters[0] = &inputImageUpdater;
        m_descriptorUpdaters[1] = &outputImageUpdater;

        DescriptorSetUpdater& curDescriptorSet = m_descriptorSets[swapChainImageIndex % m_descriptorSets.size()];

        curDescriptorSet.updateDescriptorSet(m_context, m_descriptorUpdaters);
        
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.image = outputImage.getHandle();
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, // flags
            0, nullptr, // memory barriers
            0, nullptr, // buffer memory barriers
            1, &barrier);

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_spSharpenPipeline->getHandle());

        VkDescriptorSet descriptorSets[] = {
            curDescriptorSet.getHandle(),
        };

        vkCmdBindDescriptorSets(
            cmdBuffer,
            VK_PIPELINE_BIND_POINT_COMPUTE,
            m_spSharpenPipeline->getLayout(),
            0,
            1,
            descriptorSets,
            0,
            nullptr);

        vkCmdPushConstants(
            cmdBuffer,
            m_spSharpenPipeline->getLayout(),
            VK_SHADER_STAGE_COMPUTE_BIT,
            0,
            sizeof(Constants),
            static_cast<void*>(&m_constants));

         // This value is the image region dim that each thread group of the CAS shader operates on
         static const uint32_t threadGroupWorkRegionDim = 16;
         uint32_t dispatchX = (m_curOutputWidth + (threadGroupWorkRegionDim - 1u)) / threadGroupWorkRegionDim;
         uint32_t dispatchY = (m_curOutputHeight + (threadGroupWorkRegionDim - 1u)) / threadGroupWorkRegionDim;

         vkCmdDispatch(cmdBuffer, dispatchX, dispatchY, 1u);

         barrier = {};
         barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
         barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
         barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
         barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
         barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
         barrier.image = outputImage.getHandle();
         barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
         barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
         barrier.subresourceRange.baseMipLevel = 0;
         barrier.subresourceRange.levelCount = 1;
         barrier.subresourceRange.baseArrayLayer = 0;
         barrier.subresourceRange.layerCount = 1;

         vkCmdPipelineBarrier(
             cmdBuffer,
             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
             0, // flags
             0, nullptr, // mem barriers
             0, nullptr, // buffer mem barriers
             1, &barrier);
    }

    static const ImageSharpener::ResolutionInfo s_commonResolutions[] =
    {
        { "480p", 640, 480 },
        { "720p", 1280, 720 },
        { "1080p", 1920, 1080 },
        { "1440p", 2560, 1440 },
    };

    void ImageSharpener::GetSupportedResolutions(
        uint32_t displayWidth,
        uint32_t displayHeight,
        std::vector<ResolutionInfo>* pSupportedList)
    {
        // Check which of the fixed resolutions we support rendering to with CAS enabled
        for (uint32_t iRes = 0; iRes < _countof(s_commonResolutions); ++iRes)
        {
            ResolutionInfo curResolution = s_commonResolutions[iRes];
            if (CasSupportScaling(
                    static_cast<AF1>(displayWidth),
                    static_cast<AF1>(displayHeight),
                    static_cast<AF1>(curResolution.Width),
                    static_cast<AF1>(curResolution.Height))
                && curResolution.Width < displayWidth
                && curResolution.Height < displayHeight)
            {
                pSupportedList->push_back(curResolution);
            }
        }

        // Also add the display res as a supported render resolution
        ResolutionInfo displayResInfo = {};
        displayResInfo.pName = "Display Res";
        displayResInfo.Width = displayWidth;
        displayResInfo.Height = displayHeight;
        pSupportedList->push_back(displayResInfo);
    }
}

