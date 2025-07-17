#pragma once

#include "VulkanGraphicsContext.h"
#include "VulkanGraphicsDescriptors.h"
#include "VulkanGraphicsProgram.h"
#include "VulkanGraphicsVertexBuffer.h"

#include <vulkan/vulkan.h>

#include <cassert>
#include <stdexcept>
#include <vector>

namespace vgfx
{
    class Pipeline; // Forward declaration

    using MeshEffectId = std::pair<const Program*, const Program*>;
    class Effect
    {
    public:
        enum class ProgramType
        {
            Vertex,
            Fragment,
            Compute
        };

        virtual const Program& getProgram(ProgramType type) const = 0;

        const Pipeline& getPipeline() const { return *m_pPipeline; }
    private:
        mutable const Pipeline* m_pPipeline = nullptr;
        friend class Pipeline;
        // This function called by the Pipeline that gets created from this Effect.
        void setPipeline(const Pipeline& pipeline) const { m_pPipeline = &pipeline; }
    };
    // Encapsulates a vertex and fragment shader; the DescriptorSetLayouts should correspond
    // to the uniform inputs to the shaders; the imageTypes should correspond to the image
    // inputs (these are used for loading images from disk).
    class MeshEffect : public Effect
    {
    public:
        enum class ImageType
        {
            Diffuse,
        };
        
        MeshEffect(
            const Program& vertexShader,
            const std::vector<VertexBuffer::AttributeDescription>& vtxShaderInputs,
            const Program& fragmentShader,
            const std::vector<VkPushConstantRange>& pushConstantRanges,
            DescriptorSetLayouts&& descriptorSetLayouts,
            const std::vector<ImageType>& imageTypes)
            : m_meshEffectId(MeshEffectId(&vertexShader, &fragmentShader))
            , m_vertexShader(vertexShader)
            , m_vertexShaderInputs(vtxShaderInputs)
            , m_fragmentShader(fragmentShader)
            , m_pushConstantRanges(pushConstantRanges)
            , m_descriptorSetLayouts(std::move(descriptorSetLayouts))
            , m_imageTypes(imageTypes)
        {
        }

        ~MeshEffect() = default;

        const MeshEffectId& getId() const { return m_meshEffectId; }
        const Program& getProgram(ProgramType type) const override {
            if (type == Effect::ProgramType::Vertex) {
                return getVertexShader();
            }
            else if (type == Effect::ProgramType::Fragment) {
                return getFragmentShader();
            }
            else {
                throw std::runtime_error("Unsupported program type.");
            }
        }
        const Program& getVertexShader() const { return m_vertexShader; }
        const Program& getFragmentShader() const { return m_fragmentShader; }

        const std::vector<VertexBuffer::AttributeDescription>& getVertexShaderInputs() const { return m_vertexShaderInputs; }
        const std::vector<VkPushConstantRange>& getPushConstantRanges() const { return m_pushConstantRanges; }
        const DescriptorSetLayouts& getDescriptorSetLayouts() const { return m_descriptorSetLayouts; }
        const std::vector<ImageType>& getImageTypes() const { return m_imageTypes; }

    private:
        MeshEffectId m_meshEffectId;
        const Program& m_vertexShader;
        std::vector<VertexBuffer::AttributeDescription> m_vertexShaderInputs;
        const Program& m_fragmentShader;
        const std::vector<VkPushConstantRange> m_pushConstantRanges;
        DescriptorSetLayouts m_descriptorSetLayouts;
        std::vector<ImageType> m_imageTypes;
    };

    class ComputeEffect : public Effect
    {

    };

    namespace EffectsLibrary
    { 
        struct MeshEffectDesc
        {
            std::string vertexShaderPath;
            std::string vertexShaderEntryPointFunc;
            std::vector<VertexBuffer::AttributeDescription> vertexShaderInputs;
            std::string fragmentShaderPath;
            std::string fragmentShaderEntryPointFunc;
            std::vector<MeshEffect::ImageType> imageTypes;

            MeshEffectDesc(
                const std::string& vtxShaderPath,
                const std::string& vtxShaderEntryPointFunc,
                const std::vector<VertexBuffer::AttributeDescription>& vtxShaderInputs,
                const std::string& fragShaderPath,
                const std::string& fragShaderEntryPointFunc,
                const std::vector<MeshEffect::ImageType>& imgTypes)
                : vertexShaderPath(vtxShaderPath)
                , vertexShaderEntryPointFunc(vtxShaderEntryPointFunc)
                , vertexShaderInputs(vtxShaderInputs)
                , fragmentShaderPath(fragShaderPath)
                , fragmentShaderEntryPointFunc(fragShaderEntryPointFunc)
                , imageTypes(imgTypes)
            {
            }
        };

        MeshEffect& GetOrLoadEffect(
            Context& context,
            const MeshEffectDesc& effectDesc);

        //ComputeEffect& GetOrLoadEffect(); // TODO
            //Context& context,
            //const ComputeEffectDesc& effectDesc);

        // Releases the VkShaderModule associated with each of the currently
        // loaded vertex and fragment shaders Programs, which free's up that memory,
        // but also means that the shader cannot be used to create new Pipelines.
        // Only do this after all Pipelines have been created. ALso free up DescriptorSetLayouts
        void Optimize();

        void UnloadAll();
    }
}

