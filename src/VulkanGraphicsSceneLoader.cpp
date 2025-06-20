#include "VulkanGraphicsSceneLoader.h"

#include <glm/glm.hpp>

using namespace vgfx;

SceneLoader::SceneLoader(Context& graphicsContext)
    : m_spModelLibrary(std::make_unique<ModelLibrary>())
    , m_spCommandBufferFactory(
        std::make_unique<CommandBufferFactory>(
            graphicsContext,
            graphicsContext.getGraphicsQueueFamilyIndex().value())
{
}

std::unique_ptr<SceneNode> SceneLoader::loadScene(const std::string&)
{
    std::unique_ptr<GroupNode> spScene = std::make_unique<GroupNode>();

    std::unique_ptr<LightNode> spLightNode =
        std::make_unique<LightNode>(
            glm::vec4(1.0f, 100.0f, 500.0f, 1.0f), //position
            glm::vec3(1.0f, 1.0f, 1.0f), // color
            150000.0f); // radius

    spScene->addNode(std::move(spLightNode));

    // TODO multiple lights
    /*lightingUniforms.lights[1] = {
        glm::vec4(100.0f, 0.0f, 3.0f, 1.0f), //position
        glm::vec3(1.0f, 1.0f, 1.0f), // color
        2000.0f, // radius
    };*/

    std::unique_ptr<Object> spGraphicsObject = std::make_unique<Object>();
    spGraphicsObject->addDrawable(drawable);

    spLightNode->addNode(std::move(spGraphicsObject));

    // TODO compile code
    return std::move(spScene);
}
