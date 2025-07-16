#pragma once

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsModelLibrary.h"
#include "VulkanGraphicsRenderer.h"

#include <vulkan/vulkan.h>

namespace vgfx
{
    class CommandBufferFactory;

    class Application
    {
    public:
        Application() = delete;

        using CreateRendererFunc = std::function<std::unique_ptr<Renderer>(
            Context::AppConfig& appConfig,
            Context::InstanceConfig& instanceConfig,
            Context::DeviceConfig& deviceConfig)>;

        using ConfigFunc = std::function<void(
            Context::AppConfig& appConfig,
            Context::InstanceConfig& instanceConfig,
            Context::DeviceConfig& deviceConfig)>;

        Application(
            CreateRendererFunc createRendererFunc,
            ConfigFunc initFunc,
            const Context::AppConfig& appConfig,
            const Context::InstanceConfig& instanceConfig,
            const Context::DeviceConfig& deviceConfig);

        virtual VkBool32 onValidationError(
            VkDebugReportFlagsEXT flags,
            VkDebugReportObjectTypeEXT objType,
            uint64_t obj,
            size_t location,
            int32_t code,
            const char* pLayerPrefix,
            const char* pMsg);

        const Context& getContext() const { return m_graphicsContext; }
        Context& getContext() { return m_graphicsContext; }

        void setScene(std::unique_ptr<vgfx::SceneNode>&& spSceneRoot) {
            m_spSceneRoot = std::move(spSceneRoot);
        }

        virtual void run() = 0;

    protected:
        Context m_graphicsContext;
        Context::ValidationLayerFunc m_validationLayerFunc = nullptr;
        std::unique_ptr<Renderer> m_spRenderer;
        std::unique_ptr<CommandBufferFactory> m_spUtilCommandBufferFactory;
        std::unique_ptr<vgfx::SceneNode> m_spSceneRoot;
    };

    class WindowApplication : public Application
    {
    public:
        static SwapChain::Config CreateSwapChainConfig(
            uint32_t imageCount=3,
            uint32_t width=0, uint32_t height=0,
            uint32_t imageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLORSPACE_SRGB_NONLINEAR_KHR },
            std::vector<VkFormat> preferredDepthStencilFormats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT });

        WindowApplication() = delete;

        using CreateVulkanSurfaceFunc = std::function<VkResult(VkInstance, void*, const VkAllocationCallbacks*, VkSurfaceKHR*)>;

        using CreateWindowRendererFunc = std::function<std::unique_ptr<WindowRenderer>(
            Context::AppConfig& appConfig,
            Context::InstanceConfig& instanceConfig,
            Context::DeviceConfig& deviceConfig,
            SwapChain::Config& swapChainConfig,
            void* pWindow,
            CreateVulkanSurfaceFunc createVulkanSurfaceFunc)>;

        WindowApplication(
            const Context::AppConfig& appConfig,
            const Context::InstanceConfig& instanceConfig,
            const Context::DeviceConfig& deviceConfig,
            const SwapChain::Config& swapChainConfig,
            void* pWindow,
            CreateVulkanSurfaceFunc createVulkanSurfaceFunc,
            CreateWindowRendererFunc createRendererFunc=nullptr,
            ConfigFunc configFunc=nullptr);

        const WindowRenderer& getRenderer() const { return *m_pWindowRenderer; }
        WindowRenderer& getRenderer() { return *m_pWindowRenderer; }

        void setFrameBufferResized(bool flag) {
            m_frameBufferResized = flag;
        }

    protected:
        std::unique_ptr<Renderer> createRenderer(
            const Context::AppConfig& appConfig,
            const Context::InstanceConfig& instanceConfig,
            const Context::DeviceConfig& deviceConfig);
        void initSwapChain(
            const Context::AppConfig& appConfig,
            const Context::InstanceConfig& instanceConfig,
            const Context::DeviceConfig& deviceConfig);

        void resizeWindow(uint32_t width, uint32_t height);

        SwapChain::Config m_swapChainConfig;
        void* m_pWindow = nullptr;
        CreateVulkanSurfaceFunc m_createVulkanSurface;
        WindowRenderer* m_pWindowRenderer = nullptr;
        bool m_frameBufferResized = false;
    };
}
