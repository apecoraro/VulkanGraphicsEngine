#pragma once

#include "VulkanGraphicsDrawable.h"

namespace vgfx
{
    class PbrDrawable : public Drawable
    {
    public:
        static std::unique_ptr<PbrDrawable> CreatePbrDrawable(
            Context& context,
            std::unique_ptr<VertexBuffer> spVertexBuffer,
            std::unique_ptr<IndexBuffer> spIndexBuffer,
            DescriptorPool& descriptorPool);

    private:
        PbrDrawable(
            Context& context,
            std::unique_ptr<VertexBuffer> spVertexBuffer,
            std::unique_ptr<IndexBuffer> spIndexBuffer,
            const MeshEffect& meshEffect,
            const ImageSamplerMap& imageSamplers)
            : Drawable(context,
                std::move(spVertexBuffer),
                std::move(spIndexBuffer),
                meshEffect,
                imageSamplers)
        {
        }
    };
}

