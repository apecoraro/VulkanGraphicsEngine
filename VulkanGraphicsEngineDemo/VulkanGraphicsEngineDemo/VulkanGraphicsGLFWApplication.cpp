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
    glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);
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
    : WindowApplication(
        appConfig,
        instanceConfig,
        deviceConfig,
        swapChainConfig,
        InitWindow(),
        CreateWindowSurfaceCallback)
{
}

void GLFWApplication::init(
    const vgfx::Context::AppConfig& appConfig,
    const vgfx::Context::InstanceConfig& instanceConfig,
    const vgfx::Context::DeviceConfig& deviceConfig)
{
    m_pGLFWwindow = reinterpret_cast<GLFWwindow*>(m_pWindow);
    if (!m_pGLFWwindow)
        throw std::runtime_error("Failed to init GLFW window!");

    glfwSetWindowUserPointer(m_pGLFWwindow, this);
    glfwSetFramebufferSizeCallback(m_pGLFWwindow, FramebufferResizedCallback);

    uint32_t glfwExtensionCount;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    bool enableValidationLayers = true;
    vgfx::Context::InstanceConfig glfwInstanceConfig = instanceConfig;
    glfwInstanceConfig.requiredExtensions.insert(
        glfwInstanceConfig.requiredExtensions.end(),
        glfwExtensions,
        glfwExtensions + glfwExtensionCount);

    WindowApplication::init(
        appConfig,
        glfwInstanceConfig,
        deviceConfig);
}

std::unique_ptr<vgfx::Renderer> GLFWApplication::createRenderer(
    const vgfx::Context::AppConfig& appConfig,
    const vgfx::Context::InstanceConfig& instanceConfig,
    const vgfx::Context::DeviceConfig& deviceConfig)
{
    vgfx::WindowRenderer::SwapChainConfig& glfwSwapChainConfig = m_swapChainConfig;
    if (!glfwSwapChainConfig.imageExtent.has_value()) {
        int32_t windowWidth, windowHeight;
        // Current size of window is used as default if SwapChainConfig::imageExtent is not specified.
        GetFrameBufferSize(m_pWindow, &windowWidth, &windowHeight);

        glfwSwapChainConfig.imageExtent->width = windowWidth;
        glfwSwapChainConfig.imageExtent->height = windowHeight;
    }

    return WindowApplication::createRenderer(appConfig, instanceConfig, deviceConfig);
}

GLFWApplication::~GLFWApplication()
{
    glfwDestroyWindow(m_pGLFWwindow);
    glfwTerminate();
}

void GLFWApplication::run()
{
    vgfx::WindowRenderer& renderer = getRenderer();
    while (!glfwWindowShouldClose(m_pGLFWwindow)) {
        glfwPollEvents();
        VkResult result = renderer.renderFrame(*m_spSceneRoot.get());
        if (result == VK_ERROR_OUT_OF_DATE_KHR || m_frameBufferResized) {
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            break;
        }
    }
}
