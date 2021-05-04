#include "VulkanGraphicsCompute.h"

#include <stdexcept>

namespace vgfx
{
    ComputeShader::ComputeShader(
        const Program& computeProgram,
        const std::vector<VkPushConstantRange>& pushConstantRanges,
        DescriptorSetLayouts&& descriptorSetLayouts)
        : m_computeProgram(computeProgram)
        , m_pushConstantRanges(pushConstantRanges)
        , m_descriptorSetLayouts(std::move(descriptorSetLayouts))
    {
    }

    ComputePipeline::ComputePipeline(
        Context& context,
        const ComputeShader& computeShader)
        : m_context(context)
        , m_computeShader(computeShader)
    {
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.pNext = NULL;

        // push constants
        pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(m_computeShader.getPushConstantRanges().size());
        pipelineLayoutCreateInfo.pPushConstantRanges = m_computeShader.getPushConstantRanges().data();

        pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(m_computeShader.getDescriptorSetLayouts().size());
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
        for (const auto& descSetLayoutInfo : m_computeShader.getDescriptorSetLayouts()) {
            descriptorSetLayouts.push_back(descSetLayoutInfo.spDescriptorSetLayout->getHandle());
        }
        pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();

        VkResult result =
            vkCreatePipelineLayout(
                context.getLogicalDevice(),
                &pipelineLayoutCreateInfo,
                context.getAllocationCallbacks(),
                &m_pipelineLayout);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline layout!");
        }

        VkComputePipelineCreateInfo pipelineCreateInfo = {};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.pNext = NULL;
        pipelineCreateInfo.flags = 0;
        pipelineCreateInfo.layout = m_pipelineLayout;

        pipelineCreateInfo.stage = {};
        pipelineCreateInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineCreateInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineCreateInfo.stage.module = m_computeShader.getProgram().getShaderModule();
        pipelineCreateInfo.stage.pName = m_computeShader.getProgram().getEntryPointFunction().c_str();

        pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineCreateInfo.basePipelineIndex = 0;

        result = vkCreateComputePipelines(
            context.getLogicalDevice(),
            VK_NULL_HANDLE,
            1,
            &pipelineCreateInfo,
            context.getAllocationCallbacks(),
            &m_pipeline);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create compute pipeline!");
        }

        m_computeShader.setComputePipeline(*this);
    }

    ComputePipeline::~ComputePipeline()
    {
        VkDevice device = m_context.getLogicalDevice();
        VkAllocationCallbacks* pAllocationCallbacks = m_context.getAllocationCallbacks();

        if (m_pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, m_pipeline, pAllocationCallbacks);
            m_pipeline = VK_NULL_HANDLE;
        }

        if (m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, m_pipelineLayout, pAllocationCallbacks);
            m_pipelineLayout = VK_NULL_HANDLE;
        }
    }
}