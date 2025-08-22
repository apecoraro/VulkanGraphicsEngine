#include "VulkanGraphicsRenderer.h"

#include "VulkanGraphicsCamera.h"
#include "VulkanGraphicsDepthStencilBuffer.h"
#include "VulkanGraphicsDescriptorPoolBuilder.h"
#include "VulkanGraphicsSceneNode.h"

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <algorithm>
#include <array>
#include <stdexcept>

#include <iostream>

namespace vgfx
{
    VkResult Renderer::renderFrame(SceneNode& scene)
    {
        size_t frameInFlight = m_frameIndex % m_frameBufferingCount;
        DescriptorPool& descriptorPool = *m_descriptorPools[frameInFlight].get();
        descriptorPool.reset();

        VkCommandBuffer commandBuffer = m_commandBuffers[frameInFlight];
        vkResetCommandBuffer(commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0u;
        beginInfo.pInheritanceInfo = nullptr; // Optional
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        preDrawScene(commandBuffer, m_frameIndex);

        VkRenderingAttachmentInfoKHR colorAttachmentInfo = {};

        ImageView& renderTargetView = *getRenderTarget(m_frameIndex).getDefaultImageView();
        colorAttachmentInfo.imageView = renderTargetView.getHandle();

        colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
        colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkClearValue clearColor;
        clearColor.color = { {0.0f, 0.0f, 0.0f, 1.0f} };

        colorAttachmentInfo.clearValue = clearColor;

        VkRenderingAttachmentInfoKHR depthAttachment = {};
        depthAttachment.imageView = getDepthStencilBuffer()->getDefaultImageView().getHandle();
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depthAttachment.resolveMode = VK_RESOLVE_MODE_NONE;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        VkClearValue clearDepth;
        clearDepth.depthStencil = { 0.0f, ~0U };
        depthAttachment.clearValue = clearDepth;

        auto renderArea = VkRect2D{ VkOffset2D{}, VkExtent2D {
            renderTargetView.getImage().getWidth(), renderTargetView.getImage().getHeight()}
        };

        VkRenderingInfoKHR renderingInfo = {};
        renderingInfo.renderArea = renderArea;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachmentInfo;
        renderingInfo.layerCount = 1;
        renderingInfo.pDepthAttachment = &depthAttachment;

        if (getDepthStencilBuffer()->hasStencilBuffer()) {
            renderingInfo.pStencilAttachment = &depthAttachment;
        }

        std::cerr << "beginRendering" << std::endl;
        m_context.beginRendering(commandBuffer, &renderingInfo);

        std::cerr << "setupDrawState" << std::endl;
        DrawContext drawState {
            .context = m_context,
            .descriptorPool = descriptorPool,
            .frameIndex = m_frameIndex,
            .depthBufferEnabled = true,
            .commandBuffer = commandBuffer
        };

        m_spCamera->update();
        drawState.pushView(
            m_spCamera->getView(),
            m_spCamera->getProj(),
            &m_spCamera->getProjectionBuffer(),
            m_spCamera->getViewport(),
            m_spCamera->getRasterizerConfig());

        drawState.sceneState.pLightsBuffer = m_lightsBuffers[frameInFlight].get();

        std::cerr << "draw" << std::endl;
        scene.draw(drawState);

        drawState.popView();

        std::cerr << "endRendering" << std::endl;
        m_context.endRendering(commandBuffer);

        postDrawScene(commandBuffer, m_frameIndex);

        vkEndCommandBuffer(commandBuffer);

        m_gfxQueueSubmitInfo.clearAll();
        m_gfxQueueSubmitInfo.commandBuffers.push_back(commandBuffer);

        std::cerr << "endRenderFrame" << std::endl;
        VkResult retValue = endRenderFrame(m_gfxQueueSubmitInfo);

        ++m_frameIndex;

        return retValue;
    }

    VkResult WindowRenderer::endRenderFrame(QueueSubmitInfo& submitInfo)
    {
        uint32_t swapChainImageIndex = 0u;
        if (acquireNextSwapChainImage(&swapChainImageIndex) == VK_ERROR_OUT_OF_DATE_KHR) {
            return VK_ERROR_OUT_OF_DATE_KHR;
        }

        VkSubmitInfo vkSubmitInfo = {};
        vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        submitInfo.addWait(m_spSwapChain->getImageAvailableSemaphore(m_syncObjIndex), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

        vkSubmitInfo.waitSemaphoreCount = static_cast<uint32_t>(submitInfo.waitSemaphores.size());
        vkSubmitInfo.pWaitSemaphores = submitInfo.waitSemaphores.data();
        vkSubmitInfo.pWaitDstStageMask = submitInfo.waitSemaphoreStages.data();

        vkSubmitInfo.commandBufferCount = static_cast<uint32_t>(submitInfo.commandBuffers.size());
        vkSubmitInfo.pCommandBuffers = submitInfo.commandBuffers.data();

        submitInfo.signalSemaphores.push_back(m_renderFinishedSemaphores[m_syncObjIndex]->getHandle());
        vkSubmitInfo.signalSemaphoreCount = static_cast<uint32_t>(submitInfo.signalSemaphores.size());
        vkSubmitInfo.pSignalSemaphores = submitInfo.signalSemaphores.data();

        VkDevice device = m_context.getLogicalDevice();

        VkFence fences[] = { m_inFlightFences[m_syncObjIndex]->getHandle() };
        vkResetFences(device, 1, fences);

        vkQueueSubmit(
            m_context.getGraphicsQueue(0u).queue,
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
        presentInfo.pWaitSemaphores = submitInfo.signalSemaphores.data();

        VkSwapchainKHR swapChains[] = { m_spSwapChain->getHandle() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &swapChainImageIndex;

        presentInfo.pResults = nullptr; // Optional

        return vkQueuePresentKHR(m_context.getPresentQueue(0u).queue, &presentInfo);
    }

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

    bool SurfaceUsageIsSupported(
        VkPhysicalDevice device,
        VkImageUsageFlags usage,
        VkFormat format)
    {
        VkImageFormatProperties props = {};
        vkGetPhysicalDeviceImageFormatProperties(device, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &props);

        return props.maxResourceSize > 0u;
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
        SwapChain::GetImageCapabilities(
            device,
            m_surface,
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
            if ((m_swapChainConfig.preTransform.value() & supportedTransforms) == 0) {
                throw std::runtime_error("Swap chain desired transform is not supported.");
            }
        }

        if (m_swapChainConfig.compositeAlphaMode.has_value()) {
            if ((m_swapChainConfig.compositeAlphaMode.value() & supportedCompositeAlphaModes) == 0) {
                throw std::runtime_error("Swap chain desired composite alpha mode is not supported.");
            }
        }

        if (m_swapChainConfig.imageUsage.has_value()) {
            if ((m_swapChainConfig.imageUsage.value() & supportedImageUsageModes) == 0) {
                throw std::runtime_error("Swap chain desired desired usage mode is not supported.");
            }
        }

        std::vector<VkSurfaceFormatKHR> supportedFormats;
        SwapChain::GetSupportedImageFormats(device, m_surface, &supportedFormats);

        if (supportedFormats.empty()) {
            throw std::runtime_error("Swap chain does not support any image formats.");
        } else if (m_swapChainConfig.imageFormat.has_value()) {
            if(!SurfaceFormatIsSupported(supportedFormats, m_swapChainConfig.imageFormat.value())) {
                throw std::runtime_error("Swap chain does not support specified image format.");
            }
            if (m_swapChainConfig.imageUsage.has_value()
                && !SurfaceUsageIsSupported(
                        device,
                        m_swapChainConfig.imageUsage.value(),
                        m_swapChainConfig.imageFormat.value().format)) {
                throw std::runtime_error("Default swap chain format does not support custom usage!");
            }
        }
        
        std::vector<VkPresentModeKHR> supportedModes;
        SwapChain::GetSupportedPresentationModes(device, m_surface, &supportedModes);

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
        return SwapChain::SurfaceSupportsQueueFamily(device, m_surface, familyIndex);
    }

    void WindowRenderer::configureForDevice(VkPhysicalDevice physicalDevice)
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
            SwapChain::GetImageCapabilities(
                physicalDevice,
                m_surface,
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
                    curImageExtent.width = 800;
                    curImageExtent.height = 600;
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
            // configure the surface renderTargetFormat
            std::vector<VkSurfaceFormatKHR> supportedFormats;
            SwapChain::GetSupportedImageFormats(physicalDevice, m_surface, &supportedFormats);

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
                if (!SurfaceUsageIsSupported(
                        physicalDevice,
                        m_swapChainConfig.imageUsage.value(),
                        m_swapChainConfig.imageFormat.value().format)) {
                    throw std::runtime_error("Default swap chain format does not support custom usage!");
                }
            }
        }

        if (!m_swapChainConfig.presentMode.has_value()) {
            // configure presentation mode
            std::vector<VkPresentModeKHR> supportedModes;
            SwapChain::GetSupportedPresentationModes(physicalDevice, m_surface, &supportedModes);

            if (m_choosePresentModeFunc != nullptr) {
                m_swapChainConfig.presentMode = m_choosePresentModeFunc(supportedModes);
            } else {
                m_swapChainConfig.presentMode = VK_PRESENT_MODE_FIFO_KHR;
            }
        }

        if (!m_swapChainConfig.depthStencilFormat.has_value() && m_swapChainConfig.preferredDepthStencilFormats.has_value()) {
            const auto& pickDepthStencilFormatFunc = [&](const std::set<VkFormat>& formats) {
                for (const auto& preferredDepthStencilFormat : m_swapChainConfig.preferredDepthStencilFormats.value()) {
                    if (formats.find(preferredDepthStencilFormat) != formats.end()) {
                        return preferredDepthStencilFormat;
                    }
                }
                throw std::runtime_error("Failed to find suitable depth stencil format!");
                return VK_FORMAT_MAX_ENUM;
            };
            m_swapChainConfig.depthStencilFormat = DepthStencilBuffer::PickFormat(physicalDevice, pickDepthStencilFormatFunc);
        }
    }

    void WindowRenderer::initSwapChain()
    {
        m_spSwapChain =
            std::make_unique<SwapChain>(m_context, m_surface, m_swapChainConfig);

        // Don't know how many images were created until after creation, so have
        // to query to get this value even though we passed it in.
        uint32_t frameBufferingCount = m_spSwapChain->getImageCount();

        Renderer::initGraphicsResources(frameBufferingCount);

        m_renderFinishedSemaphores.reserve(frameBufferingCount);
        for (size_t i = 0; i < frameBufferingCount; ++i) {
            m_renderFinishedSemaphores.push_back(
                std::make_unique<Semaphore>(m_context));

            m_inFlightFences.push_back(
                std::make_unique<Fence>(m_context));
        }

        if (m_swapChainConfig.depthStencilFormat.has_value()) {
            const auto& swapChainExtent = m_spSwapChain->getImageExtent();
            DepthStencilBuffer::Config dsCfg(
                swapChainExtent.width,
                swapChainExtent.height,
                m_swapChainConfig.depthStencilFormat.value());
            m_spDepthStencilBuffer.reset(
                new DepthStencilBuffer(m_context, dsCfg));
        }
    }

    std::unique_ptr<Camera> WindowRenderer::createCamera(uint32_t frameBufferingCount)
    {
        glm::vec3 viewPos(2.0f, 2.0f, 2.0f);
        glm::mat4 cameraView = glm::lookAt(
            viewPos,
            glm::vec3(0.0f, 0.0f, 0.0f), // center
            glm::vec3(0.0f, 0.0f, 1.0f)); // up

        auto& swapChainExtent = m_spSwapChain->getImageExtent();
        // Vulkan NDC is different than OpenGL, so use this clip matrix to correct for that.
        const glm::mat4 clip(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            0.0f, 0.0f, 0.5f, 1.0f);
        glm::mat4 cameraProj =
            clip * glm::perspective(
                glm::radians(45.0f),
                static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
                0.1f, // near
                30.0f); // far
        VkViewport viewport {
            .x = 0,
            .y = 0,
            .width = static_cast<float>(swapChainExtent.width),
            .height = static_cast<float>(swapChainExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        return std::make_unique<Camera>(
            m_context, frameBufferingCount, cameraView, cameraProj, viewport);
    }

    void WindowRenderer::preDrawScene(VkCommandBuffer commandBuffer, size_t frameIndex)
    {
        bool imageBarrierNeeded =
            m_context.getPresentQueue(0).queue != m_context.getGraphicsQueue(0).queue;
        if (imageBarrierNeeded) {
            // This barrier needed to transfer ownership of the image from the present queue to the
            // graphics queue.
            VkImageMemoryBarrier barrierFromPresentToDraw = {};
            barrierFromPresentToDraw.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrierFromPresentToDraw.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barrierFromPresentToDraw.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrierFromPresentToDraw.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierFromPresentToDraw.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierFromPresentToDraw.srcQueueFamilyIndex = m_context.getPresentQueueFamilyIndex();
            barrierFromPresentToDraw.dstQueueFamilyIndex = m_context.getGraphicsQueueFamilyIndex();

            VkImage swapChainImage = getRenderTarget(frameIndex).getHandle();

            barrierFromPresentToDraw.image = swapChainImage;

            VkImageSubresourceRange imageSubresourceRange = {};
            imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageSubresourceRange.baseMipLevel = 0u;
            imageSubresourceRange.levelCount = 1u;
            imageSubresourceRange.baseArrayLayer = 0u;
            imageSubresourceRange.layerCount = 1u;
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
    }

    void WindowRenderer::postDrawScene(VkCommandBuffer commandBuffer, size_t frameIndex)
    {
        bool imageBarrierNeeded =
            m_context.getPresentQueue(0).queue != m_context.getGraphicsQueue(0).queue;
        if (imageBarrierNeeded) {
            VkImageMemoryBarrier barrierFromDrawToPresent = {};
            barrierFromDrawToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrierFromDrawToPresent.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrierFromDrawToPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barrierFromDrawToPresent.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierFromDrawToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierFromDrawToPresent.srcQueueFamilyIndex = m_context.getGraphicsQueueFamilyIndex();
            barrierFromDrawToPresent.dstQueueFamilyIndex = m_context.getPresentQueueFamilyIndex();

            VkImage swapChainImage = getRenderTarget(frameIndex).getHandle();

            barrierFromDrawToPresent.image = swapChainImage;

            VkImageSubresourceRange imageSubresourceRange = {};
            imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageSubresourceRange.baseMipLevel = 0u;
            imageSubresourceRange.levelCount = 1u;
            imageSubresourceRange.baseArrayLayer = 0u;
            imageSubresourceRange.layerCount = 1u;
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

    }

    VkResult WindowRenderer::acquireNextSwapChainImage(uint32_t* pSwapChainImageIndex)
    {
        VkDevice device = m_context.getLogicalDevice();
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

    void WindowRenderer::resizeWindow(uint32_t width, uint32_t height)
    {
        m_context.waitForDeviceToIdle();

        VkExtent2D windowWidthHeight {
            .width = width,
            .height = height
        };
        m_swapChainConfig.imageExtent = windowWidthHeight;
        m_spSwapChain->recreate(m_surface, m_swapChainConfig);
        m_spCamera = createCamera(static_cast<uint32_t>(m_spSwapChain->getImageCount()));
    }

    void Renderer::initGraphicsResources(uint32_t frameBufferingCount)
    {
        m_frameBufferingCount = frameBufferingCount;

        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addDescriptors(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100);
        poolBuilder.addDescriptors(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100);
        poolBuilder.addMaxSets(200);
        //poolBuilder.setCreateFlags(VkDescriptorPoolCreateFlags);

        m_spCommandBufferFactory =
            std::make_unique<CommandBufferFactory>(
                m_context, m_context.getGraphicsQueueFamilyIndex());

        struct LightingUniforms
        {
            LightState lights[2];
            int lightCount;
            float ambient;
            glm::vec3 viewPos;
        };

        Buffer::Config lightsBufferCfg(sizeof(LightingUniforms) * 10);
        for (size_t i = 0; i < frameBufferingCount; ++i) {
            m_descriptorPools.emplace_back(poolBuilder.createPool(m_context));

            m_commandBuffers.push_back(m_spCommandBufferFactory->createCommandBuffer());

            m_lightsBuffers.emplace_back(
                std::make_unique<Buffer>(
                    m_context,
                    Buffer::Type::UniformBuffer,
                    lightsBufferCfg));
        }

        m_spCamera = createCamera(frameBufferingCount);
    }
}
