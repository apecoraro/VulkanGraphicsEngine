#include "VulkanGraphicsApplication.h"

#include <iostream>

using namespace vgfx;

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char* layerPrefix,
    const char* pMsg,
    void* pUserData)
{
    Application* pApp = reinterpret_cast<Application*>(pUserData);
    return pApp->onValidationError(flags, objType, obj, location, code, layerPrefix, pMsg);
}

Application::Application(
    const Context::AppConfig& appConfig,
    const Context::InstanceConfig& instanceConfig,
    const Context::DeviceConfig& deviceConfig)
    : m_validationLayerFunc(appConfig.onValidationLayerFunc)
{
    init(appConfig, instanceConfig, deviceConfig);
}

void Application::init(
    const Context::AppConfig& appConfig,
    const Context::InstanceConfig& instanceConfig,
    const Context::DeviceConfig& deviceConfig)
{
    m_spRenderer = createRenderer(appConfig, instanceConfig, deviceConfig);

    m_graphicsContext.init(
        appConfig,
        instanceConfig,
        deviceConfig,
        m_spRenderer.get());

    if (appConfig.enableValidationLayers) {
        m_graphicsContext.enableDebugReportCallback(DebugCallback, this);
    }
}

VkBool32 Application::onValidationError(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objType,
    uint64_t obj,
    size_t location,
    int32_t code,
    const char* pLayerPrefix,
    const char* pMsg)
{
    if (m_validationLayerFunc == nullptr) {
        std::cerr << "Validation layer: " << pMsg << std::endl;
        return VK_FALSE;
    }

    return m_validationLayerFunc(flags, objType, obj, location, code, pLayerPrefix, pMsg);
}

SwapChain::Config WindowApplication::CreateSwapChainConfig(
    uint32_t imageCount,
    uint32_t width, uint32_t height,
    uint32_t imageUsageFlags,
    VkSurfaceFormatKHR surfaceFormat,
    std::vector<VkFormat> preferredDepthStencilFormats)
{
    SwapChain::Config swapChainConfig;
    if (imageCount > 0u) {
        swapChainConfig.imageCount = imageCount;
    }
    // Seems like if the window is not fullscreen then we are basically limited to the size of
    // the window for the ImageQueue. If fullscreen then there might be other choices, although
    // I need to test that out a bit.
    if (width > 0u && height > 0u) {
        swapChainConfig.imageExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };
    }

    swapChainConfig.imageUsage = imageUsageFlags;

    swapChainConfig.imageFormat = surfaceFormat;

    swapChainConfig.preferredDepthStencilFormats = preferredDepthStencilFormats;

    return swapChainConfig;
}

WindowApplication::WindowApplication(
    const Context::AppConfig& appConfig,
    const Context::InstanceConfig& instanceConfig,
    const Context::DeviceConfig& deviceConfig,
    const SwapChain::Config& swapChainConfig,
    void* pWindow,
    CreateVulkanSurfaceFunc createVulkanSurfaceFunc)
    : Application(
        appConfig,
        instanceConfig,
        deviceConfig)
    , m_swapChainConfig(swapChainConfig)
    , m_pWindow(pWindow)
    , m_createVulkanSurface(createVulkanSurfaceFunc)
{
}

std::unique_ptr<Renderer> WindowApplication::createRenderer(
    const Context::AppConfig& appConfig,
    const Context::InstanceConfig& instanceConfig,
    const Context::DeviceConfig& deviceConfig)
{
    return std::make_unique<WindowRenderer>(m_graphicsContext, m_swapChainConfig, m_createVulkanSurface);
}

// How does this get called? Also the Application needs to set the debug validation layers based on the app config
void WindowApplication::init(
    const Context::AppConfig& appConfig,
    const Context::InstanceConfig& instanceConfig,
    const Context::DeviceConfig& deviceConfig)
{
    vgfx::Context::DeviceConfig deviceConfigFinal = deviceConfig;
    deviceConfigFinal.graphicsQueueRequired = true;
    deviceConfigFinal.requiredDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    Application::init(appConfig, instanceConfig, deviceConfig);

    m_pWindowRenderer = reinterpret_cast<WindowRenderer*>(m_spRenderer.get());
    m_pWindowRenderer->init(m_swapChainConfig.imageCount.value());
}

void vgfx::WindowApplication::resizeWindow(int32_t width, int32_t height)
{
    m_pWindowRenderer->resizeWindow(width, height);

    // TODO somehow notify m_spSceneRoot that resize has happened.
}

