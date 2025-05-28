#include "VulkanGraphicsObject.h"

#include "VulkanGraphicsDrawable.h"

namespace vgfx
{
    void Object::addDrawable(Drawable& drawable)
    {
        m_drawables.push_back(&drawable);
    }

    void Object::draw(Renderer::DrawContext& drawContext)
    {
        for (auto& pDrawable : m_drawables) {
            pDrawable->draw(drawContext);
        }
    }
}
