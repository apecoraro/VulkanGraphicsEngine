#include "VulkanGraphicsSceneNode.h"

#include "VulkanGraphicsRenderer.h"

namespace vgfx
{
    void LightNode::draw(Renderer& renderer, DrawContext& drawContext)
    {
        drawContext.pushLight(m_position, m_color, m_radius);

        GroupNode::draw(renderer, drawContext);
    }
}