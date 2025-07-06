#include "VulkanGraphicsSceneLoader.h"

#include <glm/glm.hpp>

using namespace vgfx;

SceneLoader::SceneLoader(Context& graphicsContext)
    : m_graphicsContext(graphicsContext)
    , m_spModelLibrary(std::make_unique<ModelLibrary>())
    , m_spCommandBufferFactory(
        std::make_unique<CommandBufferFactory>(
            graphicsContext,
            graphicsContext.getGraphicsQueueFamilyIndex()))
{
}

// TODO make some sort of scene file
std::unique_ptr<SceneNode> SceneLoader::loadScene(const std::string&)
{
    std::unique_ptr<GroupNode> spScene = std::make_unique<GroupNode>();

    std::unique_ptr<LightNode> spLightNode =
        std::make_unique<LightNode>(
            glm::vec4(1.0f, 100.0f, 500.0f, 1.0f), //position
            glm::vec3(1.0f, 1.0f, 1.0f), // color
            150000.0f); // radius

    // TODO multiple lights
    /*lightingUniforms.lights[1] = {
        glm::vec4(100.0f, 0.0f, 3.0f, 1.0f), //position
        glm::vec3(1.0f, 1.0f, 1.0f), // color
        2000.0f, // radius
    };*/

    std::unique_ptr<Object> spGraphicsObject = std::make_unique<Object>();

    std::string dataPath = "../../data";
    std::string modelPath = "SHAPE_SPHERE";
    std::string modelDiffuseTexName = "albedo.png";
    std::string vertexShader = "MvpTransform_XyzRgbUvNormal_Out.vert.spv";
    std::string fragmentShader = "TexturedBlinnPhong.frag.spv";

    std::string vertexShaderEntryPointFunc = "main";
    std::string fragmentShaderEntryPointFunc = "main";

    vgfx::EffectsLibrary::MeshEffectDesc meshEffectDesc(
        vertexShader,
        vertexShaderEntryPointFunc,
        vgfx::VertexXyzRgbUvN::GetConfig().vertexAttrDescriptions, // TODO should use reflection for this.
        fragmentShader,
        fragmentShaderEntryPointFunc);

    vgfx::MeshEffect& meshEffect =
        vgfx::EffectsLibrary::GetOrLoadEffect(m_graphicsContext, meshEffectDesc);

    vgfx::ModelLibrary::ModelDesc modelDesc;
    modelDesc.modelPathOrShapeName = modelPath;
    if (!modelDiffuseTexName.empty()) {
        modelDesc.imageOverrides[vgfx::MeshEffect::ImageType::Diffuse] = modelDiffuseTexName;
    }

    glm::mat4 modelWorldTransform = glm::identity<glm::mat4>();
    // Rendering to offscreen image at half the resolution requires dynamic viewport and scissor in the
    // graphics pipeline.
    auto& drawable =
        m_spModelLibrary->getOrCreateDrawable(
            m_graphicsContext,
            modelDesc,
            meshEffect,
            *m_spCommandBufferFactory,
            m_graphicsContext.getGraphicsQueue(0));

    drawable.setWorldTransform(modelWorldTransform);
    spGraphicsObject->addDrawable(drawable);

    spLightNode->addNode(std::move(spGraphicsObject));
    spScene->addNode(std::move(spLightNode));

    return std::move(spScene);
}
