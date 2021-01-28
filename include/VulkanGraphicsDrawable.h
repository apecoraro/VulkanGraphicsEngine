#ifndef QUANTUM_GFX_DRAWABLE_H
#define QUANTUM_GFX_DRAWABLE_H

#include "VulkanGraphicsContext.h"

#include "VulkanGraphicsIndexBuffer.h"
#include "VulkanGraphicsMaterials.h"
#include "VulkanGraphicsVertexBuffer.h"

#include <cstdint>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{

    // Geometry of an single drawable object (vertices and indices).
    class Drawable
    {
    public:
        Drawable(
            Context& context,
            std::unique_ptr<VertexBuffer> spVertexBuffer,
            std::unique_ptr<IndexBuffer> spIndexBuffer,
            const Material& material)
            : m_spVertexBuffer(std::move(spVertexBuffer))
            , m_spIndexBuffer(std::move(spIndexBuffer))
            , m_material(material)
        {
        }

        void update();

        void recordDrawCommands(
            size_t swapChainIndex,
            VkPipelineLayout pipelineLayout,
            VkCommandBuffer commandBuffer);

    private:
        std::unique_ptr<VertexBuffer> m_spVertexBuffer;				// GPU versions
        std::unique_ptr<IndexBuffer> m_spIndexBuffer;
        const Material& m_material;
    };
}
#endif
