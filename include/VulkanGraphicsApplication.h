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
        Application() = default;
        Application(
            const Context::AppConfig& appConfig,
            const Context::InstanceConfig& instanceConfig,
            const Context::DeviceConfig& deviceConfig,
            std::unique_ptr<Renderer> spRenderer);

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

        virtual void run() = 0;

        Drawable& loadDrawable(
            const ModelLibrary::ModelDesc& modelDesc,
            const Material& material);

    protected:
        void init(
            const Context::AppConfig& appConfig,
            const Context::InstanceConfig& instanceConfig,
            const Context::DeviceConfig& deviceConfig,
            std::unique_ptr<Renderer>&& spRenderer);

        Context m_graphicsContext;
        std::unique_ptr<Renderer> m_spRenderer;
        std::unique_ptr<CommandBufferFactory> m_spUtilCommandBufferFactory;
    };

    class WindowApplication : public Application
    {
    public:
        static const WindowRenderer::SwapChainConfig& CreateSwapChainConfig(
            uint32_t width, uint32_t height,
            uint32_t imageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLORSPACE_SRGB_NONLINEAR_KHR },
            const std::vector<VkFormat>& preferredDepthStencilFormats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT });

        WindowApplication() = default;

        WindowApplication(
            const Context::AppConfig& appConfig,
            const Context::InstanceConfig& instanceConfig,
            const Context::DeviceConfig& deviceConfig,
            const WindowRenderer::SwapChainConfig& swapChainConfig,
            void* window,
            const std::function<VkResult(VkInstance, void*, const VkAllocationCallbacks*, VkSurfaceKHR*)>& createVulkanSurface);

        const WindowRenderer& getRenderer() const { return *m_pWindowRenderer; }
        WindowRenderer& getRenderer() { return *m_pWindowRenderer; }

    protected:
        void initWindowApplication(
            const Context::AppConfig& appConfig,
            const Context::InstanceConfig& instanceConfig,
            const Context::DeviceConfig& deviceConfig,
            const WindowRenderer::SwapChainConfig& swapChainConfig,
            void* window,
            const std::function<VkResult(VkInstance, void*, const VkAllocationCallbacks*, VkSurfaceKHR*)>& createVulkanSurface);

    private:
        WindowRenderer* m_pWindowRenderer;
    };
}
