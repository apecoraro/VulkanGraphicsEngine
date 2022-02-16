#pragma once

#include "VulkanGraphicsCompute.h"
#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsImage.h"
#include "VulkanGraphicsImageDescriptorUpdaters.h"
#include "VulkanGraphicsImageView.h"
#include "VulkanGraphicsProgram.h"
#include "VulkanGraphicsSampler.h"

#include <cstdint>

namespace vgfx
{
    class ImageSharpener
    {
    public:
        struct ResolutionInfo
        {
            const char* pName;
            uint32_t Width;
            uint32_t Height;
        };

        // 4D Vector; 32 bit unsigned integer components
        struct Vec4u32
        {
            uint32_t x;
            uint32_t y;
            uint32_t z;
            uint32_t w;

            Vec4u32() = default;

            Vec4u32(const Vec4u32&) = default;
            Vec4u32& operator=(const Vec4u32&) = default;

            Vec4u32(Vec4u32&&) = default;
            Vec4u32& operator=(Vec4u32&&) = default;

            constexpr Vec4u32(uint32_t _x, uint32_t _y, uint32_t _z, uint32_t _w) : x(_x), y(_y), z(_z), w(_w) {}
        };

        struct Constants
        {
            Vec4u32 const0;
            Vec4u32 const1;
        };

        ImageSharpener(Context& context, uint32_t swapChainImageCount, float sharpnessVal);
        ~ImageSharpener() = default;

        const ComputeShader& getComputeShader() const { return *m_spSharpenCS.get(); }

        void createRenderingResources(DescriptorPool& pool);
    
        void updateSharpness(float sharpnessVal);

        void recordCommandBuffer(
            VkCommandBuffer cmdBuffer,
            size_t swapChainIndex,
            const Image& inputImage,
            const ImageView& inputImageView,
            Image& outputImage,
            ImageView& outputImageView);

        static void GetSupportedResolutions(
            uint32_t displayWidth,
            uint32_t displayHeight,
            std::vector<ResolutionInfo>* pSupportedList);

    private:
        Context& m_context;
        uint32_t m_swapChainImageCount = 0u;
        std::unique_ptr<Program> m_spComputeProgram;
        std::unique_ptr<ComputeShader> m_spSharpenCS;
        std::unique_ptr<ComputePipeline> m_spSharpenPipeline;

        std::vector<DescriptorSet> m_descriptorSets;
        std::map<uint32_t, DescriptorUpdater*> m_descriptorUpdaters;

        float m_sharpnessValue = 0.0f;
        bool m_computeConstants = true;

        uint32_t m_curInputWidth = 0u;
        uint32_t m_curInputHeight = 0u;

        uint32_t m_curOutputWidth = 0u;
        uint32_t m_curOutputHeight = 0u;

        Constants m_constants;
    };
}

