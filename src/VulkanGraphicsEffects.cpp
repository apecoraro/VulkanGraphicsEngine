#include "VulkanGraphicsEffects.h"

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

    static ShaderProgramsTable s_vertexShadersLibrary;

    static Program& GetOrLoadVertexShader(
        Context& context,
        const std::string& vertexShaderPath,
        const std::string& vertexShaderEntryPointFunc)
    {
        return GetOrLoadShaderProgram(
            s_vertexShadersLibrary,
            context,
            vertexShaderPath,
            Program::Type::Vertex,
            vertexShaderEntryPointFunc);
    }

    static ShaderProgramsTable s_fragmentShadersLibrary;

    static Program& GetOrLoadFragmentShader(
        Context& context,
        const std::string& fragmentShaderPath,
        const std::string& fragmentShaderEntryPointFunc)
    {
        return GetOrLoadShaderProgram(
            s_fragmentShadersLibrary,
            context,
            fragmentShaderPath,
            Program::Type::Fragment,
            fragmentShaderEntryPointFunc);
    }

    using MeshEffectsLibrary = std::map<MeshEffectId, std::unique_ptr<MeshEffect>>;
    static MeshEffectsLibrary s_meshEffectsLibrary;

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

    MeshEffect& EffectsLibrary::GetOrLoadEffect(
        Context& context,
        const MeshEffectDesc& meshEffectDesc)
    {
        Program& vertexShader =
            GetOrLoadVertexShader(
                context,
                meshEffectDesc.vertexShaderPath,
                meshEffectDesc.vertexShaderEntryPointFunc);

        Program& fragmentShader =
            GetOrLoadFragmentShader(
                context,
                meshEffectDesc.fragmentShaderPath,
                meshEffectDesc.fragmentShaderEntryPointFunc);

        const auto& effectId = MeshEffectId(&vertexShader, &fragmentShader);
        MeshEffectsLibrary::iterator findMeshEffectItr = s_meshEffectsLibrary.find(effectId);
        if (findMeshEffectItr != s_meshEffectsLibrary.end()) {
            std::unique_ptr<MeshEffect>& spMeshEffect = findMeshEffectItr->second;
            return *spMeshEffect.get();
        }

        std::unique_ptr<MeshEffect>& spMeshEffect = s_meshEffectsLibrary[effectId];

        uint32_t bindingIndex = 0;
        vgfx::DescriptorSetLayout::DescriptorBindings vertShaderBindings =
            GetVertShaderDescriptorBindings(meshEffectDesc.vertexShaderPath, meshEffectDesc.vertexShaderEntryPointFunc);

        vgfx::DescriptorSetLayout::DescriptorBindings fragShaderBindings =
            GetFragShaderDescriptorBindings(meshEffectDesc.fragmentShaderPath, meshEffectDesc.fragmentShaderEntryPointFunc);

        std::vector<std::unique_ptr<DescriptorSetLayout>> descriptorSetLayouts = {
            std::make_unique<DescriptorSetLayout>(context, vertShaderBindings),
            std::make_unique<DescriptorSetLayout>(context, fragShaderBindings)
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

        std::vector<vgfx::MeshEffect::ImageType> imageTypes = { vgfx::MeshEffect::ImageType::Diffuse };

        spMeshEffect =
            std::make_unique<MeshEffect>(
                vertexShader,
                meshEffectDesc.vertexShaderInputs,
                fragmentShader,
                pushConstantRanges,
                std::move(descriptorSetLayouts),
                meshEffectDesc.imageTypes);

        return *spMeshEffect.get();
    }

    void EffectsLibrary::Optimize()
    {
        // Could probably also destroy descriptor set layouts here too.
        for (auto& itr : s_vertexShadersLibrary) {
            itr.second->destroyShaderModule();
        }

        for (auto& itr : s_fragmentShadersLibrary) {
            itr.second->destroyShaderModule();
        }
    }

    void EffectsLibrary::UnloadAll()
    {
        s_vertexShadersLibrary.clear();
        s_fragmentShadersLibrary.clear();
        s_meshEffectsLibrary.clear();
    }
}