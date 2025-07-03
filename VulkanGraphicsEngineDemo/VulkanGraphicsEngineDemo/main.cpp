// VulkanGraphicsEngineDemo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "VulkanGraphicsGLFWApplication.h"
#include "VulkanGraphicsSceneLoader.h"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

void LoadScene()
{
    m_spCommandBufferFactory =
        std::make_unique<vgfx::CommandBufferFactory>(
            m_spApplication->getContext(),
            m_spApplication->getContext().getGraphicsQueueFamilyIndex().value());

    std::string vertexShaderEntryPointFunc = "main";
    std::string fragmentShaderEntryPointFunc = "main";

    // Create MaterialInfo which is used to create an instance of the material for a particular
    // model by the ModelLibrary.
    vgfx::MaterialsLibrary::MaterialInfo materialInfo(
        vertexShader,
        vertexShaderEntryPointFunc,
        vgfx::VertexXyzRgbUvN::GetConfig().vertexAttrDescriptions, // TODO should use reflection for this.
        fragmentShader,
        fragmentShaderEntryPointFunc);

    vgfx::Material& material = vgfx::MaterialsLibrary::GetOrLoadMaterial(m_spApplication->getContext(), materialInfo);

    // Allocate buffer for camera parameters
    // Allocate descriptor set for camera parameters
    // Update descriptor set to point at the buffer
    // Each frame map the buffer and write the new camera data

    m_spModelLibrary = std::make_unique<vgfx::ModelLibrary>();

    vgfx::ModelLibrary::Model model;
    model.modelPathOrShapeName = modelPath;
    if (!modelDiffuseTexName.empty()) {
        model.imageOverrides[vgfx::Material::ImageType::Diffuse] = modelDiffuseTexName;
    }

    glm::mat4 modelWorldTransform = glm::identity<glm::mat4>();
    // Rendering to offscreen image at half the resolution requires dynamic viewport and scissor in the
    // graphics pipeline.
    auto& drawable =
        m_spModelLibrary->getOrCreateDrawable(
            m_spApplication->getContext(),
            model,
            material,
            *m_spCommandBufferFactory,
            m_spApplication->getContext().getGraphicsQueue(0));

    drawable.setWorldTransform(modelWorldTransform);

    createScene(m_spApplication->getContext(), drawable);
}

static void ShowHelpAndExit(const char* pBadOption = nullptr)
{
    std::ostringstream oss;
    bool throwError = false;
    if (pBadOption) {
        throwError = true;
        oss << "Error parsing \"" << pBadOption << "\"" << std::endl;
    }
    oss << "Options:" << std::endl
        << "-p           Data directory path." << std::endl
        << "-s           Input scene filename (relative to data directory path)." << std::endl
        << "-v           Enable validation layers." << std::endl;

    if (throwError) {
        throw std::invalid_argument(oss.str());
    } else {
        std::cerr << oss.str();
        exit(0);
    }
}

static void ParseCommandLine(
    int argc, char* argv[],
    std::string* pDataDirPath,
    std::string* pSceneFilename,
    bool* pEnableValidationLayers)
{
    std::ostringstream oss;
    for (int i = 1; i < argc; ++i) {
        if (_stricmp(argv[i], "-h") == 0) {
            ShowHelpAndExit();
        } else if (_stricmp(argv[i], "-s") == 0) {
            if (++i == argc) {
                ShowHelpAndExit("-m");
            }
            *pSceneFilename = argv[i];
            continue;
        } else if (_stricmp(argv[i], "-p") == 0) {
            if (++i == argc) {
                ShowHelpAndExit("-p");
            }
            *pDataDirPath = argv[i];
            continue;
        } else if (_stricmp(argv[i], "-v") == 0) {
            *pEnableValidationLayers = true;
        }
        ShowHelpAndExit(argv[i]);
    }
}

int main(int argc, char** argv)
{
    std::string dataDirPath = ".";
    std::string sceneFilename = "default.vgfx";
    bool enableValidationLayers = false;

    ParseCommandLine(
        argc, argv,
        &dataDirPath,
        &sceneFilename,
        &enableValidationLayers);

    vgfx::Context::AppConfig appConfig("Demo");
    vgfx::Context::InstanceConfig instanceConfig = {
        .enableDebugLayers = enableValidationLayers,
    };
    vgfx::Context::DeviceConfig deviceConfig;
    vgfx::WindowRenderer::SwapChainConfig swapChainConfig;

    demo::GLFWApplication app(appConfig, instanceConfig, deviceConfig, swapChainConfig);

    vgfx::SceneLoader sceneLoader(app.getContext());

    std::unique_ptr<vgfx::SceneNode> spScene = sceneLoader.loadScene(sceneFilename);

    app.setScene(std::move(spScene));

    app.run();

    return EXIT_SUCCESS;
}
