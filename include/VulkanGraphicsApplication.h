#pragma once

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsRenderer.h"

#include <vulkan/vulkan.h>

namespace vgfx
{
    class Application
    {
    public:
        Application(
            const vgfx::Context::AppConfig& appConfig,
            const vgfx::Context::InstanceConfig& instanceConfig,
            const vgfx::Context::DeviceConfig& deviceConfig,
            std::unique_ptr<vgfx::Renderer> spRenderer);

        virtual VkBool32 onValidationError(
            VkDebugReportFlagsEXT flags,
            VkDebugReportObjectTypeEXT objType,
            uint64_t obj,
            size_t location,
            int32_t code,
            const char* pLayerPrefix,
            const char* pMsg);

        const vgfx::Context& getContext() const { return m_graphicsContext; }
        vgfx::Context& getContext() { return m_graphicsContext; }

    protected:
        vgfx::Context m_graphicsContext;
        std::unique_ptr<vgfx::Renderer> m_spRenderer;
    };


    class WindowApplication : public Application
    {
    public:
        static const vgfx::WindowRenderer::SwapChainConfig& CreateSwapChainConfig(
            uint32_t width, uint32_t height,
            uint32_t imageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLORSPACE_SRGB_NONLINEAR_KHR },
            const std::vector<VkFormat>& preferredDepthStencilFormats = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT });

        WindowApplication(
            const vgfx::Context::AppConfig& appConfig,
            const vgfx::Context::InstanceConfig& instanceConfig,
            const vgfx::Context::DeviceConfig& deviceConfig,
            const vgfx::WindowRenderer::SwapChainConfig& swapChainConfig,
            void* window,
            const std::function<VkResult(VkInstance, void*, const VkAllocationCallbacks*, VkSurfaceKHR*)>& createVulkanSurface);

        const vgfx::WindowRenderer& getRenderer() const { return m_windowRenderer; }
        vgfx::WindowRenderer& getRenderer() { return m_windowRenderer; }

    private:
        vgfx::WindowRenderer& m_windowRenderer;
    };
}
