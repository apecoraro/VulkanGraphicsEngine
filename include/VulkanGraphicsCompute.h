#ifndef VGFX_COMPUTE_H
#define VGFX_COMPUTE_H

#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsProgram.h"

#include <vector>

namespace vgfx
{
    class ComputePipeline;

    class ComputeShader
    {
    public:
        ComputeShader(
            const Program& computeProgram,
            DescriptorSetLayouts&& descriptorSetLayouts);
        ComputeShader(
            const Program& computeProgram,
            const std::vector<VkPushConstantRange>& pushConstantRanges,
            DescriptorSetLayouts&& descriptorSetLayouts);
        ~ComputeShader() = default;

        const Program& getProgram() const { return m_computeProgram; }
        const std::vector<VkPushConstantRange>& getPushConstantRanges() const { return m_pushConstantRanges; }
        const DescriptorSetLayouts& getDescriptorSetLayouts() const { return m_descriptorSetLayouts; }

    private:
        const Program& m_computeProgram;
        std::vector<VkPushConstantRange> m_pushConstantRanges;
        DescriptorSetLayouts m_descriptorSetLayouts;
        mutable const ComputePipeline* m_pComputePipeline = nullptr;
        friend class ComputePipeline;
        // This function called by the ComputePipeline that gets created from this ComputeShader
        void setComputePipeline(const ComputePipeline& computePipeline) const { m_pComputePipeline = &computePipeline; }
    };

    class ComputePipeline
    {
    public:
        ComputePipeline(
            Context& context,
            const ComputeShader& computeShader);
        ~ComputePipeline();

        VkPipelineLayout getLayout() const { return m_pipelineLayout; }
        VkPipeline getHandle() const { return m_pipeline; }

    private:
        Context& m_context;
        const ComputeShader& m_computeShader;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;
    };
}

#endif