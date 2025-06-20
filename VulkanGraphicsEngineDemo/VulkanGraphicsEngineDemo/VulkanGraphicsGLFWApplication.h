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

        void setScene(std::unique_ptr<vgfx::SceneNode>&& spSceneRoot) {
            m_spSceneRoot = std::move(spSceneRoot);
        }

        void setFrameBufferResized(bool flag) {
            m_frameBufferResized = flag;
        }

        void run() override;
    private:
        GLFWwindow* m_pWindow = InitWindow();
        std::unique_ptr<vgfx::SceneNode> m_spSceneRoot;
        bool m_frameBufferResized = false;
    };
}