#pragma once

#include "VulkanGraphicsPipeline.h"
#include "VulkanGraphicsRenderer.h"
#include "VulkanGraphicsSceneNode.h"

#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Drawable;
    // Represents an Object in the scene, has a world transformation, can be composed of
    // multiple Drawables each with their own transformation relative to this.
    class Object : public SceneNode
    {
    public:
        Object() = default;
        ~Object() = default;

        void addDrawable(Drawable& pDrawable);

        void draw(Renderer::DrawContext& drawContext);

    private:
        void buildPipelines(Renderer::DrawContext& drawContext);

        std::vector<Drawable*> m_drawables;
        bool m_buildPipelines = true;
        std::vector<std::unique_ptr<Pipeline>> m_pipelines;
    };
}

