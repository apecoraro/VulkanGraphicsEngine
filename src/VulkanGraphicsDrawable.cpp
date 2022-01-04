#include "VulkanGraphicsDrawable.h"

#include "VulkanGraphicsPipeline.h"

void vgfx::Drawable::recordDrawCommands(
    VkCommandBuffer commandBuffer,
    size_t swapChainIndex)
{
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_material.getPipeline().getLayout(),
        0u, // Offset in descriptor array
        static_cast<uint32_t>(m_descriptorSets[swapChainIndex].size()),
        m_descriptorSets[swapChainIndex].data(),
        0u, // dynamic sets count
        nullptr); // dynamic sets ptr

    vkCmdPushConstants(
        commandBuffer,
        m_material.getPipeline().getLayout(),
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4),
        static_cast<void*>(&m_worldTransform));

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

