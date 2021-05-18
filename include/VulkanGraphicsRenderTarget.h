#ifndef VGFX_RENDER_TARGET_H
#define VGFX_RENDER_TARGET_H

#include "VulkanGraphicsContext.h"

#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    class SwapChain;
    class DepthStencilBuffer;
    class RenderPass;

    class RenderTarget
    {
    public:
        struct Config
        {
            std::vector<const SwapChain*> swapChains;
            const DepthStencilBuffer* pDepthStencilBuffer = nullptr;

            Config(
                const SwapChain& swapChain,
                const DepthStencilBuffer* pDepthStencilBuffer = nullptr)
                : swapChains(1u, &swapChain)
                , pDepthStencilBuffer(pDepthStencilBuffer)
            {
            }

            Config(
                const DepthStencilBuffer& depthStencilBuffer)
                : pDepthStencilBuffer(&depthStencilBuffer)
            {
            }

            Config(
                const std::vector<const SwapChain*>& swapChainPtrs,
                const DepthStencilBuffer* pDepthStencilBuffer = nullptr)
                : swapChains(swapChainPtrs)
                , pDepthStencilBuffer(pDepthStencilBuffer)
            {
            }
        };

        RenderTarget(
            Context& context,
            const Config& config,
            const RenderPass& renderPassTemplate);

        ~RenderTarget()
        {
            destroy();
        }

        const DepthStencilBuffer* getDepthStencilBuffer() const { return m_pDepthStencilBuffer; }
        VkFramebuffer getFramebuffer(size_t index) const
        {
            return m_framebuffers[index % m_framebuffers.size()];
        }
        const VkExtent2D& getExtent() const { return m_renderExtent; }
    protected:
        void destroy();

        Context& m_context;

        const DepthStencilBuffer* m_pDepthStencilBuffer = nullptr;

        std::vector<VkFramebuffer> m_framebuffers;
        VkExtent2D m_renderExtent = {};
    };
}

#endif
