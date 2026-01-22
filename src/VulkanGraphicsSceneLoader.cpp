#include "VulkanGraphicsSceneLoader.h"

#include <glm/glm.hpp>

using namespace vgfx;

SceneLoader::SceneLoader(Context& graphicsContext)
    : m_graphicsContext(graphicsContext)
    , m_spModelLibrary(std::make_unique<ModelLibrary>())
    , m_spCommandBufferFactory(
        std::make_unique<CommandBufferFactory>(
            graphicsContext,
            graphicsContext.getGraphicsQueue(0u)))
{
}

vgfx::SceneLoader::~SceneLoader()
{
    EffectsLibrary::UnloadAll();
}

// TODO make some sort of scene file
std::unique_ptr<SceneNode> SceneLoader::loadScene(const std::string&)
{
    std::unique_ptr<GroupNode> spScene = std::make_unique<GroupNode>();

    glm::vec3 position(1.0f, 10.0f, 10.0f);
    glm::vec3 color(1.0f, 1.0f, 1.0f);
    float radius = 150.0f;
    std::unique_ptr<LightNode> spLightNode =
        std::make_unique<LightNode>(position, color, radius);

    // TODO multiple lights
    /*lightingUniforms.lights[1] = {
        glm::vec4(100.0f, 0.0f, 3.0f, 1.0f), //position
        glm::vec3(1.0f, 1.0f, 1.0f), // color
        2000.0f, // radius
    };*/

    std::unique_ptr<Object> spGraphicsObject = std::make_unique<Object>();

    const std::string& dataPath = m_graphicsContext.getAppConfig().dataDirectoryPath;
    std::string modelPath = "SHAPE_SPHERE";
    std::string modelDiffuseTexName = "wood.png";

    ModelLibrary::ModelDesc modelDesc;
    modelDesc.modelPathOrShapeName = modelPath;
    if (!modelDiffuseTexName.empty()) {
        modelDesc.imagesOverrides[MeshEffect::ImageType::Diffuse] = modelDiffuseTexName;
    }

    glm::mat4 modelWorldTransform = glm::identity<glm::mat4>();
    // Rendering to offscreen image at half the resolution requires dynamic viewport and scissor in the
    // graphics pipeline.
    auto& drawable =
        m_spModelLibrary->getOrCreateDrawable(
            m_graphicsContext,
            modelDesc,
            *m_spCommandBufferFactory);

    drawable.setWorldTransform(modelWorldTransform);
    spGraphicsObject->addDrawable(drawable);

    spLightNode->addNode(std::move(spGraphicsObject));
    spScene->addNode(std::move(spLightNode));

    return std::move(spScene);
}
