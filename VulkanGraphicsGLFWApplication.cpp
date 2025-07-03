#include "VulkanGraphicsGLFWApplication.h"

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

static void GetFrameBufferSize(void* window, int* pWidth, int* pHeight)
{
    int width = 0, height = 0;
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);
        glfwWaitEvents();
    }

    *pWidth = width;
    *pHeight = height;
}

vgfx::GLFWApplication::GLFWApplication(
    const vgfx::Context::AppConfig& appConfig,
    const vgfx::Context::InstanceConfig& instanceConfig,
    const vgfx::Context::DeviceConfig& deviceConfig,
    const vgfx::WindowRenderer::SwapChainConfig& swapChainConfig)
{
    GLFWwindow* pWindow = InitWindow();
    if (!pWindow)
        throw std::runtime_error("Failed to init GLFW window!");

    uint32_t glfwExtensionCount;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    bool enableValidationLayers = true;
    vgfx::Context::InstanceConfig glfwInstanceConfig = instanceConfig;
    glfwInstanceConfig.requiredExtensions.insert(
        glfwInstanceConfig.requiredExtensions.end(),
        platformSpecificExtensions,
        platformSpecificExtensions + platformSpecificExtensionCount);

    vgfx::Context::DeviceConfig glfwDeviceConfig = deviceConfig;
    glfwDeviceConfig.graphicsQueueRequired = true;
    glfwDeviceConfig.requiredDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    int32_t windowWidth, windowHeight;
    // Current size of window is used as default if SwapChainConfig::imageExtent is not specified.
    GetFramebufferSize(pWindow, &windowWidth, &windowHeight);

    initWindowApplication(
        appConfig,
        glfwInstanceConfig,
        deviceConfig,
        vgfx::WindowApplication::CreateSwapChainConfig(windowWidth, windowHeight),
        pWindow,
        CreateWindowSurfaceCallback);
}
