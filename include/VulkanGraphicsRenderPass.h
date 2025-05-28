#pragma once

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsRenderTarget.h"
#include "VulkanGraphicsSceneNode.h"

#include <map>
#include <optional>
#include <set>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    // Use RenderPassBuilder to construct a RenderPass.
    class RenderPass
    {
    public:
        struct SubpassDescription
        {
            VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            std::vector<VkAttachmentReference> inputRefs;
            std::vector<VkAttachmentReference> outputRefs;
            std::vector<uint32_t> preserveRefs;
            VkAttachmentReference depthAttachmentRef = {};
        };

        struct Config
        {
            std::vector<VkAttachmentDescription> attachments;
            std::vector<SubpassDescription> subpasses;
            std::list<VkSubpassDependency> subpassDeps;

            void addInternalPass(
                const std::set<size_t>& outputAttachments,
                const std::map<size_t, VkImageLayout>& inputs,
                const std::optional<const std::set<size_t>>& preservedAttachments = std::nullopt);
        };
        RenderPass(
            Context& context,
            const Config& config);
        ~RenderPass();


        void begin(
            VkCommandBuffer commandBuffer,
            RenderTarget& renderTarget,
            const std::optional<const VkRect2D> renderArea = std::nullopt,
            const std::optional<std::vector<VkClearValue>> clearValues = std::nullopt);

        void end(VkCommandBuffer commandBuffer);

        VkRenderPass getHandle() const { return m_renderPass; }

        Context& getContext() { return m_context; }

    private:
        Context& m_context;

        VkRenderPass m_renderPass = VK_NULL_HANDLE;
    }; 

    class RenderPassBuilder : public Visitor
    {
    public:
        RenderPassBuilder(Context& context) : m_context(context) {}

        RenderPass::Config& addPass(
            const RenderTarget::Config& renderTargetCfg,
            const std::optional<const std::map<size_t, VkImageLayout>>& inputs = std::nullopt,
            // If no output attachmentChain are specified then all render target attachmentChain are assumed.
            const std::optional<const std::set<size_t>>& outputAttachments = std::nullopt);

        std::unique_ptr<RenderPass> createPass(const RenderPass::Config& config);
        std::vector<std::unique_ptr<RenderPass>>&& createPasses();

        void accept(RenderPassNode& node) override;

    private:
        Context& m_context;
        std::vector<RenderPass::Config> m_renderPassConfigs;
    };
}
