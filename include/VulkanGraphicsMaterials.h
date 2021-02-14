#ifndef QUANTUM_GFX_MATERIALS_H
#define QUANTUM_GFX_MATERIALS_H

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsProgram.h"

#include <vector>

#include <vulkan/vulkan.h>

namespace vgfx
{
    //  Encapsulates a vertex and fragment shader;
    class Material
    {
    public:
        Material(
            const Program& vertexShader,
            const Program& fragmentShader)
            : m_vertexShader(vertexShader)
            , m_fragmentShader(fragmentShader)
        {
        }

        ~Material() = default;

        void configureDescriptorSets(
            std::vector<std::unique_ptr<DescriptorSetBuffer>>&& descriptorSets)
        {
            m_descriptorSets = std::move(descriptorSets);

            m_descriptorSetLayouts.reserve(m_descriptorSets.size());
            for (const auto& spDescSet : m_descriptorSets) {
                m_descriptorSetLayouts.push_back(spDescSet->getLayout());
            }
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
        }

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
        const Program& m_vertexShader;
        const Program& m_fragmentShader;
        std::vector<std::unique_ptr<DescriptorSetBuffer>> m_descriptorSets;
        std::vector<std::vector<VkDescriptorSet>> m_descriptorSetBindings;
        std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
    };

    namespace MaterialsDatabase
    {
        struct MaterialInfo
        {
            std::string vertexShaderPath;
            std::string vertexShaderEntryPointFunc;
            std::string fragmentShaderPath;
            std::string fragmentShaderEntryPointFunc;

            MaterialInfo(
                std::string vtxShaderPath,
                std::string vtxShaderEntryPointFunc,
                std::string fragShaderPath,
                std::string fragShaderEntryPointFunc)
                : vertexShaderPath(vtxShaderPath)
                , vertexShaderEntryPointFunc(vtxShaderEntryPointFunc)
                , fragmentShaderPath(fragShaderPath)
                , fragmentShaderEntryPointFunc(fragShaderEntryPointFunc)
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
