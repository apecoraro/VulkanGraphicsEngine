#include "VulkanGraphicsPipeline.h"

#include "VulkanGraphicsDepthStencilBuffer.h"

#include <stdexcept>

namespace vgfx
{
    PipelineBuilder::PipelineBuilder(
        const VkViewport& viewport,
        bool useDepthBuffer)
        : m_viewport(viewport)
    {
        m_depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        if (useDepthBuffer) {
            m_depthStencil.depthTestEnable = VK_TRUE;
            m_depthStencil.depthWriteEnable = VK_TRUE;
            m_depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
            m_depthStencil.depthBoundsTestEnable = VK_FALSE;
            m_depthStencil.minDepthBounds = 0.0f; // Optional
            m_depthStencil.maxDepthBounds = 1.0f; // Optional
            m_depthStencil.stencilTestEnable = VK_FALSE;
            m_depthStencil.front = {}; // Optional
            m_depthStencil.back = {}; // Optional
        }

        m_scissor.offset = { 0, 0 };
        m_scissor.extent = { 
            static_cast<uint32_t>(viewport.width),
            static_cast<uint32_t>(viewport.height)
        };

        Pipeline::RasterizerConfig defaultRasterizerConfig;
        configureRasterizer(defaultRasterizerConfig);

        m_multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        m_multisampling.sampleShadingEnable = VK_FALSE;
        m_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        m_multisampling.minSampleShading = 1.0f; // Optional
        m_multisampling.pSampleMask = nullptr; // Optional
        m_multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        m_multisampling.alphaToOneEnable = VK_FALSE; // Optional

        m_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        m_colorBlendAttachment.blendEnable = VK_FALSE;
        m_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        m_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        m_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        m_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        m_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        m_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

        m_colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        m_colorBlending.logicOpEnable = VK_FALSE;
        m_colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        m_colorBlending.attachmentCount = 1;
        m_colorBlending.pAttachments = &m_colorBlendAttachment;
        m_colorBlending.blendConstants[0] = 0.0f; // Optional
        m_colorBlending.blendConstants[1] = 0.0f; // Optional
        m_colorBlending.blendConstants[2] = 0.0f; // Optional
        m_colorBlending.blendConstants[3] = 0.0f; // Optional
    }

    static VkVertexInputBindingDescription BuildVertexBindingDescription(
        const VertexBuffer::Config& vertexBufferConfig,
        VkVertexInputRate vertexInputRate)
    {
        VkVertexInputBindingDescription bindingDescription = {};
        bindingDescription.binding = 0;
        bindingDescription.stride = vertexBufferConfig.vertexStride;
        bindingDescription.inputRate = vertexInputRate;

        return bindingDescription;
    }

    static std::vector<VkVertexInputAttributeDescription> BuildVertexAttributeDescriptions(
        const VertexBuffer::Config& vertexBufferConfig,
        uint32_t vertexBufferBindingIndex)
    {
        const auto& vtxBufferAttrs = vertexBufferConfig.vertexAttrDescriptions;
        std::vector<VkVertexInputAttributeDescription> vtxAttrDescriptions;
        vtxAttrDescriptions.reserve(vtxBufferAttrs.size());

        uint32_t bindingLocation = 0u;
        for (const auto& vtxBufferAttr : vtxBufferAttrs) {
            VkVertexInputAttributeDescription attributeDescription;
            attributeDescription.binding = vertexBufferBindingIndex;
            attributeDescription.location = bindingLocation;
            attributeDescription.format = vtxBufferAttr.format;
            attributeDescription.offset = vtxBufferAttr.offset;

            vtxAttrDescriptions.push_back(attributeDescription);

            ++bindingLocation;
        }

        return vtxAttrDescriptions;
    }

    PipelineBuilder& PipelineBuilder::configureDrawableInput(
        const MeshEffect& meshEffect,
        const VertexBuffer::Config& vertexBufferConfig,
        // TODO probably need to add some way to provide instance based data (i.e. VK_VERTEX_INPUT_RATE_INSTANCE).
        // This would allow for instanced drawing.
        const Pipeline::InputAssemblyConfig& inputAssemblyConfig)
    {
        m_pEffect = &meshEffect;
        const Program& vertexShaderProgram = meshEffect.getVertexShader();
        assert(vertexShaderProgram.getShaderStage() == VK_SHADER_STAGE_VERTEX_BIT);

        m_vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        m_vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

        m_vertShaderStageInfo.module = vertexShaderProgram.getShaderModule();
        m_vertShaderStageInfo.pName = vertexShaderProgram.getEntryPointFunction().c_str();

        m_vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        
        m_vertexBindingDescription =
            BuildVertexBindingDescription(
                vertexBufferConfig,
                VK_VERTEX_INPUT_RATE_VERTEX);

        m_vertexInputInfo.vertexBindingDescriptionCount = 1;
        m_vertexInputInfo.pVertexBindingDescriptions = &m_vertexBindingDescription;

        m_vertexAttributeDescriptions =
            BuildVertexAttributeDescriptions(
                vertexBufferConfig,
                m_vertexBindingDescription.binding);

        m_vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertexAttributeDescriptions.size());
        m_vertexInputInfo.pVertexAttributeDescriptions = m_vertexAttributeDescriptions.data();

        m_inputAssembly = inputAssemblyConfig.inputAssemblyInfo;

        const Program& fragmentShaderProgram = meshEffect.getFragmentShader();
        assert(fragmentShaderProgram.getShaderStage() == VK_SHADER_STAGE_FRAGMENT_BIT);

        m_fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        m_fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

        m_fragShaderStageInfo.module = fragmentShaderProgram.getShaderModule();
        m_fragShaderStageInfo.pName = fragmentShaderProgram.getEntryPointFunction().c_str();

        m_pushConstantRanges = meshEffect.getPushConstantRanges();

        for (const auto& spDescSetLayout: meshEffect.getDescriptorSetLayouts()) {
            m_descriptorSetLayouts.push_back(spDescSetLayout->getHandle());
        }

        return *this;
    }

    PipelineBuilder& PipelineBuilder::configureRasterizer(const Pipeline::RasterizerConfig& config)
    {
        m_rasterizer = config.rasterizerInfo;

        return *this;
    }

    PipelineBuilder& PipelineBuilder::configureDynamicStates(const std::vector<VkDynamicState>& dynamicStateEnables)
    {
        m_dynamicStateEnables = dynamicStateEnables;

        return *this;
    }

    PipelineBuilder& PipelineBuilder::configureRenderTarget(const RenderTarget& renderTarget)
    {
        m_renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        m_renderingInfo.colorAttachmentCount = static_cast<uint32_t>(renderTarget.getAttachmentCount());

        for (size_t i = 0; i < renderTarget.getAttachmentCount(); ++i) {
            const auto& colorImageView = renderTarget.getAttachmentView(i);
            m_colorAttachmentFormats.push_back(colorImageView.getFormat());
        }

        m_renderingInfo.pColorAttachmentFormats = m_colorAttachmentFormats.data();
        if (renderTarget.hasDepthStencilBuffer()) {
            m_renderingInfo.depthAttachmentFormat = renderTarget.getDepthStencilView()->getFormat();
            m_renderingInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED; // Seems like this should be configurable - TODO
        }

        return *this;
    }

    std::unique_ptr<Pipeline> PipelineBuilder::createPipeline(Context& context)
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(m_descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = m_descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(m_pushConstantRanges.size());
        pipelineLayoutInfo.pPushConstantRanges = m_pushConstantRanges.data();

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkResult result =
            vkCreatePipelineLayout(
                context.getLogicalDevice(),
                &pipelineLayoutInfo,
                context.getAllocationCallbacks(),
                &pipelineLayout);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        constexpr uint32_t kNumShaderStages = 2;
        VkPipelineShaderStageCreateInfo shaderStages[kNumShaderStages] = {
            m_vertShaderStageInfo,
            m_fragShaderStageInfo
        };

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = kNumShaderStages;
        pipelineInfo.pStages = shaderStages;

        pipelineInfo.pVertexInputState = &m_vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &m_inputAssembly;

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &m_viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &m_scissor;

        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pMultisampleState = &m_multisampling;
        pipelineInfo.pDepthStencilState = &m_depthStencil;
        pipelineInfo.pColorBlendState = &m_colorBlending;
        pipelineInfo.pRasterizationState = &m_rasterizer;

        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
        if (m_dynamicStateEnables.empty()) {
            pipelineInfo.pDynamicState = nullptr;
        } else {
            dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(m_dynamicStateEnables.size());
            dynamicStateCreateInfo.pDynamicStates = m_dynamicStateEnables.data();
            pipelineInfo.pDynamicState = &dynamicStateCreateInfo;
        }

        pipelineInfo.layout = pipelineLayout;

        pipelineInfo.renderPass = VK_NULL_HANDLE; // Use dynamic rendering instead of render passes
        pipelineInfo.subpass = 0;
        pipelineInfo.pNext = &m_renderingInfo;

        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        return std::make_unique<Pipeline>(
            context,
            *m_pEffect,
            pipelineLayout,
            pipelineInfo);
    }

    Pipeline::Pipeline(
        Context& context,
        const Effect& effect,
        VkPipelineLayout pipelineLayout,
        VkGraphicsPipelineCreateInfo pipelineInfo)
        : m_context(context)
        , m_effect(effect)
        , m_pipelineLayout(pipelineLayout)
    {
        VkResult result =
            vkCreateGraphicsPipelines(
                context.getLogicalDevice(),
                VK_NULL_HANDLE, // PipelineCache TODO support this.
                1,
                &pipelineInfo,
                context.getAllocationCallbacks(),
                &m_graphicsPipeline);

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        m_effect.setPipeline(*this);
    }

    void Pipeline::destroy()
    {
        VkDevice device = m_context.getLogicalDevice();
        VkAllocationCallbacks* pAllocationCallbacks = m_context.getAllocationCallbacks();

        if (m_graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, m_graphicsPipeline, pAllocationCallbacks);
            m_graphicsPipeline = VK_NULL_HANDLE;
        }

        if (m_pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, m_pipelineLayout, pAllocationCallbacks);
            m_pipelineLayout = VK_NULL_HANDLE;
        }
    }
}

