#include "VulkanGraphicsDrawable.h"

#include "VulkanGraphicsPipeline.h"

void vgfx::Drawable::recordDrawCommands(
    size_t swapChainIndex,
    VkCommandBuffer commandBuffer)
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

