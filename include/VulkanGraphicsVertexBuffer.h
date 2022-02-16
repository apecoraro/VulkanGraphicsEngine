#pragma once

#include "VulkanGraphicsContext.h"

#include "VulkanGraphicsCommandBufferFactory.h"

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

namespace vgfx
{
    class VertexBuffer
    {
    public:
        struct AttributeDescription
        {
            VkFormat format = VK_FORMAT_UNDEFINED;
            uint32_t offset = 0u;
            AttributeDescription(VkFormat fmt, uint32_t ofs) : format(fmt), offset(ofs) {}
        };

        static uint32_t ComputeVertexStride(const std::vector<AttributeDescription>& vtxAttrs);

        struct Config
        {
            uint32_t vertexStride = 0u; 
            VkPrimitiveTopology primitiveTopology;
            VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            // If VK_SHARING_MODE_CONCURRENT then queueFamilyIndices is required, which
            // is list of queue families that will access this buffer.
            std::vector<uint32_t> queueFamilyIndices;
            std::vector<AttributeDescription> vertexAttrDescriptions;

            Config() = default;

            Config(
                uint32_t vtxStride,
                VkPrimitiveTopology primTopo)
                : vertexStride(vtxStride)
                , primitiveTopology(primTopo)
            {
            }

            Config(
                VkPrimitiveTopology primTopo,
                const std::vector<AttributeDescription>& vtxAttrs)
                : vertexStride(ComputeVertexStride(vtxAttrs))
                , primitiveTopology(primTopo)
                , vertexAttrDescriptions(vtxAttrs)
            {
            }
        };

        using VertexData = void;

        VertexBuffer(
            Context& context,
            CommandBufferFactory& commandBufferFactory,
            CommandQueue& commandQueue,
            const Config& config,
            const VertexData* pVertexData,
            size_t vertexDataSizeBytes);

        ~VertexBuffer() {
            destroy();
        }

        const Config& getConfig() const { return m_config; }

        VkBuffer getHandle() { return m_buffer.handle; }

    private:
        void destroy();

        Context& m_context;

        Config m_config;

        MemoryAllocator::Buffer m_buffer;
    };

    struct VertexXyzRgbUv
    {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;
        bool operator==(const VertexXyzRgbUv& other) const
        {
            return pos == other.pos && color == other.color && texCoord == other.texCoord;
        }

        static const VertexBuffer::Config& GetConfig();
    };

    struct VertexXyzRgbUvN
    {
        glm::vec3 pos;
        glm::vec3 color;
        glm::vec2 texCoord;
        glm::vec3 normal;
        bool operator==(const VertexXyzRgbUvN& other) const
        {
            return pos == other.pos
                && color == other.color
                && texCoord == other.texCoord
                && normal == other.normal;
        }

        static const VertexBuffer::Config& GetConfig();
    };
}
