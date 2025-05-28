#include "VulkanGraphicsMaterials.h"

#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <map>

namespace vgfx
{
    using ShaderProgramsTable = std::map<std::string, std::unique_ptr<Program>>;

    static Program& GetOrLoadShaderProgram(
        ShaderProgramsTable& shaderProgramsTable,
        Context& context,
        const std::string& shader,
        Program::Type shaderType,
        const std::string& entryPointFunc)
    {
        std::string shaderPath = context.getAppConfig().dataDirectoryPath + "/" + shader;

        ShaderProgramsTable::iterator findShaderItr = shaderProgramsTable.find(shaderPath);
        if (findShaderItr != shaderProgramsTable.end()) {
            std::unique_ptr<Program>& spShader = findShaderItr->second;
            return *spShader.get();
        }

        shaderProgramsTable[shaderPath] = Program::CreateFromFile(shaderPath, context, shaderType, entryPointFunc);

        return *shaderProgramsTable[shaderPath];
    }

    static ShaderProgramsTable s_vertexShadersTable;

    static Program& GetOrLoadVertexShader(
        Context& context,
        const std::string& vertexShaderPath,
        const std::string& vertexShaderEntryPointFunc)
    {
        return GetOrLoadShaderProgram(
            s_vertexShadersTable,
            context,
            vertexShaderPath,
            Program::Type::Vertex,
            vertexShaderEntryPointFunc);
    }

    static ShaderProgramsTable s_fragmentShadersTable;

    static Program& GetOrLoadFragmentShader(
        Context& context,
        const std::string& fragmentShaderPath,
        const std::string& fragmentShaderEntryPointFunc)
    {
        return GetOrLoadShaderProgram(
            s_fragmentShadersTable,
            context,
            fragmentShaderPath,
            Program::Type::Fragment,
            fragmentShaderEntryPointFunc);
    }

    using MaterialsTable = std::map<MaterialId, std::unique_ptr<Material>>;
    static MaterialsTable s_materialsTable;

    vgfx::DescriptorSetLayout::DescriptorBindings GetVertShaderDescriptorBindings(
        const std::string&,// shaderPath
        const std::string&)// shaderEntryFunc
        // TODO at some point this should be tied to the specific shader
    {
        vgfx::DescriptorSetLayout::DescriptorBindings vertShaderBindings;
        vertShaderBindings[0] =
            vgfx::DescriptorSetLayout::DescriptorBinding(
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT);

        return vertShaderBindings;
    }

    vgfx::DescriptorSetLayout::DescriptorBindings GetFragShaderDescriptorBindings(
        const std::string&,// shaderPath
        const std::string&)// shaderEntryFunc
        // TODO at some point this should be tied to the specific shader
    {
        vgfx::DescriptorSetLayout::DescriptorBindings fragShaderBindings;
        fragShaderBindings[0] =
            vgfx::DescriptorSetLayout::DescriptorBinding(
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                VK_SHADER_STAGE_FRAGMENT_BIT);

        fragShaderBindings[1] =
            vgfx::DescriptorSetLayout::DescriptorBinding(
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_FRAGMENT_BIT);

        return fragShaderBindings;
    }

    Material& MaterialsLibrary::GetOrLoadMaterial(
        Context& context,
        const MaterialInfo& materialInfo)
    {
        Program& vertexShader =
            GetOrLoadVertexShader(
                context,
                materialInfo.vertexShaderPath,
                materialInfo.vertexShaderEntryPointFunc);

        Program& fragmentShader =
            GetOrLoadFragmentShader(
                context,
                materialInfo.fragmentShaderPath,
                materialInfo.fragmentShaderEntryPointFunc);

        const auto& materialId = MaterialId(&vertexShader, &fragmentShader);
        MaterialsTable::iterator findMaterialItr = s_materialsTable.find(materialId);
        if (findMaterialItr != s_materialsTable.end()) {
            std::unique_ptr<Material>& spMaterial = findMaterialItr->second;
            return *spMaterial.get();
        }

        std::unique_ptr<Material>& spMaterial = s_materialsTable[materialId];

        uint32_t bindingIndex = 0;
        vgfx::DescriptorSetLayout::DescriptorBindings vertShaderBindings =
            GetVertShaderDescriptorBindings(materialInfo.vertexShaderPath, materialInfo.vertexShaderEntryPointFunc);

        vgfx::DescriptorSetLayout::DescriptorBindings fragShaderBindings =
            GetFragShaderDescriptorBindings(materialInfo.fragmentShaderPath, materialInfo.fragmentShaderEntryPointFunc);

        std::vector<std::unique_ptr<vgfx::DescriptorSetLayout>> descriptorSetLayouts = {
            std::make_unique<vgfx::DescriptorSetLayout>(vertShaderBindings),
            std::make_unique<vgfx::DescriptorSetLayout>(fragShaderBindings)
        };

        struct ModelParams {
            glm::mat4 world = glm::identity<glm::mat4>();
            glm::mat4 view = glm::identity<glm::mat4>();
        };

        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(ModelParams);
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        std::vector<VkPushConstantRange> pushConstantRanges = { pushConstantRange };

        std::vector<vgfx::Material::ImageType> imageTypes = { vgfx::Material::ImageType::Diffuse };

        spMaterial =
            std::make_unique<Material>(
                vertexShader,
                materialInfo.vertexShaderInputs,
                fragmentShader,
                pushConstantRanges,
                std::move(descriptorSetLayouts),
                materialInfo.imageTypes);

        return *spMaterial.get();
    }

    void MaterialsLibrary::Optimize()
    {
        // Could probably also destroy descriptor set layouts here too.
        for (auto& itr : s_vertexShadersTable) {
            itr.second->destroyShaderModule();
        }

        for (auto& itr : s_fragmentShadersTable) {
            itr.second->destroyShaderModule();
        }
    }

    void MaterialsLibrary::UnloadAll()
    {
        s_vertexShadersTable.clear();
        s_fragmentShadersTable.clear();
        s_materialsTable.clear();
    }
}