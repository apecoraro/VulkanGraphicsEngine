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

    if (instanceConfig.enableDebugLayers) {
        m_graphicsContext.enableDebugReportCallback(DebugCallback, this);
    }
}

VkBool32 Application::onValidationError(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* pLayerPrefix, const char* pMsg)
{
    std::cerr << "Validation layer: " << pMsg << std::endl;

    return VK_FALSE;
}

const WindowRenderer::SwapChainConfig& WindowApplication::CreateSwapChainConfig(
    uint32_t imageCount,
    uint32_t width, uint32_t height,
    uint32_t imageUsageFlags,
    VkSurfaceFormatKHR surfaceFormat,
    const std::vector<VkFormat>& preferredDepthStencilFormats)
{
    WindowRenderer::SwapChainConfig swapChainConfig;
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

    swapChainConfig.pickDepthStencilFormat =
        [preferredDepthStencilFormats](const std::set<VkFormat>& formats) {
            for (const auto& preferredDepthStencilFormat : preferredDepthStencilFormats) {
                if (formats.find(preferredDepthStencilFormat) != formats.end()) {
                    return preferredDepthStencilFormat;
                }
            }
            throw std::runtime_error("Failed to find suitable depth stencil format!");
            return VK_FORMAT_MAX_ENUM;
        };
    return swapChainConfig;
}

WindowApplication::WindowApplication(
    const Context::AppConfig& appConfig,
    const Context::InstanceConfig& instanceConfig,
    const Context::DeviceConfig& deviceConfig,
    const WindowRenderer::SwapChainConfig& swapChainConfig,
    void* pWindow,
    CreateVulkanSurfaceFunc createVulkanSurface)
    : Application(
        appConfig,
        instanceConfig,
        deviceConfig)
    , m_swapChainConfig(swapChainConfig)
    , m_pWindow(pWindow)
    , m_createVulkanSurface(createVulkanSurface)
{
}

std::unique_ptr<Renderer> WindowApplication::createRenderer(
    const Context::AppConfig& appConfig,
    const Context::InstanceConfig& instanceConfig,
    const Context::DeviceConfig& deviceConfig)
{
    return std::make_unique<WindowRenderer>(
        m_swapChainConfig,
        std::make_unique<SwapChain>(
            m_pWindow,
            m_swapChainConfig.imageExtent.has_value() ? m_swapChainConfig.imageExtent.value().width : 800,
            m_swapChainConfig.imageExtent.has_value() ? m_swapChainConfig.imageExtent.value().height : 600,
            m_createVulkanSurface));
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
    m_pWindowRenderer->init(
        m_graphicsContext,
        m_swapChainConfig.imageCount.value());
}

