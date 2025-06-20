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
        if (m_buildPipelines) {
            buildPipelines(drawContext);
            m_buildPipelines = false;
        }
        for (auto& pDrawable : m_drawables) {
            pDrawable->draw(drawContext);
        }
    }
    void Object::buildPipelines(Renderer::DrawContext& drawContext)
    {
        const auto& camera = drawContext.sceneState.views.back();
        vgfx::PipelineBuilder builder(camera.viewport, drawContext.depthBufferEnabled);

        //std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        for (auto& pDrawable : m_drawables) {

            Pipeline::InputAssemblyConfig inputConfig(
                pDrawable->getVertexBuffer().getConfig().primitiveTopology,
                pDrawable->getIndexBuffer().getHasPrimitiveRestartValues());

            // TODO PipelineLibrary
            std::unique_ptr<Pipeline> spPipeline =
                builder.configureDrawableInput(pDrawable->getMaterial(), pDrawable->getVertexBuffer().getConfig(), inputConfig)
                    .configureRasterizer(camera.rasterizerConfig)
                    //.configureDynamicStates(dynamicStates)
                    .createPipeline(drawContext.context);

            m_pipelines.emplace_back(std::move(spPipeline));
        }
    }
}
