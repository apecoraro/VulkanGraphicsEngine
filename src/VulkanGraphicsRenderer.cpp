#include "VulkanGraphicsRenderer.h"

#include <algorithm>
#include <stdexcept>

namespace vgfx
{
    static bool SurfaceFormatIsSupported(
        const std::vector<VkSurfaceFormatKHR>& supportedFormats,
        VkSurfaceFormatKHR desiredSurfaceFormat)
    {
        for (const auto& imageFormat : supportedFormats) {
            if ((desiredSurfaceFormat.format == VK_FORMAT_UNDEFINED
                || imageFormat.format == desiredSurfaceFormat.format)
                && (desiredSurfaceFormat.colorSpace == VK_COLOR_SPACE_MAX_ENUM_KHR
                    || imageFormat.colorSpace == desiredSurfaceFormat.colorSpace)) {
                return true;
            }
        }

        return false;
    }

    bool PresentationModeIsSupported(
        const std::vector<VkPresentModeKHR>& supportedModes,
        VkPresentModeKHR desiredPresentMode)
    {
        for (auto& mode : supportedModes) {
            if (mode == desiredPresentMode) {
                return true;
            }
        }

        return false;
    }

    void WindowRenderer::checkIsDeviceSuitable(VkPhysicalDevice device) const
    {
        uint32_t minImageCount = 0u;
        uint32_t maxImageCount = 0u;
        VkExtent2D minImageExtent = { 0u, 0u };
        VkExtent2D maxImageExtent = { 0u, 0u };
        VkExtent2D curImageExtent = { 0u, 0u };
        VkSurfaceTransformFlagsKHR supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        VkSurfaceTransformFlagBitsKHR currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        VkCompositeAlphaFlagsKHR supportedCompositeAlphaModes = VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR;
        VkImageUsageFlags supportedImageUsageModes = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        m_spSwapChain->getImageCapabilities(
            device,
            &minImageCount, &maxImageCount,
            &minImageExtent, &maxImageExtent,
            &curImageExtent,
            &supportedTransforms,
            &currentTransform,
            &supportedCompositeAlphaModes,
            &supportedImageUsageModes);

        // Check required count and extents against capabilities.
        // If required count or extent is zero then that means there are no specific
        // requirements.
        if (m_swapChainConfig.imageCount.has_value() && m_swapChainConfig.imageCount.value() > maxImageCount) {
            throw std::runtime_error("Swap chain image count is greater than maxImageCount.");
        }

        if (m_swapChainConfig.imageCount.has_value() && m_swapChainConfig.imageCount.value() < minImageCount) {
            throw std::runtime_error("Swap chain image count is less than minImageCount.");
        }

        if (m_swapChainConfig.imageExtent.has_value()) {
            if (m_swapChainConfig.imageExtent->width < minImageExtent.width
                || m_swapChainConfig.imageExtent->height < minImageExtent.height) {
                throw std::runtime_error("Swap chain image extent is less than minImageExtent.");
            }

            if (m_swapChainConfig.imageExtent->width > maxImageExtent.width
                || m_swapChainConfig.imageExtent->height > maxImageExtent.height) {
                throw std::runtime_error("Swap chain image extent is greater than maxImageExtent.");
            }
        }

        if (m_swapChainConfig.preTransform.has_value()) {
            if (!m_swapChainConfig.preTransform & supportedTransforms) {
                throw std::runtime_error("Swap chain desired transform is not supported.");
            }
        }

        if (m_swapChainConfig.compositeAlphaMode.has_value()) {
            if (!m_swapChainConfig.compositeAlphaMode & supportedCompositeAlphaModes) {
                throw std::runtime_error("Swap chain desired composite alpha mode is not supported.");
            }
        }

        if (m_swapChainConfig.imageUsage.has_value()) {
            if (!m_swapChainConfig.imageUsage & supportedImageUsageModes) {
                throw std::runtime_error("Swap chain desired desired usage mode is not supported.");
            }
        }

        std::vector<VkSurfaceFormatKHR> supportedFormats;
        m_spSwapChain->getSupportedImageFormats(device, &supportedFormats);

        if (supportedFormats.empty()) {
            throw std::runtime_error("Swap chain does not support any image formats.");
        } else if (m_swapChainConfig.imageFormat.has_value()) {
            if(!SurfaceFormatIsSupported(supportedFormats, m_swapChainConfig.imageFormat.value())) {
                throw std::runtime_error("Swap chain does not support specified image format.");
            }
        }
        
        std::vector<VkPresentModeKHR> supportedModes;
        m_spSwapChain->getSupportedPresentationModes(device, &supportedModes);

        if (supportedModes.empty()) {
            throw std::runtime_error("Swap chain does not support any present modes.");
        } else if (m_swapChainConfig.presentMode.has_value()) {
            if (!PresentationModeIsSupported(supportedModes, m_swapChainConfig.presentMode.value())) {
                throw std::runtime_error("Swap chain does not support required present mode.");
            }
        }
    }

    bool WindowRenderer::queueFamilySupportsPresent(VkPhysicalDevice device, uint32_t familyIndex) const
    {
        return m_spSwapChain->surfaceSupportsQueueFamily(device, familyIndex);
    }

    void WindowRenderer::configureForDevice(VkPhysicalDevice device)
    {
        // Set defaults for all configuration parameters (if not explicitly set already).
        if (!m_swapChainConfig.imageCount.has_value()
            || !m_swapChainConfig.imageExtent.has_value()
            || !m_swapChainConfig.preTransform.has_value()) {
            // configure the image count, extent, and/or surface transform
            uint32_t minImageCount = 0u;
            uint32_t maxImageCount = 0u;
            VkExtent2D minImageExtent = { 0u, 0u };
            VkExtent2D maxImageExtent = { 0u, 0u };
            VkExtent2D curImageExtent = { 0u, 0u };
            VkSurfaceTransformFlagsKHR supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
            VkSurfaceTransformFlagBitsKHR currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
            VkCompositeAlphaFlagsKHR supportedCompositeAlphaModes = VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR;
            VkImageUsageFlags supportedImageUsageModes = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            m_spSwapChain->getImageCapabilities(
                device,
                &minImageCount, &maxImageCount,
                &minImageExtent, &maxImageExtent,
                &curImageExtent,
                &supportedTransforms,
                &currentTransform,
                &supportedCompositeAlphaModes,
                &supportedImageUsageModes);

            if (!m_swapChainConfig.imageCount.has_value()) {
                if (m_chooseImageCountFunc == nullptr) {
                    m_swapChainConfig.imageCount = std::min(minImageCount + 1, maxImageCount);
                } else {
                    m_swapChainConfig.imageCount = m_chooseImageCountFunc(minImageCount, maxImageCount);
                }
            }

            if (!m_swapChainConfig.imageExtent.has_value()) {
                if (curImageExtent.width == 0xFFFFFFFF && curImageExtent.height == 0xFFFFFFFF) {
                    m_spSwapChain->getWindowInitialSize(&curImageExtent.width, &curImageExtent.height);
                }

                if (m_chooseWindowExtentFunc != nullptr) {
                    m_swapChainConfig.imageExtent = m_chooseWindowExtentFunc(minImageExtent, maxImageExtent, curImageExtent);
                } else {
                    m_swapChainConfig.imageExtent->width = std::max(minImageExtent.width, std::min(maxImageExtent.width, curImageExtent.width));
                    m_swapChainConfig.imageExtent->height = std::max(minImageExtent.height, std::min(maxImageExtent.height, curImageExtent.height));
                }
            }

            if (!m_swapChainConfig.preTransform.has_value()) {
                m_swapChainConfig.preTransform = currentTransform;
            }
        }

        if (!m_swapChainConfig.compositeAlphaMode.has_value()) {
            m_swapChainConfig.compositeAlphaMode = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        }

        if (!m_swapChainConfig.imageUsage.has_value()) {
            m_swapChainConfig.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }

        if (!m_swapChainConfig.imageFormat.has_value()) {
            // configure the surface format
            std::vector<VkSurfaceFormatKHR> supportedFormats;
            m_spSwapChain->getSupportedImageFormats(device, &supportedFormats);

            if (m_chooseSurfaceFormatFunc != nullptr) {
                m_swapChainConfig.imageFormat = m_chooseSurfaceFormatFunc(supportedFormats);
            } else {
                m_swapChainConfig.imageFormat = {
                    VK_FORMAT_B8G8R8A8_SRGB,
                    VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
                };
                if (!SurfaceFormatIsSupported(supportedFormats, m_swapChainConfig.imageFormat.value())) {
                    m_swapChainConfig.imageFormat = supportedFormats[0];
                }
            }
        }

        if (!m_swapChainConfig.presentMode.has_value()) {
            // configure presentation mode
            std::vector<VkPresentModeKHR> supportedModes;
            m_spSwapChain->getSupportedPresentationModes(device, &supportedModes);

            if (m_choosePresentModeFunc != nullptr) {
                m_swapChainConfig.presentMode = m_choosePresentModeFunc(supportedModes);
            } else {
                m_swapChainConfig.presentMode = VK_PRESENT_MODE_FIFO_KHR;
            }
        }
    }

    void WindowRenderer::initSwapChain(
        Context& context,
        const SwapChain::CreateImageViewFunc& createImageViewFunc,
        uint32_t maxFramesInFlight,
        const vgfx::RenderTarget::Config& renderTargetConfig)
    {
        WindowSwapChain::Config swapChainConfig(
            m_swapChainConfig.imageCount.value(),
            maxFramesInFlight,
            m_swapChainConfig.imageFormat.value(),
            m_swapChainConfig.imageExtent.value(),
            m_swapChainConfig.imageUsage.value(),
            m_swapChainConfig.compositeAlphaMode.value(),
            m_swapChainConfig.presentMode.value(),
            m_swapChainConfig.preTransform.value());

        m_spSwapChain->createRenderingResources(context, swapChainConfig, createImageViewFunc);

        // Don't know how many images were created until after createRenderingResources completes, so have
        // to query to get this value even though we passed it in.
        size_t actualMaxFramesInFlight = m_spSwapChain->getImageAvailableSemaphoreCount();

        m_renderFinishedSemaphores.reserve(actualMaxFramesInFlight);
        for (size_t i = 0; i < actualMaxFramesInFlight; ++i) {
            m_renderFinishedSemaphores.push_back(
                std::make_unique<Semaphore>(context));

            m_inFlightFences.push_back(
                std::make_unique<Fence>(context));
        }

        m_spRenderTarget =
            std::make_unique<RenderTarget>(context, *m_spSwapChain.get(), renderTargetConfig);

        m_pContext = &context;
    }

    void WindowRenderer::recordCommandBuffer(
        VkCommandBuffer commandBuffer,
        size_t swapChainImageIndex,
        const Pipeline& pipeline,
        const std::vector<std::unique_ptr<Object>>& objects)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

        VkImageSubresourceRange imageSubresourceRange = {};
        imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageSubresourceRange.baseMipLevel = 0;
        imageSubresourceRange.levelCount = 1;
        imageSubresourceRange.baseArrayLayer = 0;
        imageSubresourceRange.layerCount = 1;

        VkClearValue clearValue = {
          { 0.0f, 1.0f, 0.0f, 1.0f },
        };

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkImage swapChainImage = m_spSwapChain->getImageHandle(swapChainImageIndex);

        bool imageBarrierNeeded =
            m_pContext->getPresentQueue(0).queue != m_pContext->getGraphicsQueue(0).queue;
        if (imageBarrierNeeded) {
            VkImageMemoryBarrier barrierFromPresentToDraw = {};
            barrierFromPresentToDraw.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrierFromPresentToDraw.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barrierFromPresentToDraw.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrierFromPresentToDraw.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierFromPresentToDraw.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierFromPresentToDraw.srcQueueFamilyIndex = m_pContext->getPresentQueueFamilyIndex().value();
            barrierFromPresentToDraw.dstQueueFamilyIndex = m_pContext->getGraphicsQueueFamilyIndex().value();
            barrierFromPresentToDraw.image = swapChainImage;
            barrierFromPresentToDraw.subresourceRange = imageSubresourceRange;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                0, // VkDependencyFlags
                0, // memory barrier count
                nullptr, // VkMemoryBarrier*
                0, // buffer barrier count
                nullptr, // VkBufferMemoryBarrier*
                1, // image memory barrier count
                &barrierFromPresentToDraw);
        }

        VkRenderPassBeginInfo renderPassBeginInfo = {};
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = m_spRenderTarget->getRenderPass();
        renderPassBeginInfo.framebuffer = m_spRenderTarget->getFramebuffer(swapChainImageIndex);
        renderPassBeginInfo.renderArea.offset = { 0, 0 };
        renderPassBeginInfo.renderArea.extent = m_spSwapChain->getImageExtent();
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearValue;

        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getHandle());

        for (auto& spObject : objects) {
            spObject->recordDrawCommands(swapChainImageIndex, commandBuffer);
        }

        vkCmdEndRenderPass(commandBuffer);

        if (imageBarrierNeeded) {
            VkImageMemoryBarrier barrierFromDrawToPresent = {};
            barrierFromDrawToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrierFromDrawToPresent.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrierFromDrawToPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barrierFromDrawToPresent.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierFromDrawToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierFromDrawToPresent.srcQueueFamilyIndex = m_pContext->getGraphicsQueueFamilyIndex().value();
            barrierFromDrawToPresent.dstQueueFamilyIndex = m_pContext->getPresentQueueFamilyIndex().value();
            barrierFromDrawToPresent.image = swapChainImage;
            barrierFromDrawToPresent.subresourceRange = imageSubresourceRange;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0, // VkDependencyFlags
                0, // memory barrier count
                nullptr, // VkMemoryBarrier*
                0, // buffer barrier count
                nullptr, // VkBufferMemoryBarrier*
                1, // image memory barrier count
                &barrierFromDrawToPresent);
        }

        vkEndCommandBuffer(commandBuffer);
    }

    VkResult WindowRenderer::acquireNextSwapChainImage(uint32_t* pSwapChainImageIndex)
    {
        VkDevice device = m_pContext->getLogicalDevice();
        VkFence fences[] = { m_inFlightFences[m_syncObjIndex]->getHandle() };
        vkWaitForFences(
            device,
            1,
            fences,
            VK_TRUE,
            std::numeric_limits<uint64_t>::max());

        return vkAcquireNextImageKHR(
            device,
            m_spSwapChain->getHandle(),
            std::numeric_limits<uint64_t>::max(),
            m_spSwapChain->getImageAvailableSemaphore(m_syncObjIndex),
            VK_NULL_HANDLE,
            pSwapChainImageIndex);
    }

    VkResult WindowRenderer::renderFrame(
        uint32_t swapChainImageIndex,
        const QueueSubmitInfo& submitInfo)
    {
        VkSubmitInfo vkSubmitInfo = {};
        vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        vkSubmitInfo.waitSemaphoreCount = 1u + static_cast<uint32_t>(submitInfo.waitSemaphores.size());
        // The intention is that by making these vectors persistent across multiple calls to renderFrame that
        // after just a few frames they will have settled on a good size that doesn't require reallocation.
        // We clear them at the end of the frame because vectors don't release their memory unless shrink_to_fit
        // is called, so by clearing it simplifies the process of building them on the next frame.
        m_gfxQueueSubmitInfo.clearAll();
        if (vkSubmitInfo.waitSemaphoreCount > m_gfxQueueSubmitInfo.waitSemaphores.capacity()) {
            m_gfxQueueSubmitInfo.waitSemaphores.reserve(vkSubmitInfo.waitSemaphoreCount);
            m_gfxQueueSubmitInfo.waitSemaphoreStages.reserve(vkSubmitInfo.waitSemaphoreCount);
        }

        m_gfxQueueSubmitInfo.waitSemaphores.push_back(m_spSwapChain->getImageAvailableSemaphore(m_syncObjIndex));
        m_gfxQueueSubmitInfo.waitSemaphoreStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        if (!submitInfo.waitSemaphores.empty()) {
            m_gfxQueueSubmitInfo.waitSemaphores.insert(
                std::end(m_gfxQueueSubmitInfo.waitSemaphores),
                std::begin(submitInfo.waitSemaphores),
                std::end(submitInfo.waitSemaphores));

            m_gfxQueueSubmitInfo.waitSemaphoreStages.insert(
                std::end(m_gfxQueueSubmitInfo.waitSemaphoreStages),
                std::begin(submitInfo.waitSemaphoreStages),
                std::end(submitInfo.waitSemaphoreStages));
        }

        vkSubmitInfo.pWaitSemaphores = m_gfxQueueSubmitInfo.waitSemaphores.data();
        vkSubmitInfo.pWaitDstStageMask = m_gfxQueueSubmitInfo.waitSemaphoreStages.data();

        vkSubmitInfo.commandBufferCount = 1u + static_cast<uint32_t>(submitInfo.commandBuffers.size());
        if (vkSubmitInfo.commandBufferCount > m_gfxQueueSubmitInfo.commandBuffers.capacity()) {
            m_gfxQueueSubmitInfo.commandBuffers.reserve(vkSubmitInfo.commandBufferCount);
        }
        // See comment above about persistent vectors used for VkSubmitInfo
        m_gfxQueueSubmitInfo.commandBuffers.insert(
            std::end(m_gfxQueueSubmitInfo.commandBuffers),
            std::begin(submitInfo.commandBuffers),
            std::end(submitInfo.commandBuffers));

        vkSubmitInfo.pCommandBuffers = m_gfxQueueSubmitInfo.commandBuffers.data();

        vkSubmitInfo.signalSemaphoreCount = 1u + static_cast<uint32_t>(submitInfo.signalSemaphores.size());

        if (vkSubmitInfo.signalSemaphoreCount > m_gfxQueueSubmitInfo.signalSemaphores.capacity()) {
            m_gfxQueueSubmitInfo.signalSemaphores.reserve(vkSubmitInfo.signalSemaphoreCount);
        }

        m_gfxQueueSubmitInfo.signalSemaphores.push_back(m_renderFinishedSemaphores[m_syncObjIndex]->getHandle());

        if (!submitInfo.signalSemaphores.empty()) {
            m_gfxQueueSubmitInfo.signalSemaphores.insert(
                std::end(m_gfxQueueSubmitInfo.signalSemaphores),
                std::begin(submitInfo.signalSemaphores),
                std::end(submitInfo.signalSemaphores));
        }
        vkSubmitInfo.pSignalSemaphores = m_gfxQueueSubmitInfo.signalSemaphores.data();

        VkDevice device = m_pContext->getLogicalDevice();

        VkFence fences[] = { m_inFlightFences[m_syncObjIndex]->getHandle() };
        vkResetFences(device, 1, fences);

        vkQueueSubmit(
            m_pContext->getGraphicsQueue(0u).queue,
            1,
            &vkSubmitInfo,
            fences[0]);

        ++m_syncObjIndex;
        if (m_syncObjIndex == m_inFlightFences.size()) {
            m_syncObjIndex = 0u;
        }

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = m_gfxQueueSubmitInfo.signalSemaphores.data();

        VkSwapchainKHR swapChains[] = { m_spSwapChain->getHandle() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &swapChainImageIndex;

        presentInfo.pResults = nullptr; // Optional

        return vkQueuePresentKHR(m_pContext->getPresentQueue(0u).queue, &presentInfo);
    }
}
