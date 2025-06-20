#include "VulkanGraphicsSceneNode.h"

#include "VulkanGraphicsRenderer.h"
#include "VulkanGraphicsRenderPass.h"

namespace vgfx
{
    void LightNode::draw(Renderer::DrawContext& drawContext)
    {
        drawContext.pushLight(m_position, m_color, m_radius);

        GroupNode::draw(drawContext);

        drawContext.popLight()
    }
}