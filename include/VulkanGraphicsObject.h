#pragma once

#include "VulkanGraphicsPipeline.h"
#include "VulkanGraphicsSceneNode.h"

#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Drawable;
    struct DrawContext;
    class Renderer;

    using Drawables = std::vector<Drawable*>;
    // Represents an Object in the scene, has a world transformation, can be composed of
    // multiple Drawables each with their own transformation relative to this.
    class Object : public SceneNode
    {
    public:
        Object() = default;
        ~Object() = default;

        void addDrawable(Drawable& pDrawable);

        Drawables& getDrawables() { return m_drawables; }
        const Drawables& getDrawables() const { return m_drawables; }

        void draw(Renderer& renderer, DrawContext& drawContext);

    private:
        Drawables m_drawables;
        bool m_buildPipelines = true;
    };
}

