#include "VulkanGraphicsRenderPass.h"

#include "VulkanGraphicsDepthStencilBuffer.h"
#include "VulkanGraphicsSwapChain.h"

#include <cassert>
#include <stdexcept>

namespace vgfx
{
    static bool HasStencilBits(VkFormat format)
    {
        return format == VK_FORMAT_D16_UNORM_S8_UINT
            || format == VK_FORMAT_D24_UNORM_S8_UINT
            || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
    }

    static bool LayoutUsesStencilBits(VkImageLayout layout)
    {
        return layout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL
            || layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL
            || layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            || layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }

    static void ValidateAttachmentIndices(
        const std::set<size_t>& attachIndices,
        const std::map<size_t, VkImageLayout>& inputs)
    {
        // Make sure indices are mutually exclusive.
        for (const auto& itr : inputs) {
            assert(attachIndices.find(itr.first) == attachIndices.end());
        }
    }

    void RenderPass::Config::addInternalPass(
        const std::set<size_t>& outputAttachments,
        const std::map<size_t, VkImageLayout>& inputs,
        const std::optional<const std::set<size_t>>& preservedAttachments)
    {
        ValidateAttachmentIndices(outputAttachments, inputs);
        if (preservedAttachments.has_value()) {
            ValidateAttachmentIndices(preservedAttachments.value(), inputs);
            // Validate attachment indices of outputs with preserved.
            for (const auto& itr : outputAttachments) {
                assert(preservedAttachments.value().find(itr) == preservedAttachments.value().end());
            }
        }

        RenderPass::SubpassDescription newSubpass;
        for (size_t i = 0; i < this->attachments.size(); ++i) { 
            if (outputAttachments.find(i) != outputAttachments.end()) {
                VkAttachmentReference outputRef = {};
                outputRef.attachment = static_cast<uint32_t>(i);
                outputRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                newSubpass.outputRefs.push_back(outputRef);
            } else if (preservedAttachments.has_value()
                    && preservedAttachments.value().find(i) != preservedAttachments.value().end()) {
                newSubpass.preserveRefs.push_back(static_cast<uint32_t>(i));
            }
        }

        for (const auto& input : inputs) {
            VkAttachmentReference inputRef = {};
            inputRef.attachment = static_cast<uint32_t>(input.first);
            inputRef.layout = input.second;
            newSubpass.inputRefs.push_back(inputRef);
        }

        this->subpasses.push_back(newSubpass);

        VkSubpassDependency internalSubpassDep = {};
        // This dependency transitions the input attachment from color attachment to shader read
        internalSubpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        internalSubpassDep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        internalSubpassDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        internalSubpassDep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        internalSubpassDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        auto insertItr = this->subpassDeps.end();
        --insertItr; // Insert it right before the last dependency
        this->subpassDeps.insert(insertItr, internalSubpassDep);
    }

    RenderPass::Config& RenderPassBuilder::addPass(
        const RenderTarget::Config& renderTargetCfg,
        const std::optional<const std::map<size_t, VkImageLayout>>& inputs,
        const std::optional<const std::set<size_t>>& outputAttachments)
    {
        assert(!renderTargetCfg.attachmentChain.empty());
        assert(!inputs.has_value() || !m_renderPassConfigs.empty());
        if (outputAttachments.has_value() && inputs.has_value() && inputs.value().size() > 0u) {
            ValidateAttachmentIndices(outputAttachments.value(), inputs.value());
        }
        // Build the VkAttachmentDescriptions (requires samples, load & store op, init & final layout)
        // Build VkAttachmentReferences (requires iteration over swap chain image indices (add support) and depth)
        RenderPass::Config curPass;
        curPass.subpasses.push_back(RenderPass::SubpassDescription());
        curPass.subpasses.back().bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        for (size_t i = 0; i < renderTargetCfg.attachmentChain.front().targetImageViews.size(); ++i) {

            // If output attachmentChain were not provided, then assume all attachmentChain are for output.
            if (!outputAttachments.has_value()
                    || outputAttachments.value().find(i) != outputAttachments.value().end()) {
                VkAttachmentReference outputRef = {};
                outputRef.attachment = static_cast<uint32_t>(i);
                outputRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                curPass.subpasses.back().outputRefs.push_back(outputRef);
            }

            const auto& pImageView = renderTargetCfg.attachmentChain.front().targetImageViews[i];

            curPass.attachments.push_back(VkAttachmentDescription{});
            curPass.attachments.back().samples = pImageView->getImage().getSampleCount();
            curPass.attachments.back().format = pImageView->getFormat();

            curPass.attachments.back().initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            curPass.attachments.back().finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            curPass.attachments.back().loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            curPass.attachments.back().storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            curPass.attachments.back().stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            curPass.attachments.back().stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }

        if (!renderTargetCfg.attachmentChain[0].targetImageViews.empty()) {
            curPass.subpassDeps.resize(2u, VkSubpassDependency{});
            // Dependency at the start of the renderpass does the transition from final to initial layout.
            auto& firstDep = curPass.subpassDeps.front();
            firstDep.srcSubpass = VK_SUBPASS_EXTERNAL;
            firstDep.dstSubpass = 0u;
            firstDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            firstDep.srcAccessMask = 0u;
            firstDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            firstDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            firstDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            // Dependency at the end the renderpass does the transition from the initial to the final layout.
            auto& secondDep = curPass.subpassDeps.back();
            secondDep.dstSubpass = VK_SUBPASS_EXTERNAL;
            secondDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            secondDep.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            secondDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            secondDep.dstAccessMask = 0u;
            secondDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        }

        if (renderTargetCfg.attachmentChain.front().pDepthStencilView != nullptr) {
            curPass.subpasses.back().depthAttachmentRef.attachment =
                static_cast<uint32_t>(renderTargetCfg.attachmentChain.front().targetImageViews.size());
            // TODO it is possible to use a depth only layout and renderTargetFormat with:
            // https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures.html
            curPass.subpasses.back().depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            const auto& pDepthStencilView = renderTargetCfg.attachmentChain.front().pDepthStencilView;
            curPass.attachments.push_back(VkAttachmentDescription{});
            curPass.attachments.back().samples = pDepthStencilView->getImage().getSampleCount();
            curPass.attachments.back().format = pDepthStencilView->getFormat();
            if (m_renderPassConfigs.empty()) {
                curPass.attachments.back().initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                curPass.attachments.back().finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                curPass.attachments.back().loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                curPass.attachments.back().storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                curPass.attachments.back().stencilLoadOp =
                    HasStencilBits(pDepthStencilView->getFormat())
                        ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                curPass.attachments.back().stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            }

            if (curPass.subpassDeps.empty()) {
                curPass.subpassDeps.push_back(VkSubpassDependency{});
            }
            auto& depthDep = curPass.subpassDeps.front();
            depthDep.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            depthDep.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            depthDep.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            depthDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        }

        // Update the previous pass' store op and final layout.
        if (!m_renderPassConfigs.empty()) {
            RenderPass::Config& prevPass = m_renderPassConfigs.back();
            for (const auto& input : inputs.value()) {
                assert(input.first < prevPass.attachments.size());
                auto& prevPassOutput = prevPass.attachments[input.first];
                // Make sure the previous pass output is stored and transferred to the expected layout.
                prevPassOutput.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                prevPassOutput.finalLayout = input.second;
                if (LayoutUsesStencilBits(input.second)) {
                    assert(HasStencilBits(prevPassOutput.format));
                    prevPassOutput.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
                    if (input.second == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR) {
                        // Change the depth store op back to dont care, since the desired layout only requires
                        // the stencil bits to be read-able.
                        prevPassOutput.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                    }
                }
            }
            // Change the previous pass' first and last subpass dependency.
            auto& prevPassFirstSubpassDep = prevPass.subpassDeps.back();
            prevPassFirstSubpassDep.srcAccessMask |= VK_ACCESS_MEMORY_READ_BIT;
            prevPassFirstSubpassDep.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            auto& prevPassLastSubpassDep = prevPass.subpassDeps.back();
            prevPassLastSubpassDep.srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
            prevPassFirstSubpassDep.dstAccessMask |= VK_ACCESS_MEMORY_READ_BIT;
        }

        m_renderPassConfigs.push_back(curPass);
        return m_renderPassConfigs.back();
    }

    std::unique_ptr<RenderPass> RenderPassBuilder::createPass(const RenderPass::Config& config)
    {
        return std::make_unique<RenderPass>(m_context, config);
    }

    std::vector<std::unique_ptr<RenderPass>>&& RenderPassBuilder::createPasses()
    {
        std::vector<std::unique_ptr<RenderPass>> renderPasses;
        for (const auto& config : m_renderPassConfigs) {
            renderPasses.push_back(std::make_unique<RenderPass>(m_context, config));
        }

        return std::move(renderPasses);
    }

    void RenderPassBuilder::accept(RenderPassNode& node)
    {
        const auto& renderPassConfig = addPass(node.getRenderTargetConfig(), node.getInputs());

        node.visit(*this);

        node.setRenderPass(std::move(createPass(renderPassConfig)));
    }

    RenderPass::RenderPass(
        Context& context,
        const Config& config)
        : m_context(context)
    {
        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

        renderPassInfo.attachmentCount = static_cast<uint32_t>(config.attachments.size());
        renderPassInfo.pAttachments = config.attachments.data();

        renderPassInfo.subpassCount = static_cast<uint32_t>(config.subpasses.size());
        std::vector<VkSubpassDescription> subpasses;
        subpasses.reserve(config.subpasses.size());
        for (size_t i = 0u; i < config.subpasses.size(); ++i) {
            VkSubpassDescription subpass = {};
            const auto& subpassDesc = config.subpasses[i];
            subpass.pipelineBindPoint = subpassDesc.bindPoint;
            subpass.colorAttachmentCount = static_cast<uint32_t>(subpassDesc.outputRefs.size());
            subpass.pColorAttachments = subpassDesc.outputRefs.data();
            if (!subpassDesc.inputRefs.empty()) {
                subpass.inputAttachmentCount = static_cast<uint32_t>(subpassDesc.inputRefs.size());
                subpass.pInputAttachments = subpassDesc.inputRefs.data();
            }
            if (!subpassDesc.preserveRefs.empty()) {
                subpass.preserveAttachmentCount = static_cast<uint32_t>(subpassDesc.preserveRefs.size());
                subpass.pPreserveAttachments = subpassDesc.preserveRefs.data();
            }
            if (subpassDesc.depthAttachmentRef.layout != VK_IMAGE_LAYOUT_UNDEFINED) {
                subpass.pDepthStencilAttachment = &subpassDesc.depthAttachmentRef;
            }
            subpasses.push_back(subpass);
        }
        renderPassInfo.pSubpasses = subpasses.data();

        renderPassInfo.dependencyCount = static_cast<uint32_t>(config.subpassDeps.size());
        std::vector<VkSubpassDependency> subpassDeps;
        subpassDeps.reserve(config.subpassDeps.size());
        uint32_t passIndex = 0u;
        for (const auto& subpassDepTemp : config.subpassDeps) {
            VkSubpassDependency subpassDep = subpassDepTemp;
            if (subpassDep.srcSubpass != VK_SUBPASS_EXTERNAL) {
                subpassDep.srcSubpass = passIndex - 1u;
            }
            if (subpassDep.dstSubpass != VK_SUBPASS_EXTERNAL) {
                subpassDep.dstSubpass = passIndex;
            }
            ++passIndex;
            subpassDeps.push_back(subpassDep);
        }
        renderPassInfo.pDependencies = subpassDeps.data();

        VkDevice device = context.getLogicalDevice();
        VkAllocationCallbacks* pAllocationCallbacks = context.getAllocationCallbacks();

        if (vkCreateRenderPass(device, &renderPassInfo, pAllocationCallbacks, &m_renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    RenderPass::~RenderPass()
    {
        vkDestroyRenderPass(m_context.getLogicalDevice(), m_renderPass, m_context.getAllocationCallbacks());
    }

    void RenderPass::begin(
        VkCommandBuffer commandBuffer,
        size_t swapChainImageIndex,
        RenderTarget& renderTarget,
        const std::optional<const VkRect2D> renderArea,
        const std::optional<std::vector<VkClearValue>> clearValues)
    {
        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = m_renderPass;
        renderPassBeginInfo.framebuffer = renderTarget.getFramebuffer(swapChainImageIndex);
        if (renderArea.has_value()) {
            renderPassBeginInfo.renderArea = renderArea.value();
        } else {
            renderPassBeginInfo.renderArea.offset = { 0, 0 };
            renderPassBeginInfo.renderArea.extent = renderTarget.getExtent();
        }

        VkClearValue defaultClearValues[2] = {
          { 0.0f, 0.0f, 0.0f, 0.0f }, // Color
          { 1.0f, 0 }, // Depth/Stencil
        };

        if (clearValues.has_value()) {
            renderPassBeginInfo.pClearValues = clearValues.value().data();
            renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.value().size());
        } else {
            renderPassBeginInfo.pClearValues = defaultClearValues;
            renderPassBeginInfo.clearValueCount = (renderTarget.hasDepthStencilBuffer() ? 1 : 2);
        }

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void RenderPass::end(VkCommandBuffer commandBuffer)
    {
        vkCmdEndRenderPass(commandBuffer);
    }
}

