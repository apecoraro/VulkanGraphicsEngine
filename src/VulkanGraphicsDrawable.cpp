#include "VulkanGraphicsDrawable.h"

#include "VulkanGraphicsPipeline.h"
#include "VulkanGraphicsRenderer.h"
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

    // First set is the projection matrix
    DescriptorSetUpdater updater;
    updater.update(
        drawContext.context,
        {
            // Bind the projection matrix buffer to index 0 in descriptor set 0.
            {0, drawContext.sceneState.pCurrentCameraNode->getProjectionBuffer()}
        },
        pDescriptorSets->at(0)
    );

    // TODO setup other descriptor sets
}

void vgfx::Drawable::draw(Renderer::DrawContext& drawContext)
{
    configureDescriptorSets(drawContext, &m_descriptorSets);

    VkCommandBuffer commandBuffer = drawContext.commandBuffer;
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
        drawContext.sceneState.pCurrentCameraNode->getView()
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
