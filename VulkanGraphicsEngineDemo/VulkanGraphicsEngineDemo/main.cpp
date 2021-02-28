// VulkanGraphicsEngineDemo.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "VulkanGraphicsEngineDemo.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>


#include <sstream>
#include <iostream>

static void FramebufferResizedCallback(
    GLFWwindow*, // window
    int, // width
    int) // height
{
    DemoSetFramebufferResized(true);
}

static GLFWwindow* InitWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(window, nullptr);
    glfwSetFramebufferSizeCallback(window, FramebufferResizedCallback);

    return window;
}

static VkResult CreateWindowSurfaceCallback(VkInstance vulkanInstance, void* window, const VkAllocationCallbacks* pAllocCallbacks, VkSurfaceKHR* pSurfaceOut)
{
    return glfwCreateWindowSurface(vulkanInstance, (GLFWwindow*)window, pAllocCallbacks, pSurfaceOut);
}

static void GetFrameBufferSizeCallback(void* window, int* pWidth, int* pHeight)
{
    int width = 0, height = 0;
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);
        glfwWaitEvents();
    }

    *pWidth = width;
    *pHeight = height;
}

static GLFWwindow* AppInit(
    const std::string& modelPath,
    const std::string& vertShaderPath,
    const std::string& fragShaderPath)
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
            modelPath.c_str(),
            vertShaderPath.c_str(),
            fragShaderPath.c_str()))
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
        << "-m           Input model file path" << std::endl;

    if (throwError) {
        throw std::invalid_argument(oss.str());
    } else {
        std::cerr << oss.str();
        exit(0);
    }
}

static void ParseCommandLine(
    int argc, char* argv[],
    std::string* pModelPath,
    std::string* pVertShaderPath,
    std::string* pFragShaderPath)
{
    std::ostringstream oss;
    for (int i = 1; i < argc; ++i) {
        if (_stricmp(argv[i], "-h") == 0) {
            ShowHelpAndExit();
        } else if (_stricmp(argv[i], "-m") == 0) {
            if (++i == argc) {
                ShowHelpAndExit("-m");
            }
            *pModelPath = argv[i];
            continue;
        } else if (_stricmp(argv[i], "-v") == 0) {
            if (++i == argc) {
                ShowHelpAndExit("-v");
            }
            *pVertShaderPath = argv[i];
            continue;
        } else if (_stricmp(argv[i], "-f") == 0) {
            if (++i == argc) {
                ShowHelpAndExit("-f");
            }
            *pFragShaderPath = argv[i];
            continue;
        }
        ShowHelpAndExit(argv[i]);
    }
}

int main(int argc, char** argv)
{
    std::string modelPath;
    std::string vertexShaderPath;
    std::string fragShaderPath;

    ParseCommandLine(
        argc, argv,
        &modelPath,
        &vertexShaderPath,
        &fragShaderPath);

    GLFWwindow* window =
        AppInit(
            modelPath,
            vertexShaderPath,
            fragShaderPath);
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
