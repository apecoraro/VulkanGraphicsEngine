#pragma once

#include "VulkanGraphicsBuffer.h"
#include "VulkanGraphicsCompute.h"
#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsImage.h"
#include "VulkanGraphicsImageView.h"
#include "VulkanGraphicsOneTimeCommands.h"
#include "VulkanGraphicsSampler.h"
#include "VulkanGraphicsBuffer.h"

#include <memory>

namespace vgfx
{
#define VGFX_DOWNSAMPLER_MAX_MIP_LEVELS 12

    class ImageDownsampler
    {
    public:
        enum class Precision
        {
            FP16,
            FP32,
        };

        ImageDownsampler(
            Context& context,
            Precision precision);
        ~ImageDownsampler() = default;

        void execute(const Image& image, OneTimeCommandsHelper& commandsHelper);

    private:
        struct LinearSamplerConstants
        {
            int32_t mips;
            int32_t numWorkGroupsPerSlice;
            int32_t workGroupOffset[2];
            float invInputSize[2];
            float padding[2];
        };

        Context& m_context;

        std::unique_ptr<ComputeShader> m_spComputeShader;
        std::unique_ptr<Program> m_spComputeProgram;
        std::unique_ptr<ComputePipeline> m_spComputePipeline;
        std::unique_ptr<DescriptorPool> m_spDescriptorPool;
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

        std::unique_ptr<Sampler> m_spSampler;
        std::unique_ptr<Buffer> m_spGlobalCounterUBO;
        std::vector<std::unique_ptr<ImageView>> m_spUAVs;
        std::unique_ptr<ImageView> m_spSourceSRV;
    };
}

