#ifndef VGFX_DRAWABLE_H
#define VGFX_DRAWABLE_H

#include "VulkanGraphicsContext.h"

#include "VulkanGraphicsIndexBuffer.h"
#include "VulkanGraphicsMaterials.h"
#include "VulkanGraphicsVertexBuffer.h"

#include <cstdint>
#include <map>
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
            Material& material,
            const std::map<Material::ImageType, const Image*>& images)
            : m_spVertexBuffer(std::move(spVertexBuffer))
            , m_spIndexBuffer(std::move(spIndexBuffer))
            , m_material(material)
            , m_images(images)
        {
        }

        void addDescriptorSetBuffer(const DescriptorSetBuffer& descriptorSetBuffer)
        {
            // Build the array for VkDescriptorSets for each swap chain image.
            if (m_descriptorSets.size() < descriptorSetBuffer.getCopyCount()) {
                size_t addedAmount = descriptorSetBuffer.getCopyCount() - m_descriptorSets.size();
                // Copy existing contents into new slots
                for (size_t setArrayIndex = 0; setArrayIndex < addedAmount; ++setArrayIndex) {
                    m_descriptorSets.push_back(m_descriptorSets[setArrayIndex]);
                }
            }
            // Unpack the copies from this new descriptor set into the end of each set.
            for (size_t descSetArrayIndex = 0; descSetArrayIndex < m_descriptorSets.size(); ++descSetArrayIndex) {
                size_t bindingIndex = descSetArrayIndex % descriptorSetBuffer.getCopyCount();

                m_descriptorSets[descSetArrayIndex].push_back(descriptorSetBuffer.getDescriptorSet(bindingIndex).getHandle());
            }
        }

        void recordDrawCommands(
            size_t swapChainIndex,
            VkPipelineLayout pipelineLayout,
            VkCommandBuffer commandBuffer);

        const VertexBuffer& getVertexBuffer() const { return *m_spVertexBuffer.get(); }
        VertexBuffer& getVertexBuffer() { return *m_spVertexBuffer.get(); }

        const IndexBuffer& getIndexBuffer() const { return *m_spIndexBuffer.get(); }
        IndexBuffer& getIndexBuffer() { return *m_spIndexBuffer.get(); }

        const Material& getMaterial() const { return m_material; }
        Material& getMaterial() { return m_material; }
 
        size_t getImageCount() const { return m_images.size(); }

        bool getImage(Material::ImageType imageType, const Image** ppImage) const
        {
            const auto& findIt = m_images.find(imageType);
            if (findIt == m_images.end()) {
                return false;
            }
            *ppImage = findIt->second;
        }

        void setImage(Material::ImageType imageType, const Image* pImage)
        {
            m_images[imageType] = pImage;
        }

    private:
        std::unique_ptr<VertexBuffer> m_spVertexBuffer;				// GPU versions
        std::unique_ptr<IndexBuffer> m_spIndexBuffer;
        Material& m_material;
        std::map<Material::ImageType, const Image*> m_images;
        using SwapChainIndex = uint32_t;
        std::vector<std::vector<VkDescriptorSet>> m_descriptorSets;
    };
}
#endif
