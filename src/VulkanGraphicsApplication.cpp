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
    CreateRendererFunc createRendererFunc,
    ConfigFunc configFunc,
    const Context::AppConfig& appConfig,
    const Context::InstanceConfig& instanceConfig,
    const Context::DeviceConfig& deviceConfig)
    : m_validationLayerFunc(appConfig.onValidationLayerFunc)
{
    Context::AppConfig appConfigFinal = appConfig;
    Context::InstanceConfig instanceConfigFinal = instanceConfig;
    Context::DeviceConfig deviceConfigFinal = deviceConfig;

    m_spRenderer = createRendererFunc(appConfigFinal, instanceConfigFinal, deviceConfigFinal);

    configFunc(appConfigFinal, instanceConfigFinal, deviceConfigFinal);

    m_graphicsContext.init(
        appConfigFinal,
        instanceConfigFinal,
        deviceConfigFinal,
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

    swapChainConfig.preferredDepthStencilFormats = preferredDepthStencilFormats;

    return swapChainConfig;
}

WindowApplication::WindowApplication(
    const Context::AppConfig& appConfig,
    const Context::InstanceConfig& instanceConfig,
    const Context::DeviceConfig& deviceConfig,
    const SwapChain::Config& swapChainConfig,
    void* pWindow,
    CreateVulkanSurfaceFunc createVulkanSurfaceFunc,
    CreateWindowRendererFunc createRendererFunc,
    ConfigFunc configFunc)
    : Application(
        [&](Context::AppConfig& appConfig,
            Context::InstanceConfig& instanceConfig,
            Context::DeviceConfig& deviceConfig) {
                if (createRendererFunc != nullptr) {
                    SwapChain::Config swapChainConfigFinal = swapChainConfig;
                    return createRendererFunc(appConfig, instanceConfig, deviceConfig, swapChainConfigFinal, pWindow, createVulkanSurfaceFunc);
                }
                else {
                    return std::make_unique<WindowRenderer>(m_graphicsContext, swapChainConfig, pWindow, createVulkanSurfaceFunc);
                }
        },
        [&](Context::AppConfig& appConfig,
            Context::InstanceConfig& instanceConfig,
            Context::DeviceConfig& deviceConfig)
        {
            deviceConfig.graphicsQueueRequired = true;
            deviceConfig.requiredDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            if (configFunc != nullptr) {
                configFunc(appConfig, instanceConfig, deviceConfig);
            }
        },
        appConfig,
        instanceConfig,
        deviceConfig)
    , m_swapChainConfig(swapChainConfig)
    , m_pWindow(pWindow)
    , m_createVulkanSurface(createVulkanSurfaceFunc)
{
    m_pWindowRenderer = reinterpret_cast<WindowRenderer*>(m_spRenderer.get());
    // Swap chain creation has to happen after the graphics context is fully
    // initialized by the Application parent class.
    m_pWindowRenderer->initSwapChain();
}

void vgfx::WindowApplication::resizeWindow(uint32_t width, uint32_t height)
{
    m_pWindowRenderer->resizeWindow(width, height);

    // TODO somehow notify m_spSceneRoot that resize has happened.
}

