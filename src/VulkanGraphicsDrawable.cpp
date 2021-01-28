#include "VulkanGraphicsDrawable.h"

void vgfx::Drawable::update()
{
}

void vgfx::Drawable::recordDrawCommands(
    size_t swapChainIndex,
    VkPipelineLayout pipelineLayout,
    VkCommandBuffer commandBuffer)
{
    const auto& descriptorSetBinding =
        m_material.getDescriptorSetBinding(
            m_material.getDescriptorSetBindingCount() % swapChainIndex);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0, // Offset in descriptor array
        descriptorSetBinding.size(), // count of descriptor sets
        descriptorSetBinding.data(),
        0, // dynamic sets count
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

