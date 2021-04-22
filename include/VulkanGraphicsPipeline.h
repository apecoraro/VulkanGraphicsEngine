#ifndef VGFX_PIPELINE_H
#define VGFX_PIPELINE_H

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsMaterials.h"
#include "VulkanGraphicsRenderTarget.h"
#include "VulkanGraphicsSwapChain.h"
#include "VulkanGraphicsVertexBuffer.h"

#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Pipeline
    {
    public:
        Pipeline(
            Context& context,
            const Material& material,
            VkPipelineLayout pipelineLayout,
            VkGraphicsPipelineCreateInfo pipelineInfo);

        ~Pipeline()
        {
            destroy();
        }

        VkPipeline getHandle() const { return m_graphicsPipeline; }
        VkPipelineLayout getLayout() const { return m_pipelineLayout; }
    private:
        void destroy();

        Context& m_context;
        const Material& m_material;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    };

    class PipelineBuilder
    {
    public:
        PipelineBuilder(const SwapChain& swapChain, const RenderTarget& renderTarget);

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

        PipelineBuilder& configureDrawableInput(
            const Material& material,
            const VertexBuffer::Config& vertexBufferConfig,
            const InputAssemblyConfig& inputAssemblyConfig);
 
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
                rasterizerInfo.frontFace =cullModeFrontFace;

                rasterizerInfo.depthBiasEnable = VK_FALSE;
                rasterizerInfo.depthBiasConstantFactor = 0.0f; // Optional
                rasterizerInfo.depthBiasClamp = 0.0f; // Optional
                rasterizerInfo.depthBiasSlopeFactor = 0.0f; // Optional
            }

            VkPipelineRasterizationStateCreateInfo rasterizerInfo = {};
        };

        PipelineBuilder& configureRasterizer(const RasterizerConfig& config);

        std::unique_ptr<Pipeline> createPipeline(Context& context);
    private:
        const Material* m_pMaterial = nullptr;
        VkPipelineShaderStageCreateInfo m_vertShaderStageInfo = {};

        VkVertexInputBindingDescription m_vertexBindingDescription;
        std::vector<VkVertexInputAttributeDescription> m_vertexAttributeDescriptions;
        VkPipelineVertexInputStateCreateInfo m_vertexInputInfo = {};

        VkPipelineInputAssemblyStateCreateInfo m_inputAssembly = {};

        VkPipelineShaderStageCreateInfo m_fragShaderStageInfo = {};
        std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;

        VkPipelineDepthStencilStateCreateInfo m_depthStencil = {};
        VkViewport m_viewport = {};
        VkRect2D m_scissor = {};

        VkPipelineRasterizationStateCreateInfo m_rasterizer = {};
        VkPipelineMultisampleStateCreateInfo m_multisampling = {};
        VkPipelineColorBlendAttachmentState m_colorBlendAttachment = {};
        VkPipelineColorBlendStateCreateInfo m_colorBlending = {};

        const RenderTarget& m_renderTarget;
    }; 
}

#endif
