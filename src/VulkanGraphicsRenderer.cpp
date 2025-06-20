#include "VulkanGraphicsRenderer.h"

#include "VulkanGraphicsDepthStencilBuffer.h"
#include "VulkanGraphicsDescriptorPoolBuilder.h"
#include "VulkanGraphicsSceneNode.h"

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <algorithm>
#include <stdexcept>

namespace vgfx
{
    Camera::Camera(
        Context& context,
        const VkViewport& viewport,
        size_t frameBufferingCount)
            : m_rasterizerConfig({
                VK_POLYGON_MODE_FILL,
                VK_CULL_MODE_BACK_BIT,
                VK_FRONT_FACE_COUNTER_CLOCKWISE})
            , m_viewport(viewport)
    {
        Buffer::Config cameraMatrixBufferCfg(sizeof(glm::mat4));
        // Need one buffer each for each frame in flight and one for the current frame
        for (uint32_t index = 0; index < (frameBufferingCount + 1); ++index) {
            m_cameraMatrixBuffers.emplace_back(
                std::make_unique<Buffer>(
                    context,
                    Buffer::Type::UniformBuffer,
                    cameraMatrixBufferCfg));
        }
    }

    void Camera::update()
    {
        auto& currentBuffer = *m_cameraMatrixBuffers[m_currentBufferIndex];
        currentBuffer.update(&m_proj, sizeof(glm::mat4));

        ++m_currentBufferIndex;
        if (m_currentBufferIndex == m_cameraMatrixBuffers.size()) {
            m_currentBufferIndex = 0u;
        }
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
            if (m_swapChainConfig.imageUsage.has_value()
                && !SurfaceUsageIsSupported(
                        device,
                        m_swapChainConfig.imageUsage.value(),
                        m_swapChainConfig.imageFormat.value().format)) {
                throw std::runtime_error("Default swap chain format does not support custom usage!");
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
            // configure the surface renderTargetFormat
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
                if (!SurfaceUsageIsSupported(
                        device,
                        m_swapChainConfig.imageUsage.value(),
                        m_swapChainConfig.imageFormat.value().format)) {
                    throw std::runtime_error("Default swap chain format does not support custom usage!");
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

    void WindowRenderer::init(
        Context& context,
        uint32_t frameBufferingCount)
    {
        SwapChain::Config swapChainConfig(
            m_swapChainConfig.imageCount.value(),
            frameBufferingCount,
            m_swapChainConfig.imageFormat.value(),
            m_swapChainConfig.imageExtent.value(),
            m_swapChainConfig.imageUsage.value(),
            m_swapChainConfig.compositeAlphaMode.value(),
            m_swapChainConfig.presentMode.value(),
            m_swapChainConfig.preTransform.value());

        m_spSwapChain->createRenderingResources(context, swapChainConfig);

        // Don't know how many images were created until after createRenderingResources completes, so have
        // to query to get this value even though we passed it in.
        size_t frameBufferingCount = m_spSwapChain->getImageAvailableSemaphoreCount();

        Renderer::init(context, frameBufferingCount);

        m_renderFinishedSemaphores.reserve(frameBufferingCount);
        for (size_t i = 0; i < frameBufferingCount; ++i) {
            m_renderFinishedSemaphores.push_back(
                std::make_unique<Semaphore>(context));

            m_inFlightFences.push_back(
                std::make_unique<Fence>(context));
        }

        if (m_swapChainConfig.pickDepthStencilFormat != nullptr) {
            const auto& swapChainExtent = m_spSwapChain->getImageExtent();
            createDepthStencilBuffer(
                context,
                swapChainExtent.width,
                swapChainExtent.height,
                m_swapChainConfig.pickDepthStencilFormat);
        }

        m_pContext = &context;
    }

    std::unique_ptr<Camera> WindowRenderer::createCamera(Context& context, uint32_t frameBufferingCount)
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
            .width = swapChainExtent.width,
            .height = swapChainExtent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        return std::make_unique<Camera>(context, cameraView, cameraProj, viewport, frameBufferingCount);
    }

    void WindowRenderer::beginDrawScene(VkCommandBuffer commandBuffer, size_t frameIndex)
    {
        bool imageBarrierNeeded =
            m_pContext->getPresentQueue(0).queue != m_pContext->getGraphicsQueue(0).queue;
        if (imageBarrierNeeded) {
            // This barrier needed to transfer ownership of the image from the present queue to the
            // graphics queue.
            VkImageMemoryBarrier barrierFromPresentToDraw = {};
            barrierFromPresentToDraw.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrierFromPresentToDraw.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barrierFromPresentToDraw.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrierFromPresentToDraw.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrierFromPresentToDraw.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierFromPresentToDraw.srcQueueFamilyIndex = m_pContext->getPresentQueueFamilyIndex().value();
            barrierFromPresentToDraw.dstQueueFamilyIndex = m_pContext->getGraphicsQueueFamilyIndex().value();

            size_t swapChainImageIndex = frameIndex % m_spSwapChain->getImageCount();
            VkImage swapChainImage = m_spSwapChain->getImage(swapChainImageIndex).getHandle();

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

    void WindowRenderer::endDrawScene(VkCommandBuffer commandBuffer, size_t frameIndex)
    {
        bool imageBarrierNeeded =
            m_pContext->getPresentQueue(0).queue != m_pContext->getGraphicsQueue(0).queue;
        if (imageBarrierNeeded) {
            VkImageMemoryBarrier barrierFromDrawToPresent = {};
            barrierFromDrawToPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrierFromDrawToPresent.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrierFromDrawToPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            barrierFromDrawToPresent.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierFromDrawToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrierFromDrawToPresent.srcQueueFamilyIndex = m_pContext->getGraphicsQueueFamilyIndex().value();
            barrierFromDrawToPresent.dstQueueFamilyIndex = m_pContext->getPresentQueueFamilyIndex().value();

            size_t swapChainImageIndex = frameIndex % m_spSwapChain->getImageCount();
            VkImage swapChainImage = m_spSwapChain->getImage(swapChainImageIndex).getHandle();

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

    VkResult WindowRenderer::presentFrame(QueueSubmitInfo& submitInfo)
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
        presentInfo.pWaitSemaphores = submitInfo.signalSemaphores.data();

        VkSwapchainKHR swapChains[] = { m_spSwapChain->getHandle() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &swapChainImageIndex;

        presentInfo.pResults = nullptr; // Optional

        return vkQueuePresentKHR(m_pContext->getPresentQueue(0u).queue, &presentInfo);
    }

    void Renderer::init(Context& context, uint32_t frameBufferingCount)
    {
        m_frameBufferingCount = frameBufferingCount;

        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addDescriptors(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100);
        poolBuilder.addDescriptors(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100);
        poolBuilder.addMaxSets(200);
        //poolBuilder.setCreateFlags(VkDescriptorPoolCreateFlags);

        m_spCommandBufferFactory =
            std::make_unique<CommandBufferFactory>(
                context, context.getGraphicsQueueFamilyIndex());

        Buffer::Config lightsBufferCfg(
            (sizeof(LightState) * 2) + sizeof(int) + sizeof(float) + sizeof(glm::vec3));
        for (size_t i = 0; i < frameBufferingCount; ++i) {
            m_descriptorPools.emplace_back(poolBuilder.createPool(context));

            m_commandBuffers.push_back(m_spCommandBufferFactory->createCommandBuffer());

            
            m_lightsBuffers.emplace_back(
                std::make_unique<Buffer>(
                    context,
                    Buffer::Type::UniformBuffer,
                    lightsBufferCfg));
        }

        m_spCamera = createCamera(context, frameBufferingCount);
    }

    void Renderer::drawScene(SceneNode& scene)
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

        beginDrawScene(commandBuffer, m_frameIndex);


        DrawContext drawState(*this->m_pContext, m_frameIndex, commandBuffer, descriptorPool);

        m_spCamera->update();
        drawState.pushView(
            m_spCamera->getView(),
            m_spCamera->getProj(),
            &m_spCamera->getProjectionBuffer(),
            m_spCamera->getViewport(),
            m_spCamera->getRasterizerConfig());

        scene.draw(drawState);

        drawState.popView();

        endDrawScene(commandBuffer, m_frameIndex);

        vkEndCommandBuffer(commandBuffer);

        m_gfxQueueSubmitInfo.clearAll();
        m_gfxQueueSubmitInfo.commandBuffers.push_back(commandBuffer);

        presentFrame(m_gfxQueueSubmitInfo);

        ++m_frameIndex;
    }

    void Renderer::createDepthStencilBuffer(
        Context& context,
        uint32_t width,
        uint32_t height,
        PickDepthStencilFormatFunc pickFormatFunc)
    {
        VkFormat candidates[] = {
            VK_FORMAT_D16_UNORM,
            VK_FORMAT_X8_D24_UNORM_PACK32,
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_S8_UINT,
            VK_FORMAT_D16_UNORM_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
        };
        std::set<VkFormat> supportedDepthStencilFormats;
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(context.getPhysicalDevice(), format, &props);

            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                supportedDepthStencilFormats.insert(format);
            }
        }

        VkFormat selectedFormat = pickFormatFunc(supportedDepthStencilFormats);

        DepthStencilBuffer::Config dsCfg(width, height, selectedFormat);
        m_spDepthStencilBuffer.reset(
            new DepthStencilBuffer(context, dsCfg));
    }

    void Renderer::DepthStencilDeleter::operator()(DepthStencilBuffer * pDSBuffer)
    {
        if (pDSBuffer != nullptr) {
            delete pDSBuffer;
        }
    }
}
