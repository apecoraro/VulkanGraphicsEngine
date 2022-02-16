#include "VulkanGraphicsSceneNode.h"

#include "VulkanGraphicsRenderer.h"

namespace vgfx
{
    CameraNode::CameraNode(Context& context, size_t numBuffers)
    {
        vgfx::Buffer::Config cameraMatrixBufferCfg(sizeof(glm::mat4));
        for (uint32_t index = 0; index < numBuffers; ++index) {
            m_cameraMatrixBuffers.emplace_back(
                std::make_unique<vgfx::Buffer>(
                    context,
                    vgfx::Buffer::Type::UniformBuffer,
                    cameraMatrixBufferCfg));
        }
    }

    void CameraNode::recordCommands(Renderer& renderer)
    {
        size_t currentBufferIndex = renderer.getFrameIndex() % m_cameraMatrixBuffers.size();
        
        renderer.setViewTransform(m_view);
        if (m_projUpdated) {
            m_projUpdated = false;

            auto& currentBuffer = *m_cameraMatrixBuffers[currentBufferIndex];
            currentBuffer.update(&m_proj, sizeof(glm::mat4));

            renderer.setProjectionTransformBuffer(currentBuffer);
        }

        GroupNode::recordCommands(renderer);
    }

    void RenderPassNode::recordCommands(Renderer& renderer)
    {
        m_spRenderPass->begin(renderer, *m_spRenderTarget.get());

        GroupNode::recordCommands(renderer);

        m_spRenderPass->end(renderer);
    }

    void RenderPassNode::setRenderPass(std::unique_ptr<RenderPass> spRenderPass)
    {
        m_spRenderPass = std::move(spRenderPass);
        m_spRenderTarget =
            std::make_unique<vgfx::RenderTarget>(
                m_spRenderPass->getContext(),
                m_renderTargetConfig,
                *m_spRenderPass.get());
    }
}