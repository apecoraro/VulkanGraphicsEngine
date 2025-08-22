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

static void GetFrameBufferSize(void* window, uint32_t* pWidth, uint32_t* pHeight)
{
    int width = 0, height = 0;
    glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);
        glfwWaitEvents();
    }

    *pWidth = static_cast<uint32_t>(width);
    *pHeight = static_cast<uint32_t>(height);
}

GLFWApplication::GLFWApplication(
    const vgfx::Context::AppConfig& appConfig,
    const vgfx::Context::InstanceConfig& instanceConfig,
    const vgfx::Context::DeviceConfig& deviceConfig,
    const vgfx::SwapChain::Config& swapChainConfig)
    : WindowApplication(
        appConfig,
        instanceConfig,
        deviceConfig,
        swapChainConfig,
        InitWindow(),
        CreateWindowSurfaceCallback,
        [&](vgfx::Context::AppConfig& appConfig,
            vgfx::Context::InstanceConfig& instanceConfig,
            vgfx::Context::DeviceConfig& deviceConfig,
            vgfx::SwapChain::Config& swapChainConfig,
            void* pWindow,
            vgfx::WindowApplication::CreateVulkanSurfaceFunc createVulkanSurfaceFunc)
        {
            GLFWwindow* pGlfwWindow = reinterpret_cast<GLFWwindow*>(pWindow);
            if (!swapChainConfig.imageExtent.has_value()) {
                VkExtent2D windowExtent;
                // Current size of window is used as default if SwapChainConfig::imageExtent is not specified.
                GetFrameBufferSize(pGlfwWindow, &windowExtent.width, &windowExtent.height);

                swapChainConfig.imageExtent = windowExtent;
            }

            return std::make_unique<vgfx::WindowRenderer>(m_graphicsContext, swapChainConfig, pWindow, createVulkanSurfaceFunc);
        },
        [&](vgfx::Context::AppConfig& appConfig,
            vgfx::Context::InstanceConfig& instanceConfig,
            vgfx::Context::DeviceConfig& deviceConfig)
        {
            uint32_t glfwExtensionCount;
            const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
            bool enableValidationLayers = true;
            instanceConfig.requiredExtensions.insert(
                instanceConfig.requiredExtensions.end(),
                glfwExtensions,
                glfwExtensions + glfwExtensionCount);
        })
{
    m_pGLFWwindow = reinterpret_cast<GLFWwindow*>(m_pWindow);
    if (!m_pGLFWwindow) {
        throw std::runtime_error("Failed to init GLFW window!");
    }

    glfwSetWindowUserPointer(m_pGLFWwindow, this);
    glfwSetFramebufferSizeCallback(m_pGLFWwindow, FramebufferResizedCallback);
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
        std::cerr << "renderframe" << std::endl;
        VkResult result = renderer.renderFrame(*m_spSceneRoot.get());
        std::cerr << "renderframe done" << std::endl;
        if (result == VK_ERROR_OUT_OF_DATE_KHR || m_frameBufferResized) {
            uint32_t width = 0u, height = 0u;
            GetFrameBufferSize(m_pGLFWwindow, &width, &height);
            resizeWindow(width, height);
        }
        else if (result != VK_SUCCESS) {
            break;
        }
    }
}
