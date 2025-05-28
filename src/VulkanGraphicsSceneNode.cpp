#include "VulkanGraphicsSceneNode.h"

#include "VulkanGraphicsRenderer.h"
#include "VulkanGraphicsRenderPass.h"

namespace vgfx
{
    CameraNode::CameraNode(Context& context, size_t maxFramesInFlight)
    {
        vgfx::Buffer::Config cameraMatrixBufferCfg(sizeof(glm::mat4));
        for (uint32_t index = 0; index < maxFramesInFlight; ++index) {
            m_cameraMatrixBuffers.emplace_back(
                std::make_unique<vgfx::Buffer>(
                    context,
                    vgfx::Buffer::Type::UniformBuffer,
                    cameraMatrixBufferCfg));
        }
    }

    void CameraNode::draw(Renderer::DrawContext& drawContext)
    {
        CameraNode* pParentCamera = drawContext.sceneState.pCurrentCameraNode;

        drawContext.sceneState.pCurrentCameraNode = this;

        auto& currentBuffer = *m_cameraMatrixBuffers[m_currentBufferIndex];
        if (m_projUpdated) {
            currentBuffer.update(&m_proj, sizeof(glm::mat4));
        }

        GroupNode::draw(drawContext);

        drawContext.sceneState.pCurrentCameraNode = pParentCamera;

        if (m_projUpdated) {
            m_projUpdated = false;
            ++m_currentBufferIndex;
            if (m_currentBufferIndex == m_cameraMatrixBuffers.size()) {
                m_currentBufferIndex = 0u;
            }
        }
    }

    void RenderPassNode::draw(Renderer::DrawContext& drawContext)
    {
        m_spRenderPass->begin(drawContext.commandBuffer, *m_spRenderTarget.get());

        GroupNode::draw(drawContext);

        m_spRenderPass->end(drawContext.commandBuffer);
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

    void GraphicsNode::draw(Renderer::DrawContext& drawContext)
    {
        for (auto& objects : m_graphicsObjects) {
            objects->draw(drawContext);
        }
    }
}