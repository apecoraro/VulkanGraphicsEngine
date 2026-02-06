#pragma once

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsModelLibrary.h"
#include "VulkanGraphicsSceneLoader.h"
#include "VulkanGraphicsRenderer.h"

#include <vulkan/vulkan.h>

namespace vgfx
{
    class CommandBufferFactory;

    class Application
    {
    public:
        Application() = delete;

        // Use AppConfigFunc to configure the config objects specific to the app.
        using AppConfigFunc = std::function<void(
            Context::AppConfig& appConfig,
            Context::InstanceConfig& instanceConfig,
            Context::DeviceConfig& deviceConfig)>;

        // Use CreateRendererFunc to create a specific type of renderer for this app.
        using CreateRendererFunc = std::function<std::unique_ptr<Renderer>(
            Context::AppConfig& appConfig,
            Context::InstanceConfig& instanceConfig,
            Context::DeviceConfig& deviceConfig,
            Presenter& presenter)>;

        Application(
            AppConfigFunc configFunc,
            const Context::AppConfig& appConfig,
            const Context::InstanceConfig& instanceConfig,
            const Context::DeviceConfig& deviceConfig,
            Presenter& presenter,
            CreateRendererFunc createRendererFunc);

        ~Application();

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

        SceneLoader& getSceneLoader() { return *m_spSceneLoader.get(); }

        void setScene(std::unique_ptr<vgfx::SceneNode>&& spSceneRoot) {
            m_spSceneRoot = std::move(spSceneRoot);
        }

        Renderer& getRenderer() { return *m_spRenderer.get(); }

        virtual void run() = 0;

    protected:
        Context m_graphicsContext;
        Context::ValidationLayerFunc m_validationLayerFunc = nullptr;
        std::unique_ptr<Renderer> m_spRenderer;
        std::unique_ptr<SceneLoader> m_spSceneLoader;
        std::unique_ptr<vgfx::SceneNode> m_spSceneRoot;
    };

    class WindowApplication : public Application
    {
    public:
        static SwapChain::Config CreateSwapChainConfig(
            uint32_t imageCount=3,
            uint32_t width=0, uint32_t height=0,
            std::vector<VkFormat> preferredDepthStencilFormats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT });

        WindowApplication() = delete;

        WindowApplication(
            const Context::AppConfig& appConfig,
            const Context::InstanceConfig& instanceConfig,
            const Context::DeviceConfig& devConfig,
            AppConfigFunc configFunc,
            std::unique_ptr<SwapChainPresenter>&& spSwapChainPresenter,
            Application::CreateRendererFunc createRendererFunc);

        const SwapChainPresenter& getSwapChainPresenter() const { return *m_spSwapChainPresenter; }
        SwapChainPresenter& getSwapChainPresenter() { return *m_spSwapChainPresenter; }

        void setFrameBufferResized(bool flag) {
            m_frameBufferResized = flag;
        }

    protected:
        void resizeWindow(uint32_t width, uint32_t height);

        std::unique_ptr<SwapChainPresenter> m_spSwapChainPresenter = nullptr;
        bool m_frameBufferResized = false;
    };
}
