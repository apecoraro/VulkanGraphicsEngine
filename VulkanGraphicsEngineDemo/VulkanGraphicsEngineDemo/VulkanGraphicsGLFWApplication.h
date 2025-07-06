#include "VulkanGraphicsApplication.h"
#include "VulkanGraphicsContext.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace demo
{
    class GLFWApplication : public vgfx::WindowApplication
    {
    public:
        GLFWApplication(
            const vgfx::Context::AppConfig& appConfig,
            const vgfx::Context::InstanceConfig& instanceConfig,
            const vgfx::Context::DeviceConfig& deviceConfig,
            const vgfx::WindowRenderer::SwapChainConfig& swapChainConfig);

        virtual ~GLFWApplication();

        void run() override;
    protected:
        std::unique_ptr<vgfx::Renderer> createRenderer(
            const vgfx::Context::AppConfig& appConfig,
            const vgfx::Context::InstanceConfig& instanceConfig,
            const vgfx::Context::DeviceConfig& deviceConfig) override;
        void init(
            const vgfx::Context::AppConfig& appConfig,
            const vgfx::Context::InstanceConfig& instanceConfig,
            const vgfx::Context::DeviceConfig& deviceConfig) override;
    private:
        GLFWwindow* m_pGLFWwindow = nullptr;
    };
}