#include "VulkanGraphicsApplication.h"

#include <iostream>

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
    vgfx::Application* pApp = reinterpret_cast<vgfx::Application*>(pUserData);
    return pApp->onValidationError(flags, objType, obj, location, code, layerPrefix, pMsg);
}

vgfx::Application::Application(
    const vgfx::Context::AppConfig& appConfig,
    const vgfx::Context::InstanceConfig& instanceConfig,
    const vgfx::Context::DeviceConfig& deviceConfig,
    std::unique_ptr<vgfx::Renderer> spRenderer)
    : m_spRenderer(std::move(spRenderer))
{
    m_graphicsContext.init(
        appConfig,
        instanceConfig,
        deviceConfig,
        m_spRenderer.get());

    if (std::find(
            instanceConfig.requiredExtensions.begin(),
            instanceConfig.requiredExtensions.end(),
            std::string(VK_EXT_DEBUG_REPORT_EXTENSION_NAME)) != std::end(instanceConfig.requiredExtensions)) {
        m_graphicsContext.enableDebugReportCallback(DebugCallback, this);
    }
}

VkBool32 vgfx::Application::onValidationError(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* pLayerPrefix, const char* pMsg)
{
    std::cerr << "Validation layer: " << pMsg << std::endl;

    return VK_FALSE;
}

const vgfx::WindowRenderer::SwapChainConfig& vgfx::WindowApplication::CreateSwapChainConfig(
    uint32_t width, uint32_t height,
    uint32_t imageUsageFlags,
    VkSurfaceFormatKHR surfaceFormat,
    const std::vector<VkFormat>& preferredDepthStencilFormats)
{
    static vgfx::WindowRenderer::SwapChainConfig swapChainConfig;
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

        swapChainConfig.pickDepthStencilFormat = [preferredDepthStencilFormats](const std::set<VkFormat>& formats) {
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

vgfx::WindowApplication::WindowApplication(
    const vgfx::Context::AppConfig& appConfig,
    const vgfx::Context::InstanceConfig& instanceConfig,
    const vgfx::Context::DeviceConfig& deviceConfig,
    const vgfx::WindowRenderer::SwapChainConfig& swapChainConfig,
    void* window,
    const std::function<VkResult(VkInstance, void*, const VkAllocationCallbacks*, VkSurfaceKHR*)>& createVulkanSurface)
    : Application(
        appConfig,
        instanceConfig,
        deviceConfig,
        std::make_unique<vgfx::WindowRenderer>(
            swapChainConfig,
            std::make_unique<vgfx::SwapChain>(
                window,
                swapChainConfig.imageExtent.has_value() ? swapChainConfig.imageExtent.value().width : 800,
                swapChainConfig.imageExtent.has_value() ? swapChainConfig.imageExtent.value().height : 600,
                createVulkanSurface)))
    , m_windowRenderer(reinterpret_cast<vgfx::WindowRenderer&>(*m_spRenderer))
{
    m_windowRenderer.initSwapChain(
        m_graphicsContext,
        // If only double buffering is available then one frame in flight, otherwise
        // 2 frames in flight (i.e. triple buffering).
        std::min(m_windowRenderer.getSwapChainConfig().imageCount.value() - 1u, 2u));
}

