#include "VulkanGraphicsImageDownsampler.h"

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsDescriptorPoolBuilder.h"
#include "VulkanGraphicsImageDescriptorUpdaters.h"

#define A_CPU
#include "ffx_a.h"
#include "ffx_spd.h"

namespace vgfx
{
    ImageDownsampler::ImageDownsampler(
        Context& context,
        Precision precision)
        : m_context(context)
    {
        std::string computeShaderPath = context.getAppConfig().dataDirectoryPath;
        computeShaderPath += "/";
        if (precision == Precision::FP16) {
            computeShaderPath += "SPDIntegrationLinearSamplerFloat16.spv";
        } else { // DownsamplePrecision::FP32
            computeShaderPath += "SPDIntegrationLinearSamplerFloat32.spv";
        }
        m_spComputeProgram =
            Program::CreateFromFile(
                computeShaderPath,
                context,
                Program::Type::Compute,
                "main");

        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(LinearSamplerConstants);
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        std::vector<VkPushConstantRange> pushConstantRanges = { pushConstantRange };

        DescriptorSetLayout::DescriptorBindings layoutBindings;
        layoutBindings[0] =
            DescriptorSetLayout::DescriptorBinding(
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                VK_SHADER_STAGE_COMPUTE_BIT,
                VGFX_DOWNSAMPLER_MAX_MIP_LEVELS + 1,
                DescriptorSetLayout::BindMode::SupportPartialBinding);

        layoutBindings[1] =
            DescriptorSetLayout::DescriptorBinding(
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                VK_SHADER_STAGE_COMPUTE_BIT);

        layoutBindings[2] =
            DescriptorSetLayout::DescriptorBinding(
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                VK_SHADER_STAGE_COMPUTE_BIT);

        // bind source texture as sampled image and sampler
        layoutBindings[3] =
            DescriptorSetLayout::DescriptorBinding(
                VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                VK_SHADER_STAGE_COMPUTE_BIT);

        layoutBindings[4] =
            DescriptorSetLayout::DescriptorBinding(
                VK_DESCRIPTOR_TYPE_SAMPLER,
                VK_SHADER_STAGE_COMPUTE_BIT);

        DescriptorSetLayouts descriptorSetLayouts;
        descriptorSetLayouts.push_back(
            std::make_unique<DescriptorSetLayout>(context, layoutBindings));

        m_spComputeShader =
            std::make_unique<ComputeShader>(
                *m_spComputeProgram.get(),
                pushConstantRanges,
                std::move(descriptorSetLayouts));

        m_spComputePipeline = std::make_unique<ComputePipeline>(context, *m_spComputeShader.get());

        DescriptorPoolBuilder descriptorPoolBuilder;
        descriptorPoolBuilder.addDescriptorSets(m_spComputeShader->getDescriptorSetLayouts(), 3);
        m_spDescriptorPool = descriptorPoolBuilder.createPool(context);

        m_spDescriptorPool->allocateDescriptorSets(
            *m_spComputeShader->getDescriptorSetLayouts().front(),
            1u, // Only one copy needed
            &m_descriptorSet);


        Sampler::Config samplerConfig(
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_MIPMAP_MODE_NEAREST,
            -1000.0f, // minLod
            1000.0f, // maxLod
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            true, // enable anisotropy filter
            1.0f); // max anisotropy
        m_spSampler = std::make_unique<Sampler>(context, samplerConfig);

        // Create global atomic counter
        size_t globalCounterBufSize = sizeof(int32_t);
        Buffer::Config globalCounterBufferCfg("SpdGlobalAtomicCounter", globalCounterBufSize);

        m_spGlobalCounterUBO =
            std::make_unique<Buffer>(
                context,Buffer::Type::StorageBuffer, globalCounterBufferCfg);

        // initialize global atomic counter to 0
        uint32_t zero = 0u;
        m_spGlobalCounterUBO->update(&zero, sizeof(zero), 0u, Buffer::MemMap::UnMap);
    }

    void ImageDownsampler::execute(const Image& image, OneTimeCommandsHelper& commandsHelper)
    {
        // We need one UAV less because source texture will be bound as shader resource view (RSV)
        // and not as UAV
        uint32_t numUAVs = image.getMipLevels() - 1u;

        // Anything smaller than 6 mips should just use standard method for downsampling.
        assert(numUAVs >= 6);

        m_spUAVs.resize(numUAVs);
        for (uint32_t i = 0; i < numUAVs; ++i) {
            ImageView::Config config(
                image.getFormat(),
                VK_IMAGE_VIEW_TYPE_2D,
                i + 1u, // mip level
                1u); // mip count
            m_spUAVs[i] = std::make_unique<ImageView>(m_context, config, image);
        }

        // update descriptors
        ImageArrayDescriptorUpdater mipImageViewUpdater(
            m_spUAVs, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL);

        DescriptorSetUpdater descriptorSetUpdater;
        descriptorSetUpdater.bindDescriptor(0, mipImageViewUpdater);

        ImageDescriptorUpdater sixthImageUpdater(
            *m_spUAVs[5].get(), VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL);

        descriptorSetUpdater.bindDescriptor(1, sixthImageUpdater);

        descriptorSetUpdater.bindDescriptor(2, *m_spGlobalCounterUBO.get());

        ImageView::Config sourceCfg(
            image.getFormat(),
            VK_IMAGE_VIEW_TYPE_2D,
            0u, // mip level
            1u); // mip count
        m_spSourceSRV = std::make_unique<ImageView>(m_context, sourceCfg, image);

        ImageDescriptorUpdater sourceSRVUpdater(
            *m_spSourceSRV.get(), VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        descriptorSetUpdater.bindDescriptor(3, sourceSRVUpdater);

        SamplerDescriptorUpdater samplerUpdater(*m_spSampler.get());

        descriptorSetUpdater.bindDescriptor(4, samplerUpdater);

        descriptorSetUpdater.updateDescriptorSet(m_context, m_descriptorSet);

        commandsHelper.execute([&](VkCommandBuffer commandBuffer) {
            // downsample
            varAU2 (dispatchThreadGroupCountXY);
            varAU2 (workGroupOffset);
            varAU2 (numWorkGroupsAndMips);
            varAU4 (rectInfo) = initAU4(0, 0, image.getWidth(), image.getHeight());
            SpdSetup(dispatchThreadGroupCountXY, workGroupOffset, numWorkGroupsAndMips, rectInfo);

            constexpr const uint32_t numBarriers = 2u;
            VkImageMemoryBarrier imageMemoryBarriers[numBarriers];

            imageMemoryBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarriers[0].pNext = NULL;
            imageMemoryBarriers[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            imageMemoryBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            imageMemoryBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageMemoryBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarriers[0].subresourceRange.baseMipLevel = 1u;
            imageMemoryBarriers[0].subresourceRange.levelCount = image.getMipLevels() - 1u;
            imageMemoryBarriers[0].subresourceRange.baseArrayLayer = 0;
            imageMemoryBarriers[0].subresourceRange.layerCount = 1u;
            imageMemoryBarriers[0].image = image.getHandle();

            imageMemoryBarriers[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarriers[1].pNext = NULL;
            imageMemoryBarriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            imageMemoryBarriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            imageMemoryBarriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageMemoryBarriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageMemoryBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarriers[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarriers[1].subresourceRange.baseMipLevel = 0u;
            imageMemoryBarriers[1].subresourceRange.levelCount = 1u;
            imageMemoryBarriers[1].subresourceRange.baseArrayLayer = 0u;
            imageMemoryBarriers[1].subresourceRange.layerCount = 1u;
            imageMemoryBarriers[1].image = image.getHandle();

            // transition LOD 0 to shader read, transition rest of LODs to general layout.
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, // dependency flags
                0, // mem barrier count
                nullptr, // mem barriers
                0, // buffer mem barrier count
                nullptr, // buffer mem barriers
                numBarriers,
                imageMemoryBarriers);

            vkCmdBindPipeline(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                m_spComputePipeline->getHandle());

            // should be / 64
            uint32_t dispatchX = dispatchThreadGroupCountXY[0];
            uint32_t dispatchY = dispatchThreadGroupCountXY[1];
            uint32_t dispatchZ = 1u;

            VkDescriptorSet descriptorSets[] = {
                m_descriptorSet,
            };
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                m_spComputePipeline->getLayout(),
                0,
                1,
                descriptorSets,
                0,
                nullptr);

            LinearSamplerConstants data;
            data.numWorkGroupsPerSlice = numWorkGroupsAndMips[0];
            data.mips = numWorkGroupsAndMips[1];
            data.workGroupOffset[0] = workGroupOffset[0];
            data.workGroupOffset[1] = workGroupOffset[1];
            data.invInputSize[0] = 1.0f / image.getWidth();
            data.invInputSize[1] = 1.0f / image.getHeight();
            vkCmdPushConstants(
                commandBuffer,
                m_spComputePipeline->getLayout(),
                VK_SHADER_STAGE_COMPUTE_BIT,
                0,
                sizeof(LinearSamplerConstants),
                static_cast<void*>(&data));

            vkCmdDispatch(commandBuffer, dispatchX, dispatchY, dispatchZ);

            imageMemoryBarriers[0] = {};
            imageMemoryBarriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarriers[0].pNext = NULL;
            imageMemoryBarriers[0].srcAccessMask = 0;
            imageMemoryBarriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
            imageMemoryBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            imageMemoryBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageMemoryBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageMemoryBarriers[0].subresourceRange.baseMipLevel = 1u;
            imageMemoryBarriers[0].subresourceRange.levelCount = image.getMipLevels() - 1u;
            imageMemoryBarriers[0].subresourceRange.baseArrayLayer = 0u;
            imageMemoryBarriers[0].subresourceRange.layerCount = 1u;
            imageMemoryBarriers[0].image = image.getHandle();

            // transition general layout of destination image to shader read only for source image
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                imageMemoryBarriers);
        });
    }
}