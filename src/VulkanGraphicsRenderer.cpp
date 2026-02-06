#include "VulkanGraphicsRenderer.h"

#include "VulkanGraphicsCamera.h"
#include "VulkanGraphicsDepthStencilBuffer.h"
#include "VulkanGraphicsDescriptorPoolBuilder.h"
#include "VulkanGraphicsDrawable.h"
#include "VulkanGraphicsEffects.h"
#include "VulkanGraphicsRenderTarget.h"
#include "VulkanGraphicsSampler.h"
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
    void Renderer::createImageSamplers(Drawable& drawable)
    {
        ImageSampler& imageSampler = drawable.getImageSampler(ImageType::Diffuse);

        const Image& image = imageSampler.first->getImage();
        uint32_t mipLevels =
            vgfx::Image::ComputeMipLevels2D(image.getWidth(), image.getHeight());

        Sampler& sampler = EffectsLibrary::GetOrCreateSampler(
            m_context,
            Sampler::Config(
                VK_FILTER_LINEAR,
                VK_FILTER_LINEAR,
                VK_SAMPLER_MIPMAP_MODE_LINEAR,
                0.0f, // min lod
                static_cast<float>(mipLevels),
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                false, 0));
        imageSampler.second = &sampler;
    }

    void Renderer::buildPipelines(Object& object, DrawContext& drawContext)
    {
        const auto& viewState = drawContext.sceneState.views.back();
        vgfx::PipelineBuilder builder(viewState.viewport, drawContext.depthBufferEnabled);

        //std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        std::string vertexShader = "MvpTransform_XyzRgbUvNormal_Out.vert.spv";
        std::string fragmentShader = "TexturedBlinnPhong.frag.spv";

        std::string vertexShaderEntryPointFunc = "main";
        std::string fragmentShaderEntryPointFunc = "main";

        EffectsLibrary::MeshEffectDesc meshEffectDesc(
            vertexShader,
            vertexShaderEntryPointFunc,
            fragmentShader,
            fragmentShaderEntryPointFunc);

        MeshEffect& meshEffect =
            EffectsLibrary::GetOrLoadEffect(m_context, meshEffectDesc);

        for (auto& pDrawable : object.getDrawables()) {

            Pipeline::InputAssemblyConfig inputConfig(
                pDrawable->getVertexBuffer().getConfig().primitiveTopology,
                pDrawable->getIndexBuffer().getHasPrimitiveRestartValues());

            std::unique_ptr<Pipeline> spPipeline =
                builder.configureDrawableInput(meshEffect, pDrawable->getVertexBuffer().getConfig(), inputConfig)
                .configureRasterizer(viewState.rasterizerConfig)
                //.configureDynamicStates(dynamicStates)
                .configureRenderTarget(drawContext.renderTarget)
                .createPipeline(drawContext.context);

            addPipeline(std::move(spPipeline));

            pDrawable->setMeshEffect(&meshEffect);

            createImageSamplers(*pDrawable);
        }
    }

    void Renderer::renderFrame(SceneNode& scene)
    {
        size_t cpuFrameInFlight = m_frameIndex % m_descriptorPools.size();

        VkCommandBuffer commandBuffer = m_commandBuffers[cpuFrameInFlight];
        vkResetCommandBuffer(commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0u;
        beginInfo.pInheritanceInfo = nullptr; // Optional
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        m_queueSubmitInfo.clearAll();

        const RenderTarget& renderTarget = prepareRenderTarget(commandBuffer);

        m_context.beginRendering(commandBuffer, renderTarget);

        DescriptorPool& descriptorPool = *m_descriptorPools[cpuFrameInFlight].get();
        descriptorPool.reset();

        DrawContext drawState{
            .context = m_context,
            .descriptorPool = descriptorPool,
            .frameIndex = m_frameIndex,
            .depthBufferEnabled = true,
            .commandBuffer = commandBuffer,
            .renderTarget = renderTarget
        };

        m_spCamera->update();
        drawState.pushView(
            m_spCamera->getView(),
            m_spCamera->getProj(),
            &m_spCamera->getProjectionBuffer(),
            m_spCamera->getViewport(),
            m_spCamera->getRasterizerConfig());

        drawState.sceneState.pLightsBuffer = m_lightsBuffers[cpuFrameInFlight].get();

        scene.draw(*this, drawState);

        postDrawScene(commandBuffer);

        m_context.endRendering(commandBuffer);

        vkEndCommandBuffer(commandBuffer);

        addCommandBufferToSubmitInfo(commandBuffer);
    }

    void Renderer::submitGraphicsCommands()
    {
        QueueSubmitInfo& submitInfo = m_queueSubmitInfo;

        VkSubmitInfo vkSubmitInfo = {};
        vkSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        vkSubmitInfo.waitSemaphoreCount = static_cast<uint32_t>(submitInfo.waitSemaphores.size());
        vkSubmitInfo.pWaitSemaphores = submitInfo.waitSemaphores.data();
        vkSubmitInfo.pWaitDstStageMask = submitInfo.waitSemaphoreStages.data();

        vkSubmitInfo.commandBufferCount = static_cast<uint32_t>(submitInfo.commandBuffers.size());
        vkSubmitInfo.pCommandBuffers = submitInfo.commandBuffers.data();

        vkSubmitInfo.signalSemaphoreCount = static_cast<uint32_t>(submitInfo.signalSemaphores.size());
        vkSubmitInfo.pSignalSemaphores = submitInfo.signalSemaphores.data();

        vkQueueSubmit(
            m_context.getGraphicsQueue(0u).queue,
            1,
            &vkSubmitInfo,
            submitInfo.fence);
    }

    VkResult SwapChainPresenter::present(Renderer& renderer)
    {
        VkDevice device = m_logicalDevice;

        VkFence fences[] = { m_inFlightFences[m_syncObjIndex]->getHandle() };
        vkResetFences(device, 1, fences);

        auto& submitInfo = renderer.getSubmitInfo();
        submitInfo.fence = fences[0];
        submitInfo.addWait(m_spSwapChain->getImageAvailableSemaphore(m_syncObjIndex), VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        submitInfo.signalSemaphores.push_back(m_renderFinishedSemaphores[m_syncObjIndex]->getHandle());

        renderer.submitGraphicsCommands();

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
        uint32_t imageIndices[] = { static_cast<uint32_t>(m_curSwapChainImageIndex) };
        presentInfo.pImageIndices = imageIndices;

        presentInfo.pResults = nullptr; // Optional

        return vkQueuePresentKHR(renderer.getContext().getPresentQueue(0u).queue, &presentInfo);
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

    void SwapChainPresenter::checkIsDeviceSuitable(VkPhysicalDevice device) const
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

    bool SwapChainPresenter::queueFamilySupportsPresent(VkPhysicalDevice device, uint32_t familyIndex) const
    {
        return SwapChain::SurfaceSupportsQueueFamily(device, m_surface, familyIndex);
    }

    void SwapChainPresenter::configureForDevice(VkPhysicalDevice physicalDevice)
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

    void SwapChainPresenter::initSwapChain(Renderer& renderer)
    {
        m_logicalDevice = renderer.getContext().getLogicalDevice();

        m_spSwapChain =
            std::make_unique<SwapChain>(renderer.getContext(), m_surface, m_swapChainConfig);

        // Don't know how many images were created until after creation, so have
        // to query to get this value even though we passed it in.
        uint32_t frameBufferingCount = m_spSwapChain->getImageCount();

        const auto& swapChainExtent = m_spSwapChain->getImageExtent();

        renderer.initGraphicsResources(swapChainExtent.width, swapChainExtent.height, frameBufferingCount);

        m_renderFinishedSemaphores.reserve(frameBufferingCount);
        m_swapChainRenderTargets.resize(frameBufferingCount);
        for (size_t i = 0; i < frameBufferingCount; ++i) {

            m_renderFinishedSemaphores.push_back(
                std::make_unique<Semaphore>(renderer.getContext()));

            m_inFlightFences.push_back(
                std::make_unique<Fence>(renderer.getContext()));

            m_swapChainRenderTargets[i].addRenderImage(m_spSwapChain->getImage(i));
        }

        if (m_swapChainConfig.depthStencilFormat.has_value()) {
            DepthStencilBuffer::Config dsCfg(
                swapChainExtent.width,
                swapChainExtent.height,
                m_swapChainConfig.depthStencilFormat.value());

            renderer.setDepthStencilBuffer(std::make_unique<DepthStencilBuffer>(renderer.getContext(), dsCfg));

            for (size_t i = 0; i < frameBufferingCount; ++i) {
                m_swapChainRenderTargets[i].attachDepthStencilBuffer(*renderer.getDepthStencilBuffer());
            }
        }
    }

    void Renderer::createCamera(uint32_t width, uint32_t height, uint32_t framesInFlightPlusOne)
    {
        glm::vec3 viewPos(2.0f, 2.0f, 2.0f);
        glm::mat4 cameraView = glm::lookAt(
            viewPos,
            glm::vec3(0.0f, 0.0f, 0.0f), // center
            glm::vec3(0.0f, 0.0f, 1.0f)); // up

        // Vulkan NDC is different than OpenGL, so use this clip matrix to correct for that.
        const glm::mat4 clip(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            0.0f, 0.0f, 0.5f, 1.0f);
        glm::mat4 cameraProj =
            clip * glm::perspective(
                glm::radians(45.0f),
                static_cast<float>(width) / static_cast<float>(height),
                0.1f, // near
                30.0f); // far
        VkViewport viewport {
            .x = 0,
            .y = 0,
            .width = static_cast<float>(width),
            .height = static_cast<float>(height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        m_spCamera = std::make_unique<Camera>(
            m_context, framesInFlightPlusOne, cameraView, cameraProj, viewport);
    }

    static void RecordImageLayoutTransition(
        VkCommandBuffer cmd,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask,
        VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstAccessMask = dstAccessMask;

        vkCmdPipelineBarrier(
            cmd,
            srcStageMask,
            dstStageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    }

    const RenderTarget& SwapChainPresenter::acquireSwapChainImageForRendering(VkCommandBuffer commandBuffer)
    {
        if (acquireNextSwapChainImage(&m_curSwapChainImageIndex) == VK_ERROR_OUT_OF_DATE_KHR) {
            throw VK_ERROR_OUT_OF_DATE_KHR;
        }

        VkImage swapChainImage = getSwapChainImage(m_curSwapChainImageIndex).getHandle();

        RecordImageLayoutTransition(
            commandBuffer,
            swapChainImage,
            VK_IMAGE_LAYOUT_UNDEFINED,                    // or PRESENT_SRC_KHR on subsequent frames
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);

        return m_swapChainRenderTargets[m_curSwapChainImageIndex];
    }

    void SwapChainPresenter::transitionSwapChainImageForPresent(VkCommandBuffer commandBuffer)
    {
        VkImage swapChainImage = getSwapChainImage(m_curSwapChainImageIndex).getHandle();

        RecordImageLayoutTransition(
            commandBuffer,
            swapChainImage,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            0);
    }

    VkResult SwapChainPresenter::acquireNextSwapChainImage(uint32_t* pSwapChainImageIndex)
    {
        VkDevice device = m_logicalDevice;
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

    void SwapChainPresenter::resizeWindow(uint32_t width, uint32_t height, Renderer& renderer)
    {
        renderer.getContext().waitForDeviceToIdle();

        VkExtent2D windowWidthHeight {
            .width = width,
            .height = height
        };
        m_swapChainConfig.imageExtent = windowWidthHeight;
        m_spSwapChain->recreate(m_surface, m_swapChainConfig);

        renderer.resizeRenderTargetResources(width, height, m_spSwapChain->getImageCount());
    }

    void Renderer::resizeRenderTargetResources(uint32_t width, uint32_t height, uint32_t frameBufferingCount)
    {
        uint32_t framesInFlightPlusOne = frameBufferingCount + 1;
        if (frameBufferingCount != m_frameBufferingCount) {
            m_frameBufferingCount = frameBufferingCount;

            m_descriptorPools.clear();
            m_commandBuffers.clear();
            m_lightsBuffers.clear();

            createResourcePools(framesInFlightPlusOne);
        }

        createCamera(width, height, framesInFlightPlusOne);
    }

    void Renderer::createResourcePools(uint32_t framesInFlightPlusOne)
    {
        DescriptorPoolBuilder poolBuilder;
        poolBuilder.addDescriptors(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 100);
        poolBuilder.addDescriptors(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 100);
        poolBuilder.addMaxSets(200);
        //poolBuilder.setCreateFlags(VkDescriptorPoolCreateFlags);

        struct LightingUniforms
        {
            LightState lights[2];
            int lightCount;
            float ambient;
            glm::vec3 viewPos;
        };

        Buffer::Config lightsBufferCfg(sizeof(LightingUniforms) * 10); // Support up to 10 lights
        for (size_t i = 0; i < framesInFlightPlusOne; ++i) {
            m_descriptorPools.emplace_back(poolBuilder.createPool(m_context));

            m_commandBuffers.push_back(m_spCommandBufferFactory->createCommandBuffer());

            m_lightsBuffers.emplace_back(
                std::make_unique<Buffer>(
                    m_context,
                    Buffer::Type::UniformBuffer,
                    lightsBufferCfg));
        }
    }

    void Renderer::initGraphicsResources(uint32_t renderTargetWidth, uint32_t renderTargetHeight, uint32_t frameBufferingCount)
    {
        m_frameBufferingCount = frameBufferingCount;

        m_spCommandBufferFactory =
            std::make_unique<CommandBufferFactory>(
                m_context, m_context.getGraphicsQueueFamilyIndex());

        uint32_t framesInFlightPlusOne = frameBufferingCount + 1;
        createResourcePools(framesInFlightPlusOne);

        createCamera(renderTargetWidth, renderTargetHeight, static_cast<uint32_t>(framesInFlightPlusOne));
    }
}
