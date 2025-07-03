#include "VulkanGraphicsApplication.h"
#include "VulkanGraphicsContext.h"

namespace vgfx
{
    class GLFWApplication : public WindowApplication
    {
    public:
        GLFWApplication(
            const vgfx::Context::AppConfig& appConfig,
            const vgfx::Context::InstanceConfig& instanceConfig,
            const vgfx::Context::DeviceConfig& deviceConfig,
            const vgfx::WindowRenderer::SwapChainConfig& swapChainConfig);

        void run() override;
    };
}