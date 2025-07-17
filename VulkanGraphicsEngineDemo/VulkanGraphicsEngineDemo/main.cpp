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
                ShowHelpAndExit("-s");
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
            continue;
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
    appConfig.enableValidationLayers = enableValidationLayers;
    appConfig.dataDirectoryPath = dataDirPath;

    vgfx::Context::InstanceConfig instanceConfig;
    vgfx::Context::DeviceConfig deviceConfig;
    vgfx::SwapChain::Config swapChainConfig;

    demo::GLFWApplication app(appConfig, instanceConfig, deviceConfig, swapChainConfig);

    vgfx::SceneLoader sceneLoader(app.getContext());

    std::unique_ptr<vgfx::SceneNode> spScene = sceneLoader.loadScene(sceneFilename);

    app.setScene(std::move(spScene));

    app.run();

    return EXIT_SUCCESS;
}
