#include "VulkanGraphicsObject.h"

#include "VulkanGraphicsDrawable.h"

namespace vgfx
{
    void Object::addDrawable(Drawable& drawable)
    {
        m_drawables.push_back(&drawable);
    }

    void Object::draw(Renderer& renderer, DrawContext& drawContext)
    {
        if (m_buildPipelines) {
            renderer.buildPipelines(*this, drawContext);
            m_buildPipelines = false;
        }
        for (auto& pDrawable : m_drawables) {
            pDrawable->draw(drawContext);
        }
    }
}
