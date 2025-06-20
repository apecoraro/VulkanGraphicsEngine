#include "VulkanGraphicsDrawable.h"

#include "VulkanGraphicsImage.h"
#include "VulkanGraphicsImageDescriptorUpdaters.h"
#include "VulkanGraphicsPipeline.h"
#include "VulkanGraphicsRenderer.h"
#include "VulkanGraphicsSampler.h"
#include "VulkanGraphicsSceneNode.h"

//void vgfx::Renderer::updateCameraDescriptorSet(DescriptorSet& cameraDescriptorSet)
//{
    // TODO
//}

void vgfx::Drawable::configureDescriptorSets(
    Renderer::DrawContext& drawContext,
    std::vector<VkDescriptorSet>* pDescriptorSets)
{
    pDescriptorSets->clear();
    pDescriptorSets->resize(m_material.getDescriptorSetLayouts().size());
    for (size_t index = 0; index < m_material.getDescriptorSetLayouts().size(); ++index) {
        const auto& spDescSetLayout = m_material.getDescriptorSetLayouts()[index];
        
        VkDescriptorSet* pWritePtr = pDescriptorSets->data() + index;
        drawContext.descriptorPool.allocateDescriptorSets(
            *spDescSetLayout.get(), 1, &pWritePtr);
    }

    Renderer::Camera& curCamera = drawContext.sceneState.views.back();
    // First set is the projection matrix
    DescriptorSetUpdater updater;
    updater.bindDescriptor(0, *curCamera.pCameraProjectionBuffer);
    updater.updateDescriptorSet(drawContext.context, pDescriptorSets->at(0));

    auto& imageSampler = getImageSampler(Material::ImageType::Diffuse);

    CombinedImageSamplerDescriptorUpdater imageSamplerUpdater(*imageSampler.first, *imageSampler.second);

    // Second set is the texture sampler
    updater.bindDescriptor(0, imageSamplerUpdater);
    updater.updateDescriptorSet(drawContext.context, pDescriptorSets->at(1));

    int32_t lightCount = 1;
    Renderer::LightState* pLights = &drawContext.sceneState.lights.back();
    if (drawContext.sceneState.lights.size() > 1) {
        pLights = &drawContext.sceneState.lights[drawContext.sceneState.lights.size() - 2];
        lightCount = 2;
    }
    size_t writeSize = sizeof(Renderer::LightState) * lightCount;
    drawContext.sceneState.pLightsBuffer->update(pLights, writeSize);

    size_t writeOffset = writeSize;
    writeSize = sizeof(lightCount);
    drawContext.sceneState.pLightsBuffer->update(&lightCount, writeSize, writeOffset);

    writeOffset += writeSize;

    float ambient = 0.2f;
    writeSize = sizeof(ambient);

    drawContext.sceneState.pLightsBuffer->update(&ambient, writeSize, writeOffset);

    writeOffset += writeSize;

    auto& translationColumn = curCamera.cameraViewMatrix[3];
    glm::vec3 viewPos(
        -translationColumn.x,
        -translationColumn.y,
        -translationColumn.z);

    writeSize = sizeof(viewPos);
    drawContext.sceneState.pLightsBuffer->update(&viewPos, writeSize, writeOffset);

    updater.bindDescriptor(1, *drawContext.sceneState.pLightsBuffer);
    updater.updateDescriptorSet(drawContext.context, pDescriptorSets->at(1));
}

void vgfx::Drawable::draw(Renderer::DrawContext& drawContext)
{
    // TODO sort Drawables by pipeline and only bind once for each unique
    VkCommandBuffer commandBuffer = drawContext.commandBuffer;
    vkCmdBindPipeline(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_material.getPipeline().getHandle());

    configureDescriptorSets(drawContext, &m_descriptorSets);

    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_material.getPipeline().getLayout(),
        0u, // Offset in descriptor array
        static_cast<uint32_t>(m_descriptorSets.size()),
        m_descriptorSets.data(),
        0u, // dynamic sets count
        nullptr); // dynamic sets ptr

    glm::mat4 pushConstants[2] = {
        m_worldTransform,
        drawContext.sceneState.views.back().cameraViewMatrix
    };

    vkCmdPushConstants(
        commandBuffer,
        m_material.getPipeline().getLayout(),
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(pushConstants),
        static_cast<void*>(pushConstants));

    VkBuffer vertexBuffers[] = { m_spVertexBuffer->getHandle() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(
        commandBuffer,
        0, // first binding
        1, // count
        vertexBuffers,
        offsets);

    vkCmdBindIndexBuffer(
        commandBuffer,
        m_spIndexBuffer->getHandle(),
        0, // Offset
        m_spIndexBuffer->getType());

    vkCmdDrawIndexed(
        commandBuffer,
        m_spIndexBuffer->getCount(),
        1, // instance count
        0, // first index
        0, // vertex offset
        0); // first instance
}
