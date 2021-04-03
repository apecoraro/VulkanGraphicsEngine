#ifndef VGFX_MATERIALS_H
#define VGFX_MATERIALS_H

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsProgram.h"

#include <cassert>
#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    using MaterialId = std::pair<Program*, Program*>;
    //  Encapsulates a vertex and fragment shader;
    class Material
    {
    public:
        Material(
            MaterialId materialId,
            const Program& vertexShader,
            const Program& fragmentShader)
            : m_vertexShader(vertexShader)
            , m_fragmentShader(fragmentShader)
        {
        }

        ~Material() = default;

        void attachDescriptorSets(
            std::vector<std::unique_ptr<DescriptorSetBuffer>>&& descriptorSets)
        {
            m_descriptorSets = std::move(descriptorSets);

            m_descriptorSetLayouts.reserve(m_descriptorSets.size());
            for (const auto& spDescSet : m_descriptorSets) {
                m_descriptorSetLayouts.push_back(spDescSet->getLayout());
            }

            m_areDescriptorSetsInitialized = false;
        }

        bool areDescriptorSetsInitialized() const
        {
            return m_areDescriptorSetsInitialized;
        }

        void initDescriptors(DescriptorPool& pool)
        {
            for (auto& spDescSet : m_descriptorSets) {
                spDescSet->init(pool);
                const auto& descSetCopies = spDescSet->getDescriptorSetCopies();
                for (size_t i = 0; i < descSetCopies.size(); ++i) {
                    if (i == m_descriptorSetBindings.size()) {
                        m_descriptorSetBindings.resize(m_descriptorSetBindings.size() + 1);
                    }
                    m_descriptorSetBindings[i].push_back(descSetCopies[i]);
                }
            }

            m_areDescriptorSetsInitialized = true;
        }

        const MaterialId& getId() const { return m_materialId; }
        const Program& getVertexShader() const { return m_vertexShader; }
        const Program& getFragmentShader() const { return m_fragmentShader; }
        size_t getDescriptorSetCount() const { return m_descriptorSets.size(); }
        const DescriptorSetBuffer& getDescriptorSet(size_t index) const { return *(m_descriptorSets[index].get()); }
        const std::vector<VkDescriptorSetLayout>& getDescriptorSetLayouts() const { return m_descriptorSetLayouts; }

        size_t getDescriptorSetBindingCount() const {
            return m_descriptorSetBindings.size();
        }

        const std::vector<VkDescriptorSet>& getDescriptorSetBinding(size_t index) const {
            return m_descriptorSetBindings[index];
        }
    private:
        MaterialId m_materialId;
        const Program& m_vertexShader;
        const Program& m_fragmentShader;
        bool m_areDescriptorSetsInitialized = false;
        std::vector<std::unique_ptr<DescriptorSetBuffer>> m_descriptorSets;
        std::vector<std::vector<VkDescriptorSet>> m_descriptorSetBindings;
        std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
    };

    namespace MaterialsDatabase
    {
        struct DescriptorBinding
        {
            const VkDescriptorType descriptorType;
            const VkShaderStageFlags shaderStageFlags;
            DescriptorBinding(VkDescriptorType descType, VkShaderStageFlags shStFlags)
                : descriptorType(descType)
                , shaderStageFlags(shStFlags)
            {
            }
        };

        struct UniformBufferDescriptorBinding : public DescriptorBinding
        {
            UniformBuffer& uniformBuffer;
            UniformBufferDescriptorBinding(
                VkShaderStageFlags shaderStageFlags,
                UniformBuffer& buffer)
                : DescriptorBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, shaderStageFlags)
                , uniformBuffer(buffer)
            {
            }
        };

        enum class ImageType
        {
            DIFFUSE,
            CUSTOM
        };

        struct CombinedImageSamplerDescriptorBinding : public DescriptorBinding
        {
            // This type is used to determine which
            // image from the model will be loaded for this CombinedImageSampler.
            const ImageType imageType;
            const std::optional<vgfx::ImageView::Config> imageViewConfig;
            const std::optional<vgfx::Sampler::Config> samplerConfig;
            // If imageType == CUSTOM then this imageSampler is used.
            const std::optional<CombinedImageSampler> imageSampler;

            CombinedImageSamplerDescriptorBinding(
                VkShaderStageFlags shaderStageFlags,
                ImageType imgType,
                const vgfx::ImageView::Config& imgViewCfg,
                const vgfx::Sampler::Config& smplrCfg)
                : DescriptorBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shaderStageFlags)
                , imageType(imgType)
                , imageViewConfig(imgViewCfg)
                , samplerConfig(smplrCfg)
            {
            }

            CombinedImageSamplerDescriptorBinding(
                VkShaderStageFlags shaderStageFlags,
                // Note that the CombinedImageSampler will be eventually owned by the the Material
                // that gets created with this binding.
                CombinedImageSampler& imgSmplr)
                : DescriptorBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, shaderStageFlags)
                , imageType(ImageType::CUSTOM)
                , imageSampler(imgSmplr)
            {
            }
        };

        struct MaterialInfo
        {
            std::vector<VkFormat> vertexShaderInputs; // vertex attribute input types, in location order
            std::string vertexShaderPath;
            std::string vertexShaderEntryPointFunc;
            std::vector<VkFormat> fragmentShaderInputs; // frag shader input types, in location order
            std::string fragmentShaderPath;
            std::string fragmentShaderEntryPointFunc;
            std::vector<DescriptorBinding> descriptorBindings; // Descriptors (uniform buffers, samplers, etc.) in location order

            MaterialInfo(
                const std::vector<VkFormat>& vtxShaderInputs,
                std::string vtxShaderPath,
                std::string vtxShaderEntryPointFunc,
                const std::vector<VkFormat>& fragShaderInputs,
                std::string fragShaderPath,
                std::string fragShaderEntryPointFunc,
                std::vector<DescriptorBinding>&& descBindings)
                : vertexShaderInputs(vtxShaderInputs)
                , vertexShaderPath(vtxShaderPath)
                , vertexShaderEntryPointFunc(vtxShaderEntryPointFunc)
                , fragmentShaderInputs(fragShaderInputs)
                , fragmentShaderPath(fragShaderPath)
                , fragmentShaderEntryPointFunc(fragShaderEntryPointFunc)
                , descriptorBindings(descBindings)
            {
            }
        };

        Material& GetOrLoadMaterial(
            Context& context,
            const MaterialInfo& materialInfo);

        // Releases the VkShaderModule associated with each of the currently
        // loaded vertex and fragment shaders Programs, which free's up that memory,
        // but also means that the shader cannot be used to create new Pipelines.
        // Only do this after all Pipelines have been created.
        void Optimize();

        void UnloadAll();
    }
}

#endif
