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
    const Context::DeviceConfig& deviceConfig,
    std::unique_ptr<Renderer> spRenderer)
{
    init(appConfig, instanceConfig, deviceConfig, std::move(spRenderer));
}

void Application::init(
    const Context::AppConfig& appConfig,
    const Context::InstanceConfig& instanceConfig,
    const Context::DeviceConfig& deviceConfig,
    std::unique_ptr<Renderer>&& spRenderer)
{
    m_spRenderer = std::move(spRenderer);

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
    uint32_t width, uint32_t height,
    uint32_t imageUsageFlags,
    VkSurfaceFormatKHR surfaceFormat,
    const std::vector<VkFormat>& preferredDepthStencilFormats)
{
    static WindowRenderer::SwapChainConfig swapChainConfig;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;

        swapChainConfig.imageCount = 3;
        // Seems like if the window is not fullscreen then we are basically limited to the size of
        // the window for the ImageQueue. If fullscreen then there might be other choices, although
        // I need to test that out a bit.
        swapChainConfig.imageExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

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
    }
    return swapChainConfig;
}

WindowApplication::WindowApplication(
    const Context::AppConfig& appConfig,
    const Context::InstanceConfig& instanceConfig,
    const Context::DeviceConfig& deviceConfig,
    const WindowRenderer::SwapChainConfig& swapChainConfig,
    void* window,
    const std::function<VkResult(VkInstance, void*, const VkAllocationCallbacks*, VkSurfaceKHR*)>& createVulkanSurface)
    : Application(
        appConfig,
        instanceConfig,
        deviceConfig,
        std::make_unique<WindowRenderer>(
            swapChainConfig,
            std::make_unique<SwapChain>(
                window,
                swapChainConfig.imageExtent.has_value() ? swapChainConfig.imageExtent.value().width : 800,
                swapChainConfig.imageExtent.has_value() ? swapChainConfig.imageExtent.value().height : 600,
                createVulkanSurface)))
    , m_pWindowRenderer(reinterpret_cast<WindowRenderer*>(m_spRenderer.get()))
{
    m_pWindowRenderer->init(
        m_graphicsContext,
        m_pWindowRenderer->getSwapChainConfig().imageCount.value());
}

void WindowApplication::initWindowApplication(
    const Context::AppConfig& appConfig,
    const Context::InstanceConfig& instanceConfig,
    const Context::DeviceConfig& deviceConfig,
    const WindowRenderer::SwapChainConfig& swapChainConfig,
    void* window,
    const std::function<VkResult(VkInstance, void*, const VkAllocationCallbacks*, VkSurfaceKHR*)>& createVulkanSurface)
{
    std::unique_ptr<WindowRenderer> spWindowRenderer = std::make_unique<WindowRenderer>(
        swapChainConfig,
        std::make_unique<SwapChain>(
            window,
            swapChainConfig.imageExtent.has_value() ? swapChainConfig.imageExtent.value().width : 800,
            swapChainConfig.imageExtent.has_value() ? swapChainConfig.imageExtent.value().height : 600,
            createVulkanSurface));
    Application::init(appConfig, instanceConfig, deviceConfig, std::move(spWindowRenderer));
    m_pWindowRenderer = reinterpret_cast<WindowRenderer*>(m_spRenderer.get());
    m_pWindowRenderer->init(
        m_graphicsContext,
        m_pWindowRenderer->getSwapChainConfig().imageCount.value());
}

