#include "VulkanGraphicsGLFWApplication.h"

#include <sstream>
#include <iostream>

using namespace demo;

static void FramebufferResizedCallback(
    GLFWwindow* pWindow, // window
    int, // width
    int) // height
{
    GLFWApplication* pThis = reinterpret_cast<GLFWApplication*>(glfwGetWindowUserPointer(pWindow));
    pThis->setFrameBufferResized(true);
}

static GLFWwindow* InitWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Vulkan", nullptr, nullptr);

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

GLFWApplication::GLFWApplication(
    const vgfx::Context::AppConfig& appConfig,
    const vgfx::Context::InstanceConfig& instanceConfig,
    const vgfx::Context::DeviceConfig& deviceConfig,
    const vgfx::WindowRenderer::SwapChainConfig& swapChainConfig)
    : m_pWindow(InitWindow())
{
    if (!m_pWindow)
        throw std::runtime_error("Failed to init GLFW window!");

    glfwSetWindowUserPointer(m_pWindow, this);
    glfwSetFramebufferSizeCallback(m_pWindow, FramebufferResizedCallback);

    uint32_t glfwExtensionCount;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    bool enableValidationLayers = true;
    vgfx::Context::InstanceConfig glfwInstanceConfig = instanceConfig;
    glfwInstanceConfig.requiredExtensions.insert(
        glfwInstanceConfig.requiredExtensions.end(),
        glfwExtensions,
        glfwExtensions + glfwExtensionCount);

    vgfx::Context::DeviceConfig glfwDeviceConfig = deviceConfig;
    glfwDeviceConfig.graphicsQueueRequired = true;
    glfwDeviceConfig.requiredDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    int32_t windowWidth, windowHeight;
    // Current size of window is used as default if SwapChainConfig::imageExtent is not specified.
    GetFrameBufferSize(m_pWindow, &windowWidth, &windowHeight);

    initWindowApplication(
        appConfig,
        glfwInstanceConfig,
        deviceConfig,
        vgfx::WindowApplication::CreateSwapChainConfig(windowWidth, windowHeight),
        m_pWindow,
        CreateWindowSurfaceCallback);
}

GLFWApplication::~GLFWApplication()
{
    glfwDestroyWindow(m_pWindow);
    glfwTerminate();
}

GLFWApplication::run()
{
    vgfx::WindowRenderer& renderer = getRenderer();
    while (!glfwWindowShouldClose(m_pWindow)) {
        glfwPollEvents();
        if (!renderer.drawScene(*m_spScene.get()))
            break;
    }
}
