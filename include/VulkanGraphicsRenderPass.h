#ifndef VGFX_RENDER_PASS_H
#define VGFX_RENDER_PASS_H

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsRenderTarget.h"

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
            size_t swapChainImageIndex,
            RenderTarget& renderTarget,
            const std::optional<const VkRect2D> renderArea = std::nullopt);

        void end(VkCommandBuffer commandBuffer);

        VkRenderPass getHandle() const { return m_renderPass; }
    private:
        Context& m_context;

        VkRenderPass m_renderPass = VK_NULL_HANDLE;
    }; 

    class RenderPassBuilder
    {
    public:
        RenderPassBuilder() = default;

        RenderPass::Config& addPass(
            const RenderTarget::Config& renderTargetCfg,
            // If not output attachments are specified then all render target attachments are assumed.
            const std::optional<const std::set<size_t>>& outputAttachments = std::nullopt,
            const std::optional<const std::map<size_t, VkImageLayout>>& inputs = std::nullopt);

        std::unique_ptr<RenderPass> createPass(Context& context, size_t passIndex=0u);
        std::vector<std::unique_ptr<RenderPass>>&& createPasses(Context& context);

    private:
        std::vector<RenderPass::Config> m_renderPassConfigs;
    };
}

#endif
