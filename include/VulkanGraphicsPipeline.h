#pragma once

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsEffects.h"
#include "VulkanGraphicsRenderTarget.h"
#include "VulkanGraphicsVertexBuffer.h"

#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class DepthStencilBuffer;

    class Pipeline
    {
    public:
        Pipeline(
            Context& context,
            const Effect& effect,
            VkPipelineLayout pipelineLayout,
            VkGraphicsPipelineCreateInfo pipelineInfo);

        ~Pipeline()
        {
            destroy();
        }

        VkPipeline getHandle() const { return m_graphicsPipeline; }
        VkPipelineLayout getLayout() const { return m_pipelineLayout; }

        struct InputAssemblyConfig
        {
            InputAssemblyConfig(VkPrimitiveTopology primitiveTopology, bool primRestartEnable)
            {
                inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
                inputAssemblyInfo.topology = primitiveTopology;
                inputAssemblyInfo.primitiveRestartEnable = primRestartEnable;
            }
            VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
        };

        struct RasterizerConfig
        {
            RasterizerConfig(
                VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
                VkCullModeFlagBits cullMode = VK_CULL_MODE_BACK_BIT,
                VkFrontFace cullModeFrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE)
            {
                rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
                rasterizerInfo.depthClampEnable = VK_FALSE;
                rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
                rasterizerInfo.polygonMode = polygonMode;
                rasterizerInfo.lineWidth = 1.0f;
                rasterizerInfo.cullMode = cullMode;
                rasterizerInfo.frontFace = cullModeFrontFace;

                rasterizerInfo.depthBiasEnable = VK_FALSE;
                rasterizerInfo.depthBiasConstantFactor = 0.0f; // Optional
                rasterizerInfo.depthBiasClamp = 0.0f; // Optional
                rasterizerInfo.depthBiasSlopeFactor = 0.0f; // Optional
            }

            VkPipelineRasterizationStateCreateInfo rasterizerInfo = {};
        };

    private:
        void destroy();

        Context& m_context;
        const Effect& m_effect;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    };

    class PipelineBuilder
    {
    public:
        PipelineBuilder(
            const VkViewport& viewport,
            bool useDepthBuffer = false);

        PipelineBuilder& configureDrawableInput(
            const MeshEffect& meshEffect,
            const VertexBuffer::Config& vertexBufferConfig,
            const Pipeline::InputAssemblyConfig& inputAssemblyConfig);

        // DynamicState allows certain settings to be changed "on the fly" (e.g. viewport & scissor).
        PipelineBuilder& configureDynamicStates(const std::vector<VkDynamicState>& dynamicStateEnables);

        // Override the Viewport specified at construction to create new pipeline with similar
        // settings. Note that viewport and scissor can also be changed dynamically without need
        // for a whole new pipeline if the pipeline is configured with configureDynamicStates.
        PipelineBuilder& configureViewport(const VkViewport& viewport)
        {
            m_viewport = viewport;

            return *this;
        }
        // Override default scissor. Default scissor is "no scissor".
        // Note that viewport and scissor can also be changed dynamically without need
        // for a whole new pipeline if the pipeline is configured with configureDynamicStates.
        PipelineBuilder& configureScissor(const VkRect2D& scissor)
        {
            m_scissor = scissor;

            return *this;
        }

        PipelineBuilder& configureRasterizer(const Pipeline::RasterizerConfig& config);

        PipelineBuilder& configureRenderTarget(const RenderTarget::AttachmentViews& attachments);

        std::unique_ptr<Pipeline> createPipeline(Context& context);

    private:
        const MeshEffect* m_pEffect = nullptr;
        VkPipelineShaderStageCreateInfo m_vertShaderStageInfo = {};

        VkVertexInputBindingDescription m_vertexBindingDescription;
        std::vector<VkVertexInputAttributeDescription> m_vertexAttributeDescriptions;
        VkPipelineVertexInputStateCreateInfo m_vertexInputInfo = {};

        VkPipelineInputAssemblyStateCreateInfo m_inputAssembly = {};

        VkPipelineShaderStageCreateInfo m_fragShaderStageInfo = {};

        std::vector<VkPushConstantRange> m_pushConstantRanges;

        std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;

        VkPipelineDepthStencilStateCreateInfo m_depthStencil = {};
        VkViewport m_viewport = {};
        VkRect2D m_scissor = {};

        VkPipelineMultisampleStateCreateInfo m_multisampling = {};
        VkPipelineColorBlendAttachmentState m_colorBlendAttachment = {};
        VkPipelineColorBlendStateCreateInfo m_colorBlending = {};

        VkPipelineRasterizationStateCreateInfo m_rasterizer = {};

        std::vector<VkDynamicState> m_dynamicStateEnables;

        VkPipelineRenderingCreateInfo m_renderingInfo = {};

        uint32_t m_subpass = 0u;
    }; 
}

