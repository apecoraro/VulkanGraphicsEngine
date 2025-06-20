// VulkanGraphicsEngineDemo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "VulkanGraphicsGLFWApplication.h"

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

static GLFWwindow* AppInit(
    const std::string& dataDirPath,
    const std::string& modelFilename,
    const std::string& modelDiffuseTextureName,
    const std::string& vertShaderFilename,
    const std::string& fragShaderFilename)
{
    GLFWwindow* window = InitWindow();
    if (window != nullptr)
    {
        uint32_t glfwExtensionCount;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        bool enableValidationLayers = true;
        if (!DemoInit(window,
            CreateWindowSurfaceCallback,
            GetFrameBufferSizeCallback,
            glfwExtensionCount,
            glfwExtensions,
            enableValidationLayers,
            dataDirPath.c_str(),
            modelFilename.c_str(),
            modelDiffuseTextureName.c_str(),
            vertShaderFilename.c_str(),
            fragShaderFilename.c_str()))
        {
            glfwDestroyWindow(window);
            glfwTerminate();
            window = nullptr;
        }
    }

    return window;
}

static void AppRun(GLFWwindow* window)
{
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (!DemoDraw())
            break;
    }
}

static void AppCleanup(GLFWwindow* window)
{
    // TODO the main function should handle loading the scene and passing to this Application.
    DemoCleanUp();

    glfwDestroyWindow(window);

    glfwTerminate();
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
        << "-d           Data directory path." << std::endl
        << "-m           Input model filename (relative to data directory path)." << std::endl
        << "-v           Input vertex shader filename (relative to data directory path)." << std::endl
        << "-f           Input fragment shader (relative to data directory path)." << std::endl;

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
    std::string* pModelFilename,
    std::string* pModelDiffuseTextureName,
    std::string* pVertShaderFilename,
    std::string* pFragShaderFilename)
{
    std::ostringstream oss;
    for (int i = 1; i < argc; ++i) {
        if (_stricmp(argv[i], "-h") == 0) {
            ShowHelpAndExit();
        } else if (_stricmp(argv[i], "-m") == 0) {
            if (++i == argc) {
                ShowHelpAndExit("-m");
            }
            *pModelFilename = argv[i];
            continue;
        } else if (_stricmp(argv[i], "-i") == 0) {
            if (++i == argc) {
                ShowHelpAndExit("-m");
            }
            *pModelDiffuseTextureName = argv[i];
            continue;
        } else if (_stricmp(argv[i], "-v") == 0) {
            if (++i == argc) {
                ShowHelpAndExit("-v");
            }
            *pVertShaderFilename = argv[i];
            continue;
        } else if (_stricmp(argv[i], "-f") == 0) {
            if (++i == argc) {
                ShowHelpAndExit("-f");
            }
            *pFragShaderFilename = argv[i];
            continue;
        } else if (_stricmp(argv[i], "-d") == 0) {
            if (++i == argc) {
                ShowHelpAndExit("-d");
            }
            *pDataDirPath = argv[i];
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }
}

int main(int argc, char** argv)
{
    std::string dataDirPath = ".";
    std::string modelFilename;
    std::string modelDiffuseTextureName;
    std::string vertexShaderFilename;
    std::string fragShaderFilename;

    ParseCommandLine(
        argc, argv,
        &dataDirPath,
        &modelFilename,
        &modelDiffuseTextureName,
        &vertexShaderFilename,
        &fragShaderFilename);

    GLFWwindow* window =
        AppInit(
            dataDirPath,
            modelFilename,
            modelDiffuseTextureName,
            vertexShaderFilename,
            fragShaderFilename);
    if (window != nullptr)
    {
        AppRun(window);
        AppCleanup(window);
    }
    else
    {
        std::cerr << "AppInit returned nullptr to window!" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
