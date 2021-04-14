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
    using MaterialId = std::pair<const Program*, const Program*>;
    // Encapsulates a vertex and fragment shader; the DescriptorSetLayouts should correspond
    // to the uniform inputs to the shaders; the imageTypes should correspond to the image
    // inputs (these are used for loading images from disk).
    class Material
    {
    public:
        enum class ImageType
        {
            DIFFUSE,
        };
        using DescriptorSetLayouts = std::vector<std::unique_ptr<DescriptorSetLayout>>;
        Material(
            const Program& vertexShader,
            const Program& fragmentShader,
            DescriptorSetLayouts&& descriptorSetLayouts,
            const std::vector<ImageType>& imageTypes)
            : m_materialId(MaterialId(&vertexShader, &fragmentShader))
            , m_vertexShader(vertexShader)
            , m_fragmentShader(fragmentShader)
            , m_descriptorSetLayouts(std::move(descriptorSetLayouts))
            , m_imageTypes(imageTypes)
        {
        }

        ~Material() = default;

        const MaterialId& getId() const { return m_materialId; }
        const Program& getVertexShader() const { return m_vertexShader; }
        const Program& getFragmentShader() const { return m_fragmentShader; }

        const std::vector<ImageType>& getImageTypes() const { return m_imageTypes; }
        const DescriptorSetLayouts& getDescriptorSetLayouts() const { return m_descriptorSetLayouts; }

    private:
        MaterialId m_materialId;
        const Program& m_vertexShader;
        const Program& m_fragmentShader;
        DescriptorSetLayouts m_descriptorSetLayouts;
        std::vector<ImageType> m_imageTypes;
    };

    namespace MaterialsLibrary
    {
        struct MaterialInfo
        {
            std::vector<VkFormat> vertexShaderInputs; // vertex attribute input types, in location order
            std::string vertexShaderPath;
            std::string vertexShaderEntryPointFunc;
            std::vector<VkFormat> fragmentShaderInputs; // frag shader input types, in location order
            std::string fragmentShaderPath;
            std::string fragmentShaderEntryPointFunc;
            std::vector<DescriptorSetLayout::DescriptorBindings> descriptorSetLayoutBindings;
            std::vector<Material::ImageType> imageTypes;

            MaterialInfo(
                const std::vector<VkFormat>& vtxShaderInputs,
                std::string vtxShaderPath,
                std::string vtxShaderEntryPointFunc,
                const std::vector<VkFormat>& fragShaderInputs,
                std::string fragShaderPath,
                std::string fragShaderEntryPointFunc,
                const std::vector<DescriptorSetLayout::DescriptorBindings>& descSetLayoutBindings,
                const std::vector<Material::ImageType>& imgTypes)
                : vertexShaderInputs(vtxShaderInputs)
                , vertexShaderPath(vtxShaderPath)
                , vertexShaderEntryPointFunc(vtxShaderEntryPointFunc)
                , fragmentShaderInputs(fragShaderInputs)
                , fragmentShaderPath(fragShaderPath)
                , fragmentShaderEntryPointFunc(fragShaderEntryPointFunc)
                , descriptorSetLayoutBindings(descSetLayoutBindings)
                , imageTypes(imgTypes)
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
