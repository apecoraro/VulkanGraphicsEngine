#include "VulkanGraphicsPbrDrawable.h"

using namespace vgfx;

std::unique_ptr<PbrDrawable> vgfx::PbrDrawable::CreatePbrDrawable(Context& context, std::unique_ptr<VertexBuffer> spVertexBuffer, std::unique_ptr<IndexBuffer> spIndexBuffer, DescriptorPool& descriptorPool)
{
    return std::unique_ptr<PbrDrawable>();
}
